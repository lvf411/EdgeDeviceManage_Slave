#include "slave.hpp"
#include <jsoncpp/json/json.h>
#include <fstream>

#define INITFILE "slave_init.json"

using namespace std;

Slave slave;
list_head task_list;

//系统初始化
void startup(){
    ifstream ifs(INITFILE);
    if(!ifs.is_open())
    {
        perror("open init file error");
        exit(0);
    }
    
    Json::Value root;
    Json::Reader rd;
    rd.parse(ifs,root);

    //根据init文件绑定端口，连接服务器
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket create error");
        exit(0);
    }

    struct sockaddr_in selfaddr, destaddr;
    selfaddr.sin_family = AF_INET;
    selfaddr.sin_addr.s_addr = inet_addr(root["ip"].asCString());
    selfaddr.sin_port = htons(root["port"].asInt());
    if(bind(sock, (struct sockaddr *)&selfaddr, sizeof(selfaddr)) < 0)
    {
        perror("socket bind error");
        exit(0);
    }

    destaddr.sin_addr.s_addr = inet_addr(root["master_ip"].asCString());
    destaddr.sin_family = AF_INET;
    destaddr.sin_port = htons(root["master_port"].asInt());
    while(connect(sock, (struct sockaddr *)&destaddr, sizeof(destaddr)) < 0)
    {
        perror("connect to master error");
        sleep(5);
    }

    //初始化slave实例
    slave.sock = sock;
    slave.addr.sin_family = selfaddr.sin_family;
    slave.addr.sin_addr.s_addr = inet_addr(root["ip"].asCString());
    slave.addr.sin_port = htons(root["port"].asInt());
    slave.master_addr.sin_family = destaddr.sin_family;
    slave.master_addr.sin_addr.s_addr = inet_addr(root["master_ip"].asCString());
    slave.master_addr.sin_port = htons(root["master_port"].asInt());
    slave.slave_id = 0;
    slave.task_num = 0;
    slave.task = task_list;
    slave.current_task = NULL;
    slave.next_task = NULL;

}

int main()
{
    startup();
}