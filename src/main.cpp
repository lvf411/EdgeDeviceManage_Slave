#include "slave.hpp"
#include <jsoncpp/json/json.h>
#include <fstream>
#include <thread>
#include <map>
#include "msg.hpp"

#define INITFILE "slave_init.json"
#define MAX_CONN_COUNT  10

using namespace std;

Slave slave;
list_head task_list, peer_list, file_req_list;
mutex mutex_peer_list;
map<int, PeerNode*> peer_list_map;

//系统初始化
int startup(){
    ifstream ifs(INITFILE);
    if(!ifs.is_open())
    {
        perror("open init file error");
        exit(0);
    }
    
    Json::Value root;
    Json::Reader rd;
    rd.parse(ifs,root);

    //根据init文件绑定监听端口，连接服务器(2个socket)
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_sock < 0)
    {
        perror("listen socket create error");
        exit(0);
    }
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket create error");
        exit(0);
    }

    //绑定监听socket地址
    struct sockaddr_in selfaddr, destaddr;
    selfaddr.sin_family = AF_INET;
    selfaddr.sin_addr.s_addr = inet_addr(root["ip"].asCString());
    selfaddr.sin_port = htons(root["listen_port"].asInt());
    if(bind(listen_sock, (struct sockaddr *)&selfaddr, sizeof(selfaddr)) < 0)
    {
        perror("socket bind error");
        exit(0);
    }

    //向主节点监听地址发起连接
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
    slave.listen_sock = listen_sock;
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

    //初始化peer_list
    peer_list = LIST_HEAD_INIT(peer_list);
    //初始化文件请求队列file_req_list
    file_req_list = LIST_HEAD_INIT(file_req_list);
    return listen_sock;
}

//根据ip端口地址找到对应客户端的ID
int get_clientID(struct sockaddr_in* addr)
{
    for(map<int, struct sockaddr_in*>::iterator it = slave.work_slave_addr.begin(); it != slave.work_slave_addr.end(); it++)
    {
        if(addr->sin_addr.s_addr == it->second->sin_addr.s_addr)
        {
            if(addr->sin_port == it->second->sin_port)
            {
                return it->first;
            }
        }
    }
    return -1;
}

//从节点连接主节点线程，接收从节点连接并将其加入到从节点管理链表，为每个从节点连接分配单独的消息收发线程
void slave_accept(int sock)
{
    struct sockaddr_in client;
	socklen_t len = sizeof(client);
    while(1)
    {
        int new_sock = accept(sock,(struct sockaddr *)&client,&len);

        mutex_peer_list.lock();
        PeerNode *peerNode = new PeerNode();

        peerNode->sock = new_sock;
        peerNode->addr.sin_family = client.sin_family;
        peerNode->addr.sin_addr.s_addr = client.sin_addr.s_addr;
        peerNode->addr.sin_port = client.sin_port;
        
        //根据ip端口找到客户端id
        peerNode->client_id = get_clientID(&(peerNode->addr));
        if(peerNode->client_id < 0)
        {
            delete peerNode;
            perror("accept peer connect error: wrong client ID");
            continue;
        }
        
        list_head *self = new list_head();
        *self = LIST_HEAD_INIT(*self);
        peerNode->self = *self;
        list_add_tail(self, &peer_list);
        peer_list_map.insert(map<int, PeerNode *>::value_type(peerNode->client_id, peerNode));
        mutex_peer_list.unlock();
        
        peerNode->msg_send_threadID_S = thread(peerS_msg_send, peerNode);
        peerNode->msg_recv_threadID_S = thread(peerS_msg_recv, peerNode);
        peerNode->status = 0;
        peerNode->file_trans_sock = -1;
    }
    return;
}

//向主节点请求文件
void file_req()
{
    while(1)
    {
        list_head *req_task = file_req_list.next;
        if(req_task == &file_req_list)
        {
            sleep(1);
            continue;
        }
        list_del(req_task);
        FileReqNode *node = list_entry(req_task, FileReqNode, self);
        Json::Value root;
        root["type"] = Json::Value(MSG_TYPE_FILEREQ_REQ);
        root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
        root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
        root["fname"] = Json::Value(node->fname);
        Json::FastWriter fw;
        std::stringstream ss;
        ss << fw.write(root);
        send(slave.sock, ss.str().c_str(), ss.str().length(), 0);

        delete req_task;
        //假若请求成功，也需时间传递文件
        sleep(3);
    }
}

//运行子任务=============================
void subtask_run()
{
    list_head *temp = &slave.task;
    while(1)
    {
        if(slave.task_num == 0)
        {
            sleep(1);
            continue;
        }

        if(temp == &slave.task)
        {
            temp = temp->next;
        }
        SubTaskNode *node = list_entry(temp, SubTaskNode, self);
        int count = 0;      //计数，防止所有待执行子任务都需等待其他子任务结果时陷入死循环
        while((node->prev_num > 0 || node->exe_flag == false) && count < slave.task_num){
            temp = temp->next;
            node = list_entry(temp, SubTaskNode, self);
            count++;
        }
        if(count == slave.task_num)
        {
            //循环一圈并未找到可以执行的子任务
            sleep(1);
            continue;
        }
        //找到可执行子任务的节点node
        //执行子任务========================

        //发送任务执行结果给后继
        SubTaskResult *tres;
        vector<int> failtrans_succ_client_id;
        for(int i = 0; i < node->next_num; i++)
        {
            tres = node->succ_head->next;
            int count = 0;
            while(count < MAX_CONN_COUNT)
            {
                int connsock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = slave.work_slave_addr.find(tres->client_id)->second->sin_port;
                addr.sin_addr.s_addr = slave.work_slave_addr.find(tres->client_id)->second->sin_addr.s_addr;
                int ret = connect(connsock, (struct sockaddr*)&addr, sizeof(addr));
                if(ret == 0)
                {
                    break;
                }
            }
            if(count >= MAX_CONN_COUNT)
            {
                failtrans_succ_client_id.push_back(tres->client_id);
                continue;
            }

            //建立客户端发送/接收线程发送结果文件===================
        }

        //向主节点通报子任务执行情况
        Json::Value root;
        root["type"] = Json::Value(MSG_TYPE_SUBTASK_RESULT);
        root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
        root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
        root["ret"] = Json::Value(true);

        Json::FastWriter fw;
        std::stringstream ss;
        ss << fw.write(root);
        send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
        
    }
}

int main()
{
    int sock = startup();
    thread slave_listen_threadID(slave_accept, sock);
    if(slave_listen_threadID.joinable())
    {
        slave_listen_threadID.join();
    }
    
    return 0;
}