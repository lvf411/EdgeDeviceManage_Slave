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
#include "file.hpp"
#include <thread>
#include "semaphore.hpp"

#define SLAVE_STATUS_ORIGINAL               0
#define SLAVE_STATUS_FILERECV_REQ_RECV      100
#define SLAVE_STATUS_FILERECV_WAIT_CONN     101

//子任务间结果传递
struct SubTaskResult{
    int client_id;                      //对应前驱和后继被分配的从节点ID
    int subtask_id;                     //子任务的ID
    std::string fname;                  //传递的结果的文件名
    struct SubTaskResult *next;
};

//子任务描述节点
struct SubTaskNode{
    int subtask_id;                     //标记当前子任务在整个任务中的编号
    int root_id;                        //标记整个任务在系统中的编号
    int client_id;                      //标记被分配到的设备编号
    int prev_num;                       //标记子任务需要的前驱文件数量
    int cprev_num;                      //标记运行当前子任务需要传来参数的前驱的数量
    struct SubTaskResult *prev_head;    //运行当前子任务需要传来参数的前驱的输出结果链表头节点
    int next_num;                       //标记子任务运行生成的后继输出文件数量
    struct SubTaskResult *succ_head;    //当前子任务需要向后传递的后继信息链表头结点
    struct list_head *head;             //任务链表表头
    struct list_head self;              //指向自身在链表中的指针
    std::string exepath;                //子任务执行文件路径
    bool exe_flag;                      //指示子任务执行文件是否已从主节点接收
};

//客户端链表节点
struct PeerNode{
    int client_id;                      //对方的设备编号
    int sock;                           //与对方客户端通信的文件描述符
    struct sockaddr_in addr;            //对方客户端的地址信息
    int local_port;                     //该连接中与对方连接的本地端口
    std::thread msg_send_threadID_S;    //作为服务端，消息发送线程ID，服务端接收来自客户端的前驱文件
    std::thread msg_recv_threadID_S;    //作为服务端，消息接收线程ID
    std::thread msg_send_threadID_C;    //作为客户端，消息发送线程ID
    std::thread msg_recv_threadID_C;    //作为客户端，消息接收线程ID
    struct list_head self;              //指向自身在从节点客户端链表中的指针
    struct list_head *head;             //指向从节点客户端链表表头
    int status;                         //分配的发送/接收线程状态，用以指示状态机运行以及部分同步问题
    bool file_trans_flag;               //指示当前线程是否正在进行文件传输，每个连接只支持同时进行一个文件数据传输
    int file_trans_sock;                //文件传输时与从节点建立的新连接，文件发送端为文件传输sock，文件接收端为监听sock
    int file_trans_port;                //文件接收方提供的通讯端口
    FileTransInfo *current_file_trans_info;      //正在传输的文件的信息
    std::thread file_trans_threadID;    //数据传输线程的ID
    Semaphore sem;                      //实现消息发送/接收线程同步的信号量
};

//服务端（接收端）起始状态
#define PEER_STATUS_S_ORIGINAL              SLAVE_STATUS_ORIGINAL
//服务端（接收端）收到来自发送端的文件传输请求
#define PEER_STATUS_S_FILERECV_REQ_RECV     SLAVE_STATUS_FILERECV_REQ_RECV
//服务端（接收端）等待来自发送端的对文件接收端口的连接
#define PEER_STATUS_S_FILERECV_WAIT_CONN    SLAVE_STATUS_FILERECV_WAIT_CONN

//客户端（发送端）起始状态
#define PEER_STATUS_C_ROOT                  0
//客户端（发送端）发送文件传输请求
#define PEER_STATUS_C_FILESEND_SEND_REQ     100
//客户端（发送端）等待服务端对文件传输请求的应答
#define PEER_STATUS_C_FILESEND_WAIT_ACK     101
//客户端（发送端）收到了服务端发来的应答，建立文件传输连接
#define PEER_STATUS_C_FILESEND_CONNECT      102
//客户端（发送端）与服务端建立文件传输连接后等待服务端发来开始信号
#define PEER_STATUS_C_FILESEND_WAIT_START   103
//客户端（发送端）向服务端正式发送文件
#define PEER_STATUS_C_FILESEND_SENDFILE     104

struct Slave{
    int sock;                           //与服务端通信的文件描述符
    int listen_sock;                    //供其他节点主动发起消息通讯连接的文件描述符
    int listen_port;                    //监听主动连接的端口
    struct sockaddr_in addr;            //自身连接主节点的地址信息
    struct sockaddr_in master_addr;     //服务端地址信息
    int slave_id;                       //服务端分配的客户端编号
    int task_num;                       //待执行的子任务数量
    struct list_head *task;             //子任务链表表头
    struct Task *current_task;          //当前正在执行的子任务
    struct Task *next_task;             //下一个执行的子任务
    std::map<int, struct sockaddr_in*> work_slave_addr;          //按照客户端编号存储的正在运行的所有客户端节点地址信息
    int status;                         //从节点此时所处的状态
    std::thread master_msg_send_threadID;       //与主节点通信的消息发送线程ID
    std::thread master_msg_recv_threadID;       //与主节点通信的消息接收线程ID
    bool file_trans_flag;               //指示当前线程是否正在进行文件传输，每个连接只支持同时进行一个文件数据传输
    int file_trans_listen_sock;         //从节点监听的数据接收文件描述符
    int file_trans_port;                //数据接收文件描述符绑定的监听端口
    FileTransInfo *current_file_trans_info;      //正在传输的文件的信息
    std::thread file_trans_threadID;    //数据接收的线程ID
    std::string containerID;            //执行子任务的容器的编号
};   

#endif //__SLAVE_HPP