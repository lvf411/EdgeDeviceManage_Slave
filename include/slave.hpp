#ifndef __SLAVE_HPP
#define __SLAVE_HPP

#include <iostream>
#include <list>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include "list.hpp"
#include <thread>

#define SLAVE_STATUS_ORIGINAL               0
#define SLAVE_STATUS_FILERECV_REQ_RECV      100
#define SLAVE_STATUS_FILERECV_WAIT_CONN     101

//子任务间结果传递
struct SubTaskResult{
    int client_id;                      //对应前驱和后继被分配的从节点ID
    int subtask_id;                     //子任务的ID
    struct SubTaskResult *next;
};

//子任务描述节点
struct SubTaskNode{
    int subtask_id;                     //标记当前子任务在整个任务中的编号
    int root_id;                        //标记整个任务在系统中的编号
    int client_id;                      //标记被分配到的设备编号
    int prev_num;                       //标记运行当前子任务需要传来参数的前驱的数量
    struct SubTaskResult *prev_head;    //运行当前子任务需要传来参数的前驱的输出结果链表头节点
    int next_num;                       //标记当前子任务需要向后传递的后继数量
    struct SubTaskResult *succ_head;    //当前子任务需要向后传递的后继信息链表头结点
    struct list_head head;              //任务链表表头
    struct list_head self;              //指向自身在链表中的指针
    std::string exepath;                //子任务执行文件路径
    bool exe_flag;                      //指示子任务执行文件是否已从主节点接收
};

//客户端链表节点
struct PeerNode{
    int client_id;                      //设备编号
    int sock;                           //与客户端通信的文件描述符
    struct sockaddr_in addr;            //客户端的地址信息
    std::thread msg_send_threadID_S;    //作为服务端，消息发送线程ID，服务端接收来自客户端的前驱文件
    std::thread msg_recv_threadID_S;    //作为服务端，消息接收线程ID
    std::thread msg_send_threadID_C;    //作为客户端，消息发送线程ID
    std::thread msg_recv_threadID_C;    //作为客户端，消息接收线程ID
    struct list_head self;              //指向自身在客户端链表中的指针
    int status;                         //分配的发送/接收线程状态，用以指示状态机运行以及部分同步问题
    int file_trans_sock;                //文件传输时与从节点建立的新连接
};

struct Slave{
    int sock;                           //与服务端通信的文件描述符
    int listen_sock;                    //供其他节点主动发起消息通讯连接的文件描述符
    struct sockaddr_in addr;            //自身的地址信息
    struct sockaddr_in master_addr;     //服务端地址信息
    int slave_id;                       //服务端分配的客户端编号
    int task_num;                       //待执行的子任务数量
    struct list_head task;              //子任务链表表头
    struct Task *current_task;          //当前正在执行的子任务
    struct Task *next_task;             //下一个执行的子任务
    std::map<int, struct sockaddr_in*> work_slave_addr;          //按照客户端编号存储的正在运行的所有客户端节点地址信息
    int status;                         //从节点此时所处的状态
    int file_trans_listen_sock;         //从节点监听的数据接收文件描述符
    int file_trans_port;                //数据接收文件描述符绑定的监听端口
    int file_trans_connect_sock;        //数据接收的数据传输文件描述符
    std::thread file_trans_threadID;    //数据接收的线程ID
};   

#endif //__SLAVE_HPP