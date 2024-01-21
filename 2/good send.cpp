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
#include "sender_def.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>

// 0:START, 1:END, 2:DATA, 3:ACK
uint32_t Window_size_;
int sock_;
struct sockaddr_in addr_;
socklen_t addr_len_;
int Seq_num;
void terminateSender();
int SEQ_NUM()
{
    int x = Seq_num;
    Seq_num++;
    return x;
}
int right_sum_(char *buffer, int len)
{
    rtp_header_t tmp;
    memcpy(&tmp, buffer, sizeof(rtp_header_t));
    uint32_t recv_checksum = tmp.checksum;
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    rtp->checksum = 0;
    uint32_t right_sum_ = compute_checksum((void *)buffer, len);
    return (right_sum_ == recv_checksum);
}
void Send_(int type, int seq_num) // send start,end,ack;
{
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    rtp_header_t *tmp = (rtp_header_t *)buffer;
    tmp->type = type;
    tmp->length = 0;
    tmp->seq_num = seq_num;
    tmp->checksum = 0;
    tmp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));
    sendto(sock_, (void *)buffer, sizeof(rtp_header_t), 0, (struct sockaddr *)&addr_, addr_len_);
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
    sendto(sock_, (void *)buffer, sizeof(rtp_header_t) + length, 0, (struct sockaddr *)&addr_, addr_len_);
}
int Recv_(char *buffer) //返回0说明超时
{
    memset(buffer, 0, sizeof(buffer));
    fd_set rset;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; //超时时间100毫秒
    while (1)
    {
        //当声明了一个文件描述符集后，必须用FD_ZERO将所有位置零。之后将我们所感兴趣的描述符所对应的位置位
        FD_ZERO(&rset);
        FD_SET(sock_, &rset);
        // select函数，拥塞等待文件描述符事件的到来；如果超过设定的时间，则不再等待，继续往下执行
        int result = select(sock_ + 1, &rset, NULL, NULL, &tv);
        //  printf("result=%d\n",result);
        if (!result) //说明超时了
            return 0;
        else if (FD_ISSET(sock_, &rset))
        {
            int len = recvfrom(sock_, buffer, 2048, 0, (struct sockaddr *)&addr_, &addr_len_);
            if (right_sum_(buffer, len))
            return len;
            else
            return -1;
        }
    }
}
//用于建立RTP连接
int initSender(const char *receiver_ip, uint16_t receiver_port, uint32_t window_size)
{
    printf("start\n");
    //建立套接字；
    Window_size_ = window_size;
    Seq_num = 0;

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0)
        printf("sock_ fail\n");
    addr_len_ = sizeof(struct sockaddr);
    addr_.sin_port = htons(receiver_port);
    addr_.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &addr_.sin_addr);

    Send_(0, SEQ_NUM()); //发送start
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int len = Recv_(buffer);
    if (len == 0 || len ==-1)
    {
        terminateSender();
        return -1;
    }
    rtp_header_t tmp;
    memcpy(&tmp, buffer, sizeof(rtp_header_t));
    if (tmp.type != 3 || tmp.seq_num != 0)
    {
        terminateSender();
        return -1;
    }
    printf("connected\n");
    return 0; // 0表示连接成功建立连接
}

//要发送的数据由DATA类型的报文进行传输。发送方的数据报文的seq_num从0开始每个报文递增。
int sendMessage(const char *message)
{
    printf("send data start  windowsize=%d\n", Window_size_);

    FILE *fd = fopen(message, "rb");
    int len;
    char buffer[2048];
    Seq_num = 0; //当前该发送的seqnum；
    char **Q;    //维护一个滑动窗口
    int *Qlen;
    // Q = malloc(sizeof(char *) * Window_size_);
    Q = new char *[Window_size_];
    Qlen = new int[Window_size_];
    int window_left = 0;   //左窗口
    int window_right = -1; //右窗口
    int window_size = 0;   //窗口大小
    int recent_num = 0;    //最近一个发送的编号
    // int fd = open(message, O_RDONLY);
    if (fd ==NULL)
    {
        printf("%s  fd open fail\n", message);
        fclose(fd);
        return -1; //发送文件失败
    }
    int over = 0; //判断文件是否发送结束
    while (!over||window_size)
    {
        while (window_size < Window_size_ && !over)
        {
            // printf("send\n");
            char *data = new char[2048];
            int len = fread(data, 1, 1461, fd);
            // printf("len =%d left %d right %d size= %d recentnum= %d seq_num %d\n",len,window_left,window_right,window_size,recent_num,Seq_num);
            if (len <= 0) //已读完
            {
                // printf("len over!\n");
                over = 1;
                break;
            }
            data[len] = 0;
            window_right = (window_right + 1) % Window_size_; //窗口右移
            window_size++;
            Q[window_right] = data;
            Qlen[window_right] = len;
            Send_data(2, len, SEQ_NUM(), data);
        }
        while (window_size)
        {
            len = Recv_(buffer);
            if(len==-1)
            continue;
            else if (len==0) //如果超时的话,重传一遍
            {
                //   printf("chongchuan  recent_num=%d  %d\n");
                // printf("%d %d \n", window_left, recent_num);
                int i = window_left;
                int num = recent_num;
                for (int j = 0; j < window_size; j++)
                {
                    Send_data(2, Qlen[i], num, Q[i]);
                    i = (i + 1) % Window_size_;
                    num++;
                }
                break;
            }
            else if (len>0)
            {
                rtp_header_t tmp;
                memcpy(&tmp, buffer, sizeof(rtp_header_t));
                while (recent_num < tmp.seq_num)
                {
                    recent_num++;
                    free(Q[window_left]);
                    window_left = (window_left + 1) % Window_size_;
                    window_size--;
                }
                // printf("ok window size=%d recent_num=%d  ACK_num=%d \n",window_size,recent_num,tmp.seq_num);
                // printf("count =%d \n",uuu);
                // uuu++;
            }
        }
       
    }
    fclose(fd);
    // free(Q);
    delete[] Q;
    delete[] Qlen;
    return 0;
}
//用于断开RTP连接以及关闭UDP socket
int sendMessageOpt(const char *message)
{
    printf("send data start  windowsize=%d\n", Window_size_);

    Seq_num = 0; //当前该发送的seqnum；
    char **Q;    //维护一个滑动窗口
    int *Qlen;
    int *okwindow;
    // Q = malloc(sizeof(char *) * Window_size_);
    Q = new char *[Window_size_];
    Qlen = new int[Window_size_];
    okwindow = new int[Window_size_];
    memset(okwindow, 0, Window_size_ * sizeof(int));
    int window_left = 0;   //左窗口
    int window_right = -1; //右窗口
    int window_size = 0;   //窗口大小
    int recent_num = 0;    //最近一个发送的编号
    FILE *fd = fopen(message, "rb");
    // int fd = open(message, O_RDONLY);
    if (fd ==NULL)
    {
        //  Send_data(2, strlen(message), SEQ_NUM(), message);
        printf("%s  fd open fail\n", message);
        fclose(fd);
        return -1; //发送文件失败
    }
    int over = 0; //判断文件是否发送结束

  //  printf("%d\n", okwindow[window_left]);
    while (1)
    {
        while (window_size < Window_size_ && !over)
        {
            // printf("send\n");
            char *data = new char[2048];
            int len = fread(data, 1, 1461, fd);
            if (len <= 0) //已读完
            {
                // printf("len over!\n");
                over = 1;
                break;
            }
            data[len] = 0;
            window_right = (window_right + 1) % Window_size_; //窗口右移
            window_size++;
            Q[window_right] = data;
            Qlen[window_right] = len;
            Send_data(2, len, SEQ_NUM(), data);
            // printf("len =%d left %d right %d size= %d recentnum= %d seq_num %d\n", len, window_left, window_right, window_size, recent_num, Seq_num);
        }
        if (over && window_size == 0)
            break;
        char buffer[2048];
        // int ackcount=0;
        while (window_size)
        {
            int len = Recv_(buffer);
            if(len==-1)
            continue;
            else if (len==0) //如果超时的话,重传一遍
            {
                // printf("chongchuan  recent_num=%d \n", recent_num);
                // printf("%d %d \n", window_left, recent_num);
                int i = window_left;
                int num = recent_num;
                for (int j = 0; j < window_size; j++)
                {
                    //  printf("* %d  %d  %d\n", i, okwindow[16], window_size);
                    if (!okwindow[i])
                    {
                        //   printf("* %d  %d  %d\n", i, okwindow[16], window_size);
                        Send_data(2, Qlen[i], num, Q[i]);
                    }
                    i = (i + 1) % Window_size_;
                    num++;
                }
            }
            else if (len>0)
            {
                rtp_header_t tmp;
                memcpy(&tmp, buffer, sizeof(rtp_header_t));
                if (tmp.seq_num >= recent_num && tmp.seq_num < recent_num + window_size) //收到之后的
                {
                    int h = (window_left + tmp.seq_num - recent_num) % Window_size_;
                    okwindow[h] = 1;
                }
                int flag = 0;
                //  printf("%d\n", tmp.seq_num);
                //  int h = (window_left + tmp.seq_num - recent_num) % Window_size_;
                while (okwindow[window_left])
                {
                    flag = 1;
                    okwindow[window_left] = 0;
                    recent_num++;
                    free(Q[window_left]);
                    window_left = (window_left + 1) % Window_size_;
                    window_size--;
                    // printf("ok \n");
                }
                //    printf("ok window size=%d recent_num=%d \n", window_size, recent_num);
                if (flag == 1)
                    break;

                // printf("count =%d \n",uuu);
                // uuu++;
            }
        }
    }
    fclose(fd);
    //  free(Q);
    delete[] Q;
    delete[] Qlen;
    delete[] okwindow;
    return 0;
}
void terminateSender()
{
    Send_(1, SEQ_NUM()); //发送end
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    fd_set rset;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; //超时时间100毫秒
    int len;             //当声明了一个文件描述符集后，必须用FD_ZERO将所有位置零。之后将我们所感兴趣的描述符所对应的位置位
    FD_ZERO(&rset);
    FD_SET(sock_, &rset); // select函数，拥塞等待文件描述符事件的到来；如果超过设定的时间，则不再等待，继续往下执行
    int result = select(sock_ + 1, &rset, NULL, NULL, &tv);
    if (!result)
    {
        close(sock_);
        printf("over&*&(\n");
        return;
    }
    // int len = Recv_(buffer);
    len = recvfrom(sock_, buffer, 2048, 0, (struct sockaddr *)&addr_, &addr_len_);
    close(sock_);
    printf("over&*&(\n");
    return;
}