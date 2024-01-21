#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <map>

#include "util.h"
#include "rtp.h"
#include "receiver_def.h"

uint32_t Window_size;
int sock;
struct sockaddr_in addr;
socklen_t addr_len = sizeof(struct sockaddr_in);
void terminateReceiver();
int right_sum(char *buffer, int len)
{
    rtp_header_t tmp;
    memcpy(&tmp, buffer, sizeof(rtp_header_t));
    uint32_t recv_checksum = tmp.checksum;
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    rtp->checksum = 0;
    uint32_t right_sum = compute_checksum((void *)buffer, len);
    return (right_sum == recv_checksum);
}
void Send(int type, int seq_num) // send ack;
{
    char buffer[12];
    memset(buffer, 0, sizeof(buffer));
    rtp_header_t *tmp = (rtp_header_t *)buffer;
    tmp->type = type;
    tmp->length = 0;
    tmp->seq_num = seq_num;
    tmp->checksum = 0;
    tmp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));
    sendto(sock, (void *)buffer, sizeof(rtp_header_t), 0, (struct sockaddr *)&addr, addr_len);
}
int initReceiver(uint16_t port, uint32_t window_size)
{
    Window_size = window_size;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == 0)
        return -1;
    struct sockaddr_in init_addr;
    memset(&init_addr, 0, sizeof(struct sockaddr_in));
    init_addr.sin_family = AF_INET;
    init_addr.sin_addr.s_addr = INADDR_ANY;
    init_addr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&init_addr, sizeof(struct sockaddr)) < 0)
        return -1;
    printf("listen\n");
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int recv_len = recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&addr, &addr_len);
    if (recv_len <= 0)
        return -1;
    if (!right_sum(buffer, recv_len))
        return -1;
    rtp_header_t tmp;
    memcpy(&tmp, buffer, sizeof(rtp_header_t));
    if (tmp.type != 0)
        return -1;
    Send(3, tmp.seq_num); //对start发送确认报文
                          //  printf("connect!!\n");
    return 0;             //连接成功,
}
// 0:START, 1:END, 2:DATA, 3:ACK

int recvMessage(char *filename)
{
    //  printf("%s\n", filename);
    printf("start recv!\n");
    uint32_t ACKnum = 0;
    char **W; //维护一个滑动窗口
    int *Wlen;
    W = new char *[Window_size];
    memset(W, 0, Window_size * sizeof(char *));
    Wlen = new int[Window_size]; //对应的数据的len长度
    FILE *fd = fopen(filename, "wb");
    char buffer[2048];
    int count2 = 0;
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        struct timeval t_v;
        t_v.tv_sec = 0;
        t_v.tv_usec = 10000000; //超时时间10000毫秒
        fd_set rset;
        FD_ZERO(&rset);                                 //当声明了一个文件描述符集后，必须用FD_ZERO将所有位置零。
        FD_SET(sock, &rset);                            //设置对应的位
        if (!select(sock + 1, &rset, NULL, NULL, &t_v)) // select函数，拥塞等待文件描述符事件的到来；如果超过设定的时间，则不再等待，继续往下执行
        {
            fclose(fd);
            return -1;
            // terminateReceiver();,判断是否超时；
        }
        int recvlen = recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&addr, &addr_len);
        rtp_header_t tmp;
        memcpy(&tmp, buffer, sizeof(rtp_header_t));
        if (!right_sum(buffer, recvlen))
        {
            //  printf("-----------not right sum------------------------\n");
            continue; //不当的数据包直接丢弃
        }
        if (tmp.type == 1) //如果是END
        {
            //  printf("-----------all ok-------------------\n");
            Send(3, tmp.seq_num); //返回ACK；
            fclose(fd);
            return count2;
        }
        if (tmp.seq_num >= ACKnum + Window_size || tmp.seq_num < ACKnum)
        {
            // printf("-----------more!------------------\n");
            continue;
        }
        // printf("tmp.seq_num=%d  ACKnum=%d\n", tmp.seq_num, ACKnum);
        if (W[tmp.seq_num % Window_size])
        {
            // printf("chongfule\n");
            continue;
        }
        //先缓存一波
        char *data = new char[tmp.length + 1];
        memcpy(data, buffer + sizeof(rtp_header_t), tmp.length);
        data[tmp.length] = 0;
        W[tmp.seq_num % Window_size] = data;
        Wlen[tmp.seq_num % Window_size] = tmp.length;
        if (tmp.seq_num > ACKnum)
        {
            Send(3, ACKnum); //发送一个ACK
        }
        if (tmp.seq_num == ACKnum)
        {
            while (W[ACKnum % Window_size])
            {
                count2 = count2 + Wlen[ACKnum % Window_size];
                fwrite(W[ACKnum % Window_size], 1, Wlen[ACKnum % Window_size], fd);
                delete[] W[ACKnum % Window_size];
                W[ACKnum % Window_size] = nullptr;
                // printf("-------------------------------ACKnum----- %d\n", ACKnum);
                ACKnum++;
                //  printf("uiui\n");
            }
            // printf("ui\n");
            Send(3, ACKnum); //发送一个ACK
        }
    }
}
int recvMessageOpt(char *filename)
{
    printf("%s\n", filename);
    printf("start recv!\n");
    uint32_t ACKnum = 0;
    char **W; //维护一个滑动窗口
    int *Wlen;
    W = new char *[Window_size];
    memset(W, 0, Window_size * sizeof(char *));
    Wlen = new int[Window_size]; //对应的数据的len长度
    FILE *fd = fopen(filename, "wb");
    char buffer[2048];
    int count2 = 0;
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        struct timeval t_v;
        t_v.tv_sec = 0;
        t_v.tv_usec = 10000000; //超时时间10000毫秒
        fd_set rset;
        FD_ZERO(&rset);                                 //当声明了一个文件描述符集后，必须用FD_ZERO将所有位置零。
        FD_SET(sock, &rset);                            //设置对应的位
        if (!select(sock + 1, &rset, NULL, NULL, &t_v)) // select函数，拥塞等待文件描述符事件的到来；如果超过设定的时间，则不再等待，继续往下执行
        {
            fclose(fd);
            return -1; // 判断是否超时；
        }
        int recvlen = recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&addr, &addr_len);
        rtp_header_t tmp;
        memcpy(&tmp, buffer, sizeof(rtp_header_t));
        if (!right_sum(buffer, recvlen))
        {
            //  printf("-----------not right sum------------------------\n");
            continue; //不当的数据包直接丢弃
        }
        if (tmp.type == 1) //如果是END
        {
            //  printf("-----------all ok-------------------\n");
            Send(3, tmp.seq_num); //返回ACK；
            fclose(fd);
            return count2;
        }
        if (tmp.seq_num >= ACKnum + Window_size || tmp.seq_num < ACKnum)
        {
            // printf("-----------more!------------------\n");
            continue;
        }
        //  printf("tmp.seq_num=%d  ACKnum=%d\n", tmp.seq_num, ACKnum);
        if (W[tmp.seq_num % Window_size])
        {
            //  printf("chongfule\n");
            continue;
        }
        //先缓存一波
        char *data = new char[tmp.length + 1];
        memcpy(data, buffer + sizeof(rtp_header_t), tmp.length);
        data[tmp.length] = 0;
        W[tmp.seq_num % Window_size] = data;
        Wlen[tmp.seq_num % Window_size] = tmp.length;
        Send(3, tmp.seq_num); //发送一个ACK
        if (tmp.seq_num == ACKnum)
        {
            while (W[ACKnum % Window_size])
            {
                count2 = count2 + Wlen[ACKnum % Window_size];
                fwrite(W[ACKnum % Window_size], 1, Wlen[ACKnum % Window_size], fd);
                //    printf("uiui\n");
                delete[] W[ACKnum % Window_size];
                W[ACKnum % Window_size] = nullptr;
                //    printf("-------------------------------ACKnum----- %d\n", ACKnum);
                ACKnum++;
            }
            // printf("ui\n");
            // Send(3, ACKnum);
        }
    }
}
void terminateReceiver()
{
    printf("I am going to end!\n");
    close(sock);
    return;
}
