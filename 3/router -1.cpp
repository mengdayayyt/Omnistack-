#include "router.h"
#include <iostream>
#include <arpa/inet.h>
#include <map>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <climits>

using namespace std;

#define TYPE_DV 0x00
#define TYPE_DATA 0x01
#define TYPE_CONTROL 0x02

void bit2(int num)
{
    for (int i = 31; i >= 0; i--)
    {
        printf("%d", (num >> i) & 1);
    }
    printf("\n");
}

uint32_t inside_host = ntohl(inet_addr("10.0.0.0"));
uint32_t host_mask = ~((1 << (32 - 24)) - 1);

Router *name2addr[N];                 // 名字对应的端口的映射
map<uint32_t, uint32_t> host_to_name; // 主机对应哪个端口
int s = 0;                            // 路由器的数量
int d[N][N];
int pre[N][N];
void ppp()
{
    cout << "------------------" << endl;
    for (int i = 0; i < s; i++)
    {
        for (int j = 0; j < s; j++)
        {
            cout << " *" << d[i][j] << "   " << pre[i][j];
        }
        cout << endl;
    }
    cout << "----------------" << endl;
}
int find_best(int from, int nameto)
{
    cout << "find_best " << from << " to " << nameto << endl;
    for (int i = 0; i < s; i++)
        for (int j = 0; j < s; j++)
            if (i == j)
            {
                d[i][j] = 0;
                pre[i][j] = j;
            }
            else
            {
                d[i][j] = 1 << 25;
                pre[i][j] = -1;
            }
    ppp();
    for (int i = 0; i < s; i++)
    {
        if (name2addr[i])
            cout << "ok" << endl;
        Router *tmp = name2addr[i];
        for (int j = 1; j <= tmp->port_num; j++)
            if (tmp->self_weight[j])
            {
                cout << "find_best  port=" << j << " to= " << tmp->port_to_name[j] << "weight= " << tmp->self_weight[j] << endl;
                d[i][tmp->port_to_name[j]] = tmp->self_weight[j];
                pre[i][tmp->port_to_name[j]] = tmp->port_to_name[j];
            }
    }
    //  ppp();
    for (int v = 0; v < s; v++)
        for (int i = 0; i < s; i++)
            for (int j = 0; j < s; j++)
                if ((d[i][v] + d[v][j]) < d[i][j])
                {
                    d[i][j] = d[i][v] + d[v][j];
                    pre[i][j] = pre[i][v];
                }
    ppp();
    cout << "find_best " << from << " to " << nameto << " answer_port" << pre[from][nameto] << endl;
    return pre[from][nameto];
}

// 路由器数量不超过 100 个， host 数量不超过 300 个， 所有路由器的可用公网地址数量之和不超过 1024 个
RouterBase *create_router_object()
{
    return new Router;
}
void Router::router_init(int port_num_, int external_port_, char *external_addr_, char *available_addr_)
{
    name = s;
    name2addr[s] = this;
    s++;
    port_num = port_num_;
    external_port = external_port_;
    if (external_addr_ != NULL)
    {
        int i = 0;
        while (external_addr_[i] != '/')
            i++;
        char IP[1024];
        memcpy(IP, external_addr_, i);
        int n = atoi(external_addr_ + i + 1);
        cout << i << " " << n << " " << IP << endl;
        uint32_t number = inet_addr(IP); // 从 IPv4 的数字点表示形式转换为以网络字节顺序的二进制形式
        external_addr = ntohl(number);
        external_mask = ~((1 << (32 - n)) - 1);
        bit2(number);
        bit2(external_addr);
        bit2(external_mask);
    }
    if (available_addr_ != NULL)
    {
        int j = 0;
        while (available_addr_[j] != '/')
            j++;
        char addr[1024];
        memcpy(addr, available_addr_, j);
        int n = atoi(available_addr_ + j + 1);
        addr[j] = 0;
        cout << j << " " << n << " " << addr << endl;
        uint32_t number = inet_addr(addr); // 从 IPv4 的数字点表示形式转换为以网络字节顺序的二进制形式
        available_addr = ntohl(number);
        available_mask = ~((1 << (32 - n)) - 1);
        available_num = ((1 << (32 - n)) - 1);
        bit2(available_addr);
        bit2(available_mask);
    }
    cout << "start" << name << " " << port_num << " " << external_port << " " << available_num << endl;
    return;
}
// router：收到的入端口号+报文
// 注意，src 与 dst 字段为大端法表示，其余字段均为小端法表示。
int Router::router(int in_port, char *packet)
{
    cout << name + 1 << "   inport " << in_port << endl;
    Header *tmp = (Header *)packet;
    // 注意大小端
    if (tmp->type == TYPE_DV) // 自己交换距离向量
    {
        // 把packet改成距离向量
        int othername = packet[12];
        name_to_port[othername] = in_port;
        port_to_name[in_port] = othername;
        cout << name << " " << in_port << " " << othername << endl;
        return -1;
    }
    else if (tmp->type == TYPE_DATA) // 数据
    {
        // 如果报文从自己的外网端口进来，进行地址转换
        if (in_port == external_port)
        {
            if (available_addrs.find(ntohl(tmp->dst)) == available_addrs.end())
            {
                //  cout << "无效 waiwang addr" << endl;
                return -1;
            }
            else
            {
                bit2(ntohl(tmp->src));
                bit2(ntohl(tmp->dst));
                tmp->dst = available_addrs[ntohl(tmp->dst)];
                // cout << "zhuji ok" << endl;
            }
        }
        else
        {
            // 判断目的地址是否属于外网
            int e_name = -1;
            for (int i = 0; i < s; i++)
            {
                Router *t = name2addr[i];
                if (t->external_port == 0)
                    continue;
                uint32_t t1 = ntohl(tmp->dst) & t->external_mask;
                uint32_t t2 = t->external_addr & t->external_mask;
                if (t1 == t2)
                {
                    e_name = i;
                    break;
                }
            }
            cout << e_name + 1 << "  * " << endl;
            if (e_name != -1)
            {
                if (e_name == name) // 要从自己的外网端口出去，进行网络地址转换
                {
                    if (host_addrs.find(tmp->src) != host_addrs.end()) // 已经分配地址
                    {
                        tmp->src = host_addrs[tmp->src];
                        tmp->src=htonl(tmp->src);
                        cout << "already have" << endl;
                        return external_port;
                    }
                    uint32_t available = available_addr & available_mask;
                    for (int i = 0; i <= available_num; i++)
                    {
                        // 找到空闲的话
                        if (available_addrs.find(available + i) == available_addrs.end())
                        {
                            available_addrs[available + i] = tmp->src;
                            host_addrs[tmp->src] = available + i;
                            tmp->src = htonl(available + i);

                            cout << "okok find it!" << endl;
                            bit2(ntohl(tmp->src));
                            bit2(ntohl(tmp->dst));
                            return external_port;
                        }
                    }
                    return -1;
                }
                int best = find_best(name, e_name);
                cout << " external answer " << name_to_port[best] << endl;
                return name_to_port[best];
            }
        }
        // 再判断目的地址是否主机
        //  cout<<host_to_name[tmp->dst]<<endl;
        if (host_to_name.find(tmp->dst) == host_to_name.end())
        {
            cout << "no host!" << endl;
            return 1;
        }
        int toname = host_to_name[tmp->dst];
        if (toname == name) // 如果目的地址是自己的话，返回对应端口即可。
        {
            return host_to[tmp->dst];
        }
        cout << name << " receive data   toname:" << toname << endl;
        int best = find_best(name, toname);
        cout << " answer " << name_to_port[best] << endl;
        if (best == -1)
            return 1;

        return name_to_port[best];

        return 1;
    }
    else if (tmp->type == TYPE_CONTROL) // payload 包含一条指令字符串
    {
        char *data = packet + 12;
        char type = data[0];
        if (type == '0') // 该交换距离向量了
        {
            struct Header tmp;
            tmp.length = 4;
            tmp.type = TYPE_DV;
            memcpy(packet, &tmp, 12);
            memcpy(packet + 12, &name, 4);
            cout << name << " send dv ok" << endl;
            return 0;
        }
        else if (type == '1') // 释放公网地址；
        {
            uint32_t number = inet_addr(data + 2);
            //number = ntohl(number);
            host_addrs.erase(number);
            bit2(number);
           // available_addrs.erase(number);
            map<uint32_t, uint32_t>::iterator it;
            for (it = available_addrs.begin(); it != available_addrs.end(); it++)
            {
                if (it->second == number)
                {
                    bit2(it->first);
                    available_addrs.erase(it->first);
                    return -1;
                }
            }
            // host_addrs
        }
        else if (type == '2') // 更改链路权值
        {
            int type1 = atoi(data + 2);
            int i = 2;
            while (data[i] != ' ')
                i++;
            i++;
            int type2 = atoi(data + i);
            if (type2 == -1) // 关闭端口
            {
                  if (port_to.find(type1) != port_to.end()) // 如果发现是主机
                {
                    host_to_name.erase(port_to[type1]);
                    port_to.erase(type1);
                }
                //port_to.erase(type1);
                self_weight.erase(type1);
            }
            else
                self_weight[type1] = type2;
            cout << name << " change port weight type1:" << type1 << "   type2:" << type2 << endl;
            return -1;
        }
        else if (type == '3') // 端口增加主机
        {
            int type1 = atoi(data + 2);
            int i = 2;
            while (data[i] != ' ')
                i++;
            i++;
            uint32_t host = inet_addr(data + i);
            port_to[type1] = host;
            host_to[host] = type1;
            host_to_name[host] = name;
            // 还要改变dv；
            cout << name << " " << type1 << " new host" << endl;
            return -1;
        }
        else
            printf("control error\n");
    }
    else
        printf("type error \n");
    return -1;
}