#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <netinet/in.h>
#include<stdlib.h>
#define unused 0
#define MM 1024*2048
int connect_flag=0;
int sock=0;
int client;
int AUTH;

struct FTP_Header
{
    char m_protocol[6]; /* protocol magic number (6 bytes) */
    unsigned char m_type;        /* type (1 byte) */
    char m_status;      /* status (1 byte) */
    uint32_t m_length;  /* length (4 bytes) in Big endian*/
} __attribute__((packed));
void Send(struct FTP_Header m_header)//send header
{
    char buffer[128];
    memset(buffer,0,sizeof(buffer));
    memcpy(buffer, &m_header, 12); 
   send(client, buffer, 12, 0);
   return;
}
struct FTP_Header Recv()//recv 
{
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    int x=recv(client, buffer, 128, 0);
    if(x!=12)
    printf("%d fail\n",x);
    struct FTP_Header m_header ;
    memcpy(&m_header, buffer, 12); 
    m_header.m_length=ntohl( m_header.m_length);
    return m_header;
}
/*
struct FTP_Header Recv_data(char *buffer)
{
    struct FTP_Header m_header ;
    buffer=buffer+12;
    return m_header;
}*/
int Send_data(struct FTP_Header m_header,char* data)//all send 
{
    char buffer[2048];
    int l=strlen(data);
    memset(buffer,0,sizeof(buffer));
    memcpy(buffer, &m_header, 12); 
    memcpy(buffer+12,data,l+1);
   return(send(client, buffer,12+l+1, 0));
}
void Get(uint32_t len)
{
    
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    recv(client, buffer, len, 0);//rest

        struct FTP_Header m_header;
        memcpy(m_header.m_protocol, "\xe3myftp", 6);
        m_header.m_type = 0xA8;
        m_header.m_length=htonl(12);

        FILE *fd = NULL;//open file
        fd = fopen(buffer, "r");
        if(fd==NULL)
            {
                m_header.m_status = 0;
                Send(m_header);//reply
            }
        else
            {
                m_header.m_status = 1;
                Send(m_header);//reply

                memset(buffer,0,sizeof(buffer));
                fread(buffer,1,2048,fd);//read

                m_header.m_type = 0xFF;
                m_header.m_status = unused;
                m_header.m_length = htonl(12+strlen(buffer)+1);
                

                 char b[MM];
                int l=strlen(buffer);
                memset(b,0,sizeof(b));
                memcpy(b, &m_header, 12); 
                memcpy(b+12,buffer,l+1);

                int ret = 0;
                int len=12+strlen(buffer)+1;
                while (ret < len) {
                int x = send(client, b + ret, len - ret, 0);
                ret += x; 
                } 

               // if(Send_data(m_header,buffer)!=12+strlen(buffer)+1)
               //printf("error\n");
           
              }    
            fclose(fd);
}
void Put(uint32_t len)
{

    char name[2048];
    memset(name, 0, sizeof(name));
    recv(client, name, len, 0);//rest

    struct FTP_Header m_header;
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xAA;
    m_header.m_status=unused;
    m_header.m_length=htonl(12);
    Send(m_header);//reply

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    
    recv(client, buffer, 12, 0);
    memcpy(&m_header,buffer, 12); 
    m_header.m_length=ntohl( m_header.m_length);
    int h=m_header.m_length-12;
    recv(client, buffer,h, 0);
    
        FILE *fd = NULL;
        fd = fopen(name, "w");      
        int g= fwrite(buffer, 1, h, fd);  
        printf("%s %d\n",buffer,strlen(buffer));       
        fclose(fd); 
}
void Put_Big(uint32_t len)
{

    char name[2048];
    memset(name, 0, sizeof(name));
    recv(client, name, len, 0);//rest

    struct FTP_Header m_header;
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xAA;
    m_header.m_status=unused;
    m_header.m_length=htonl(12);
    Send(m_header);//reply

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    
    recv(client, buffer, 12, 0);
    memcpy(&m_header,buffer, 12); 
    m_header.m_length=ntohl( m_header.m_length);
    int h=m_header.m_length-12;


        FILE *fd = NULL;
        fd = fopen(name, "w");   
  //  recv(client, buffer,h, 0);
            int count;    
           int ret = 0;
            while (ret < h) {
            count = recv(client, buffer,2048, 0);
            fwrite(buffer,1, count, fd);
            memset(buffer,0,sizeof(buffer));
            ret += count; 
            }   
      //  int g= fwrite(buffer, 1, h, fd);        
        fclose(fd); 
}
void doit()
{
     struct FTP_Header m_header;
    memcpy(m_header.m_protocol, "\xe3myftp", 6);

    int op;
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
   while(op=recv(client, buffer, 12, 0))
   {

    memcpy(&m_header, buffer, 12); 
    m_header.m_length=ntohl(m_header.m_length);//receve header
    
    if(m_header.m_type==(0xA3))//auth
    {
         m_header.m_type = 0xA4;
          memset(buffer, 0, sizeof(buffer));
        op=recv(client, buffer, m_header.m_length-12, 0);
        m_header.m_length = htonl(12);
        
        if(strncmp(buffer, "user 123123\0",12) == 0)
            {
                AUTH=1;
                m_header.m_status = 1;
            } 
        else
         m_header.m_status = 0;
          Send(m_header);
    }
   else if(m_header.m_type==(0xA5))//ls
   {
    FILE* file=NULL;
    memset(buffer, 0, sizeof(buffer));
    file = popen("ls", "r");
    fread(buffer,1,2048, file);
    printf("%s\n",buffer);
    m_header.m_type = 0xA6;
    m_header.m_status = unused;
    m_header.m_length = htonl(12+strlen(buffer)+1);
    if(Send_data(m_header,buffer)!=12+strlen(buffer)+1)
    printf("error\n");
    pclose(file);
    }
    else if(m_header.m_type==(0xA7))//get request
    {
        Get(m_header.m_length-12);
    }
    else if(m_header.m_type==(0xA9))//get request
    {
        Put_Big(m_header.m_length-12);
    }
    else 
    printf(" error%d  %d\n",buffer,strlen(buffer),op);

    memset(buffer, 0, sizeof(buffer));
   }
}
int main(int argc, char ** argv) 
{
    int post=atoi(argv[2]);
    sock = socket(AF_INET, SOCK_STREAM, 0);//申请一个TCP的socket
    struct sockaddr_in addr;
    addr.sin_port = htons(post); 
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &addr.sin_addr); 
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock,128)<0;
    client = accept(sock, NULL, NULL);
    struct FTP_Header m_header;
    m_header=Recv();
    memcpy(m_header.m_protocol, "\xe3myftp", 6);
    m_header.m_type = 0xA2;
    m_header.m_status = 1;
    m_header.m_length = htonl(12);
    Send(m_header);
    //open success@!
    AUTH=0;
    //
    doit();
    return 0;
}

