#include "router_prototype.h"
#include <map>
#include <cstring>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;
#define N 101

struct Header
{
    uint32_t src;
    uint32_t dst;
    uint8_t type;
    uint16_t length;
};

class Router : public RouterBase
{
public:
    int name;
    int port_num; //端口数量

    map<int, uint32_t> port_to;      //端口x对应的主机映射；
    map<uint32_t, int> host_to; 
    map<int, int> name_to_port; //哪个路由器对应的端口
    map<int, int> port_to_name;
    map<int, int> self_weight; //每个端口对应的链路权值；
                                  
    int external_port = 0; //连接外网的端口,0就表示没有连接外网
    uint32_t external_addr;
    uint32_t external_mask;

    map<uint32_t, uint32_t> available_addrs; //公网映射->主机；
    map<uint32_t, uint32_t> host_addrs;      //主机->公网
    uint32_t available_addr;                 //可用公共地址
    uint32_t available_mask;
    int available_num;

    void router_init(int port_num_, int external_port_, char *external_addr, char *available_addr);
    int router(int in_port, char *packet);
};
//  router_init 路由器的端口数，外网端口号，外网的范围地址，可用公网地址范围
// 1号端口是controller，当 external_port = 0 时， external_addr 与 available_addr 为空指针