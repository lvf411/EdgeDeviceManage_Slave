#ifndef __MASTER_HPP
#define __MASTER_HPP

#include <iostream>
#include <list>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <thread>
#include "list.hpp"

#define SLAVE_ABILITY_DEFAULT 10

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
};

//客户端链表节点
struct ClientNode{
    int client_id;                      //设备编号
    int sock;                           //与客户端通信的文件描述符
    struct sockaddr_in addr;            //客户端的地址信息
    int flag;                           //表示当前该进程是否在运行，若为-1表示空闲；若大于0，表示分配给编号为flag的任务运行
    int ability;                        //执行任务的效率，能力，越大越强
    int subtask_num;                    //分配到的子任务数量
    struct list_head head;              //子任务链表头地址，若flag为-1，为空闲节点；若大于0，为分配给该从节点的子任务链表表头
    struct list_head self;              //指向自身在客户端链表中的指针
    std::thread msg_send_threadID;      //消息发送线程ID
    std::thread msg_recv_threadID;      //消息接收线程ID
    int modified;                       //修改标记，当值为0时表示没有受到修改，没有分配新的子任务；当被置为1时，表示被分配了新的子任务，需要同步任务链表
    int status;                         //分配的发送/接收线程状态，用以指示状态机运行以及部分同步问题
    int file_trans_sock;                //文件传输时与从节点建立的新连接
    int file_trans_port;                //文件传输时从节点提供的端口号
};

//任务描述
struct Task{
    int id;                             //系统内的任务编号
    std::string task_id;                //任务自身编号
    int subtask_num;                    //可分解的子任务个数
    struct list_head subtask_head;      //子任务链表表头
    struct list_head self;              //自身在任务链表中的指针
};

//服务端信息结构体
struct Master{
    int sock;                           //监听的文件描述符
    struct sockaddr_in addr;            //服务端的地址信息
    int free_client_num;                //空闲设备数量
    struct list_head free_client_head;  //空闲设备链表表头
    int work_client_num;                //工作设备数量
    struct list_head work_client_head;  //工作设备链表表头
    int task_num;                       //已分配的任务数量
    struct list_head task_list_head;    //已分配的任务链表表头
    int uninit_task_num;                //未分配的任务数量
    struct list_head uninit_task_list_head;     //未分配的任务的链表表头
};

#endif //__MASTER_HPP