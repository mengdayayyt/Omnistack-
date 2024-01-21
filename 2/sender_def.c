#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"
#include "rtp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>

// 0:START, 1:END, 2:DATA, 3:ACK
uint32_t Window_size;
int sock;
struct sockaddr_in addr;
socklen_t addr_len;
int Seq_num; //当前该发送的seqnum；
void terminateSender();
int sendMessageOpt(const char *message)
{
}
int SEQ_NUM()
{
    int x = Seq_num;
    Seq_num++;
    return x;
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
void Send(int type, int seq_num) // send start,end,ack;
{
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    rtp_header_t *tmp = (rtp_header_t *)buffer;
    tmp->type = type;
    tmp->length = 0;
    tmp->seq_num = seq_num;
    tmp->checksum = 0;
    tmp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));
    sendto(sock, (void *)buffer, sizeof(rtp_header_t), 0, (struct sockaddr *)&addr, addr_len);
}
void Send_data(int type, int length, int seq_num, const char *data)
{
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    rtp_header_t *tmp = (rtp_header_t *)buffer;
    tmp->type = type;
    tmp->length = length;
    tmp->seq_num = seq_num;
    tmp->checksum = 0;
    memcpy(buffer + sizeof(rtp_header_t), data, length);
    tmp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t) + length);
  //  printf("send %d\n", seq_num);
    sendto(sock, (void *)buffer, sizeof(rtp_header_t) + length, 0, (struct sockaddr *)&addr, addr_len);
}
int Recv(char *buffer) //返回0说明超时
{
    memset(buffer, 0, sizeof(buffer));
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; //超时时间100毫秒
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
//用于建立RTP连接
int initSender(const char *receiver_ip, uint16_t receiver_port, uint32_t window_size)
{
    printf("start\n");
    //建立套接字；
    Window_size = window_size;
    Seq_num = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        printf("sock fail\n");
    addr_len = sizeof(struct sockaddr);
    addr.sin_port = htons(receiver_port);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &addr.sin_addr);

    Send(0, SEQ_NUM()); //发送start
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int len = Recv(buffer);
    if (len == 0 || right_sum(buffer, len) == 0)
    {
        terminateSender();
        return -1;
    } //判断如果丢失或者损坏，直接发送end返回-1;
      /*
      rtp_header_t tmp;
      memcpy(&tmp, buffer, sizeof(rtp_header_t));
      int correct = right_sum(buffer, len);
      if (correct == 0 || tmp.seq_num != 0)
      {
          terminateSender();
          return -1;
      }
      */
    printf("connected\n");
    return 0; // 0表示连接成功建立连接
}

//要发送的数据由DATA类型的报文进行传输。发送方的数据报文的seq_num从0开始每个报文递增。
int sendMessage(const char *message)
{
    printf("send data start  windowsize=%d\n", Window_size);

    Seq_num = 0;
    char **Q; //维护一个滑动窗口
    Q = malloc(sizeof(char *) * Window_size);
    int window_left = 0;   //左窗口
    int window_right = -1; //右窗口
    int window_size = 0;   //窗口大小
    int recent_num = 0;    //最近一个发送的编号
    int fd = open(message, O_RDONLY);
    if (fd == -1)
    {
        //  Send_data(2, strlen(message), SEQ_NUM(), message);
        printf("%s  fd open fail\n", message);
        close(fd);
        return -1; //发送文件失败
    }
    int over = 0; //判断文件是否发送结束
    while (1)
    {
        while (window_size < Window_size && !over)
        {
            // printf("send\n");
            char *data = malloc(sizeof(char) * 2048);
            int len = read(fd, data, 1000);
            //     printf("len =%d left %d right %d size= %d recentnum= %d seq_num %d\n",len,window_left,window_right,window_size,recent_num,Seq_num);
            if (len <= 0) //已读完
            {
                over = 1;
                break;
            }
            data[len] = 0;
            window_right = (window_right + 1) % Window_size; //窗口右移
            window_size++;
            Q[window_right] = data;
            Send_data(2, len, SEQ_NUM(), data);
        }
        if (over && window_size == 0)
            break;
        char buffer[2048];
        int uuu = 0;
        while (1)
        {
            int len = Recv(buffer);
            if (!len) //如果超时的话,重传一遍
            {
                printf("chongchuan\n");
                printf("%d %d \n", window_left, recent_num);
                int i = window_left;
                int num = recent_num;
                for (int j = 0; j < window_size; j++)
                {
                    Send_data(2, strlen(Q[i]), num, Q[i]);
                    i = (i + 1) % Window_size;
                    num++;
                }
                break;
            }
            else if (right_sum(buffer, len))
            {
                rtp_header_t tmp;
                memcpy(&tmp, buffer, sizeof(rtp_header_t));
                while (recent_num < tmp.seq_num)
                {
                    recent_num++;
                    free(Q[window_left]);
                    window_left = (window_left + 1) % Window_size;
                    window_size--;
                }
                // printf("ok window size=%d recent_num=%d  ACK_num=%d \n",window_size,recent_num,tmp.seq_num);
                // printf("count =%d \n",uuu);
                // uuu++;
            }
        }
    }
    close(fd);
    free(Q);
    return 0;
}
//用于断开RTP连接以及关闭UDP socket
void terminateSender()
{
    Send(1, SEQ_NUM()); //发送end
    char buffer[2048];
    int len = Recv(buffer);
    close(sock);
    printf("over&*&(\n");
    return;
}