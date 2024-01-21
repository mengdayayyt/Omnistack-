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

int Recv(char *buffer) //返回0说明超时
{
    memset(buffer, 0, sizeof(buffer));
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000000; //超时时间10000毫秒
    fd_set rset;
    while (1)
    {
        //当声明了一个文件描述符集后，必须用FD_ZERO将所有位置零。之后将我们所感兴趣的描述符所对应的位置位
        FD_ZERO(&rset);
        FD_SET(sock, &rset);
        // select函数，拥塞等待文件描述符事件的到来；如果超过设定的时间，则不再等待，继续往下执行
        int result = select(sock + 1, &rset, NULL, NULL, &tv);
        //  printf("result=%d\n",result);
        if (!result) //说明超时了
            return 0;
        else if (FD_ISSET(sock, &rset))
            return recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&addr, &addr_len);
    }
}
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
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr)) < 0)
        return -1;
    printf("listen\n");
    char buffer[2048];
    while (1)
    {
        int recv_len = recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&addr, &addr_len);
        if (recv_len > 0)
        {
            if (right_sum(buffer, recv_len))
            {
                rtp_header_t tmp;
                memcpy(&tmp, buffer, sizeof(rtp_header_t));
                Send(3, tmp.seq_num); //对start发送确认报文
                printf("connect!!\n");
                return 0; //连接成功,
            }
            else
                return -1; //当START报文的checksum错误，receiver进程直接退出即可
        }
    }
    return 0;
}
//设置一个超时时间(建议为10s)当超过这个时间没有收到任何报文的时候，即认为该连接已经关闭
/*
char **Q;
void init()
{
    Q = malloc(sizeof(char *) * Window_size);
    map=malloc(sizeof(int)*Window_size);
    return;
}
void Free()
{
    free(Q);
    return;
}
*/
// 0:START, 1:END, 2:DATA, 3:ACK
uint32_t ACKnum;
int recvMessage(char *filename)
{
    printf("start recv!\n");
    std::map<int, char *> Q;
    std::map<int, int> Qlen;
    FILE* fd = fopen(filename,"wb");
    ACKnum = 0;
    char buffer[2048];
    int count1 = 0;
    int count2 = 0;
    while (1)
    {
        int recvlen = Recv(buffer);
      //  printf("recvlen %d %d %d \n", recvlen, count1, count2);
        if (recvlen <= 0) //超过10s没收到消息
        {
            fclose(fd);
            return -1;
            // terminateReceiver();
        }
        if (!right_sum(buffer, recvlen))
        {
          //  printf("-----------not right sum------------------------\n");
            continue; //不当的数据包直接丢弃
        }
        rtp_header_t tmp;
        memcpy(&tmp, buffer, sizeof(rtp_header_t));
        if (tmp.type == 1)
        {
          //  printf("-----------all ok-------------------\n");
            Send(3, tmp.seq_num);
            fclose(fd);
            return count2;
        }
        if (tmp.seq_num > ACKnum + Window_size)
        {
           // printf("-----------more!------------------\n");
            continue;
        }
        if(Q[tmp.seq_num])
        {
      //  printf("chongfule\n");
        continue;
        }
        //先缓存一波
        char *data = new char[tmp.length + 1];
        memcpy(data, buffer + sizeof(rtp_header_t), tmp.length);
        
        data[tmp.length] = 0;
        Q[tmp.seq_num] = data;
        Qlen[tmp.seq_num]=tmp.length;
        if (tmp.seq_num > ACKnum)
        {
            Send(3, ACKnum); //发送一个ACK
        }
        if (tmp.seq_num == ACKnum)
        {
            while (Q[ACKnum])
            {
                count2 = count2 + strlen(Q[ACKnum]);
                //write(fd, (void *)Q[ACKnum], strlen(Q[ACKnum]));
                fwrite(Q[ACKnum],1,Qlen[ACKnum],fd);
                Q.erase(ACKnum);
                Qlen.erase(ACKnum);
               // printf("-------------------------------ACKnum----- %d\n",ACKnum);
                ACKnum++;
            }
            Send(3, ACKnum); //发送一个ACK
        }
    }
}
int recvMessageOpt(char *filename)
{
}
void terminateReceiver()
{
    printf("I am going to end!\n");
   close(sock);
    return;
}