#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <netinet/in.h>
#define unused 0
#define MM 1024*2048
int connect_flag=0;
int sock=0;
int AUTH;

struct FTP_Header
{
    char m_protocol[6]; /* protocol magic number (6 bytes) */
    unsigned char m_type;        /* type (1 byte) */
    char m_status;      /* status (1 byte) */
    uint32_t m_length;  /* length (4 bytes) in Big endian*/
} __attribute__((packed));

int Send_data(struct FTP_Header m_header,char* data)//all send 
{
    char buffer[128];
    int l=strlen(data);
    memset(buffer,0,sizeof(buffer));
    memcpy(buffer, &m_header, 12); 
    memcpy(buffer+12,data,l+1);
   return(send(sock, buffer,12+l+1, 0));
}
int Send(struct FTP_Header m_header)//send header
{
    char buffer[128];
    memset(buffer,0,sizeof(buffer));
    memcpy(buffer, &m_header, 12); 
   return(send(sock, buffer, 12, 0));
}
struct FTP_Header Recv()//recv 
{
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    if((recv(sock, buffer, 128, 0))!=12)
    printf("%s \n","recv failed");
    struct FTP_Header m_header ;
    memcpy(&m_header, buffer, 12); 
    m_header.m_length=ntohl( m_header.m_length);
    return m_header;
}
struct FTP_Header Recv_data(char *buffer)
{
    int op;
    memset(buffer, 0, sizeof(buffer));
    op=recv(sock, buffer, 2048, 0);
    struct FTP_Header m_header ;
    memcpy(&m_header, buffer, 12); 
    m_header.m_length=ntohl(m_header.m_length);
    return m_header;
}
void Get_Big(char* data)
{
    struct FTP_Header m_header; // define
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xA7;
    m_header.m_status = unused;
    m_header.m_length = htonl(12+strlen(data)+1);

    if(Send_data(m_header, data)!=12+strlen(data)+1)
    printf("%s\n","get send data fail");
 
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
     if((recv(sock, buffer, 12, 0))!=12)
    printf("%s \n","recv failed");
    memcpy(&m_header,buffer, 12); 
    m_header.m_length=ntohl( m_header.m_length);
     struct FTP_Header m_header2 ;
    if((recv(sock, buffer, 12, 0))!=12)
    printf("%s \n","recv failed");
    memcpy(&m_header2,buffer, 12); 
    m_header2.m_length=ntohl( m_header2.m_length);
    int len=m_header2.m_length-12;
    if(m_header.m_status==0)
    {
        printf("%s","No File!\n");
    }
    else
    {
        FILE *fd = NULL;
        fd = fopen(data, "w");      
        if (fd == NULL)
        {
            printf("%s\n","fopen error!");
            return ;
        }
        int count;    
           int ret = 0;
            while (ret < len) {
            count = recv(sock, buffer,2048, 0);
            fwrite(buffer,1, count, fd);
            memset(buffer,0,sizeof(buffer));
            ret += count; 
            }    
        fclose(fd); 
    }
    return;
}
void Get(char* data)
{
    struct FTP_Header m_header; // define
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xA7;
    m_header.m_status = unused;
    m_header.m_length = htonl(12+strlen(data)+1);
  
    if(Send_data(m_header, data)!=12+strlen(data)+1)
    printf("%s\n","get send data fail");
 
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
     if((recv(sock, buffer, 12, 0))!=12)
    printf("%s \n","recv failed");
    memcpy(&m_header,buffer, 12); 
    m_header.m_length=ntohl( m_header.m_length);
     struct FTP_Header m_header2 ;
    if((recv(sock, buffer, 12, 0))!=12)
    printf("%s \n","recv failed");
    memcpy(&m_header2,buffer, 12); 
    m_header2.m_length=ntohl( m_header2.m_length);
    int h=recv(sock, buffer, 128, 0);
    if(m_header.m_status==0)
    {
        printf("%s","No File!\n");
    }
    else
    {
        FILE *fd = NULL;
        fd = fopen(data, "w");      
        if (fd == NULL)
        {
            printf("%s\n","fopen error!");
            return ;
        }
        int g= fwrite(buffer, 1, h, fd);  
        printf("%s %d\n",buffer,strlen(buffer));       
        fclose(fd); 
    }
    return;
}
void Put(char* filename)
{
        FILE *fd = NULL;
        fd = fopen(filename, "r");
        if (fd)
        {
            char buffer[128];
            fread(buffer,1,128,fd);
            struct FTP_Header m_header; // define
            memcpy(m_header.m_protocol, "\xe3myftp", 6);
            m_header.m_type = 0xA9;
           m_header.m_status = unused;
           m_header.m_length = htonl(12+strlen(filename)+1);
        if(Send_data(m_header,filename)!=12+strlen(filename)+1)
          printf("%s\n","get send data fail");
           m_header=Recv();

            m_header.m_type = 0xFF;
           m_header.m_status = unused;
           m_header.m_length = htonl(12+strlen(buffer)+1);
         // printf("%d.....\n",6);
        if(Send_data(m_header,buffer)!=12+strlen(buffer)+1)
          printf("%s\n","put send data fail");
            fclose(fd);
        }
        else
        {
           printf("%s\n","no file");
                return ;
        }
}

void Put_Big(char* filename)
{
        FILE *fd = NULL;
        fd = fopen(filename, "r");
            char buffer[2048];
            fread(buffer,1,2048,fd);
            struct FTP_Header m_header; // define
            memcpy(m_header.m_protocol, "\xe3myftp", 6);
            m_header.m_type = 0xA9;
           m_header.m_status = unused;
           m_header.m_length = htonl(12+strlen(filename)+1);
        if(Send_data(m_header,filename)!=12+strlen(filename)+1)
          printf("%s\n","get send data fail");
            m_header=Recv();
            m_header.m_type = 0xFF;
           m_header.m_status = unused;
           m_header.m_length = htonl(12+strlen(buffer)+1);
  
    char b[MM];
    memset(b,0,sizeof(b));
    memcpy(b, &m_header, 12); 
    memcpy(b+12,buffer,strlen(buffer)+1);

    int ret = 0;
    int len=12+strlen(buffer)+1;
    while (ret < len) {
    int x = send(sock, b + ret, len - ret, 0);
    ret += x; 
    } 
            fclose(fd);
     return;
}
int Connect(char *IP, int port)
{
    AUTH = 0; // not auth
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0)
    printf("%s\n","sock fail");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, IP, &addr.sin_addr); // 127.0.0.1 12323
    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr))<0)
    printf("%s","connect fail!\n");
  

    struct FTP_Header m_header; // define
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xA1;
    m_header.m_status = unused;
    m_header.m_length = htonl(12);
   
    if(Send(m_header)!=12)
    printf("%s","send fail!\n");

   // char buffer[128];
    m_header = Recv();

    return m_header.m_status;
}

int Ls()
{
    struct FTP_Header m_header; // define
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xA5;
    m_header.m_status = unused;
    m_header.m_length = htonl(12);

    if(Send(m_header)!=12)
    printf("%s","ls Send fail\n");

    char buffer[2048];
    m_header=Recv_data(buffer);
    printf("%s",buffer+12);
}
int Auth(char *data)
{
    struct FTP_Header m_header; // define
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xA3;
    m_header.m_status = unused;
    m_header.m_length = htonl(12+strlen(data)+1);

  
    if(Send_data(m_header, data)!=12+strlen(data)+1)
    printf("%s\n","send data fail");
    //char buffer[128];
    m_header = Recv();
    if(m_header.m_status==0)
    {
        close(sock);
        connect_flag=0;
        return 0;
    }
    else
    {
        AUTH=1;
        return 1;
    }
}

void Close()
{
    struct FTP_Header m_header; // define
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xAB;
    m_header.m_status = unused;
    m_header.m_length = htonl(12);
   
    if(Send(m_header)!=12)
    printf("%s","send fail!\n");

    printf("%d\n",3);
     m_header=Recv();

    if (close(sock) < 0)
    {
        printf("%s", "close error!\n");
    }
    else
    {
         printf("%s", "close !\n");
    }
}
int main(int argc, char **argv)
{
     printf("%s\n","welcome to yyt's client!");
    char order1[128];
    char order2[128];
    char order4[128];
    int order3;
    while(1)
    {
        memset(order1,0,sizeof(order1));
        memset(order1,0,sizeof(order2));
        memset(order1,0,sizeof(order4));
    scanf("%s", order1);
    if (strncmp(order1, "open", 4) == 0)//open IP PORT
    {
       
        scanf("%s", order2);
        scanf("%d", &order3);
        
        if(connect_flag)
        {
            printf("%s","have connected\n");
            continue;
        }

        if (Connect(order2, order3)>0)
           {
            connect_flag=1;
            printf("%s", "Server connection accepted !\n");
           } 
        else
            printf("%s", "connect fail!\n");
    }
    else if(strncmp(order1, "auth", 4) == 0)//auth user pass
    {
        scanf("%s", order2);
        scanf("%s", order4);

        char buf[128];
        memset(buf,0,sizeof(buf));

        int l1=strlen(order2);
        memcpy(buf, order2,l1);
        buf[l1]=' ';
        memcpy(buf+l1+1, order4,strlen(order4));
    
        if(Auth(buf))
        printf("%s\n","auth success");
        else
        printf("%s\n","auth fail");
    }
    else if(strncmp(order1, "ls", 2) == 0)
    {
        Ls();
    }
     else if(strncmp(order1, "get",3) == 0)
     {
        scanf("%s", order2);
        Get_Big(order2);
     }
     else if(strncmp(order1, "put",3) == 0)
     {
        scanf("%s", order2);
        Put_Big(order2);
     }
      else if(strncmp(order1, "quit",4) == 0)
     {
        Close();
     }
    else
    {
        printf("%s", "input error!\n");
    }
    }
    return 0;
}
