#include "slave.hpp"
#include <jsoncpp/json/json.h>
#include <fstream>
#include <thread>
#include <map>
#include "msg.hpp"
#include <chrono>

#define INITFILE "slave_init.json"
#define CIDFILE "containerID.txt"
#define LOGFILEAME "logs.txt"
#define MAX_LISTENING 1000
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
    if(listen(listen_sock, MAX_LISTENING) < 0)
	{
		perror("listen");
		exit(3);
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
    struct sockaddr_in local_addr;
    socklen_t addrlen = sizeof(local_addr);
    getsockname(slave.sock, (struct sockaddr *)&local_addr, &addrlen);
    slave.addr.sin_port = local_addr.sin_port;
    slave.listen_port = root["listen_port"].asInt();
    slave.master_addr.sin_family = destaddr.sin_family;
    slave.master_addr.sin_addr.s_addr = inet_addr(root["master_ip"].asCString());
    slave.master_addr.sin_port = htons(root["master_port"].asInt());
    slave.slave_id = 0;
    slave.task_num = 0;
    slave.unexecutedTaskNum = 0;
    slave.downloadedTaskNum = 0;
    task_list = LIST_HEAD_INIT(task_list);
    slave.task = &task_list;
    slave.current_file_trans_info = new FileTransInfo();
    slave.master_msg_send_threadID = thread(msg_send);
    slave.master_msg_recv_threadID = thread(msg_recv);
    slave.runFlag = false;

    //初始化peer_list
    peer_list = LIST_HEAD_INIT(peer_list);
    //初始化文件请求队列file_req_list
    file_req_list = LIST_HEAD_INIT(file_req_list);

    //向主节点发送自身监听端口的信息
    Json::Value obj;
    obj["listen_port"] = slave.listen_port;
    stringstream ss;
    Json::FastWriter fw;
    ss << fw.write(obj);
    send(slave.sock, ss.str().c_str(), ss.str().length(), 0);

    ifs.close();

    ifs.open(CIDFILE);
    getline(ifs, slave.containerID);
    ifs.close();

    return listen_sock;
}

//根据ip端口地址找到对应客户端的ID
int get_clientID(struct sockaddr_in* addr)
{
    for(map<int, struct sockaddr_in*>::iterator it = slave.work_slave_addr.begin(); it != slave.work_slave_addr.end(); it++)
    {
        if(addr->sin_addr.s_addr == it->second->sin_addr.s_addr)
        {
            return it->first;
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
        peerNode->current_file_trans_info = new FileTransInfo();

        peerNode->sock = new_sock;
        peerNode->addr.sin_family = client.sin_family;
        peerNode->addr.sin_addr.s_addr = client.sin_addr.s_addr;
        peerNode->addr.sin_port = client.sin_port;
        FileInfoInit(&peerNode->current_file_trans_info->info);
        
        //根据ip端口找到客户端id
        peerNode->client_id = get_clientID(&(peerNode->addr));
        if(peerNode->client_id < 0)
        {
            delete peerNode;
            perror("accept peer connect error: wrong client ID");
            continue;
        }
        
        peerNode->head = &peer_list;
        peerNode->self = LIST_HEAD_INIT(peerNode->self);
        list_add_tail(&peerNode->self, &peer_list);
        peer_list_map.insert(map<int, PeerNode *>::value_type(peerNode->client_id, peerNode));
        mutex_peer_list.unlock();
        
        peerNode->msg_send_threadID_S = thread(peerS_msg_send, peerNode);
        peerNode->msg_send_threadID_S.detach();
        peerNode->msg_recv_threadID_S = thread(peerS_msg_recv, peerNode);
        peerNode->msg_recv_threadID_S.detach();
        peerNode->status = PEER_STATUS_S_ORIGINAL;
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
        root["rootid"] = Json::Value(node->rootid);
        root["subtaskid"] = Json::Value(node->subtaskid);
        Json::FastWriter fw;
        std::stringstream ss;
        ss << fw.write(root);
        send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
	    std::cout << ss.str() << std::endl;

        delete node;
        //假若请求成功，也需时间传递文件
        sleep(3);
    }
}

//运行子任务=============================
void subtask_run()
{
    int ret;
    list_head *temp = slave.task;
    string logname = LOGFILEAME;
    while(1)
    {
        if(slave.downloadedTaskNum != slave.task_num)
        {
            static int temp = 0;
            if(temp == 5)
            {
                cout << endl << "downloaded task num:" << slave.downloadedTaskNum << endl;
            }
            temp++;
            sleep(1);
            continue;
        }
        if(slave.runFlag == false)
        {
            continue;
        }

        cout << endl << " unexecuted task num:" << slave.unexecutedTaskNum << endl;
        if(temp == slave.task)
        {
            temp = temp->next;
        }
        SubTaskNode *node = list_entry(temp, SubTaskNode, self);
        int count = 0;      //计数，防止所有待执行子任务都需等待其他子任务结果时陷入死循环
        while(count < slave.unexecutedTaskNum && (node->cprev_num > 0 || node->exe_flag == false))
        {
            temp = temp->next;
            if(temp == slave.task)
            {
                temp = temp->next;
            }
            node = list_entry(temp, SubTaskNode, self);
            count++;
        }
        if(count == slave.unexecutedTaskNum)
        {
            //循环一圈并未找到可以执行的子任务
            printf("no available task\n");
            sleep(1);
            continue;
        }
        //找到可执行子任务的节点node
        //temp后移一位避免任务执行完成后释放node节点导致temp指向未知值
        temp = temp->next;

        //执行子任务========================
        printf("root:%d subtask:%d processing...\n", node->root_id, node->subtask_id);
        stringstream ss;
        // //传可执行文件
        // ss << "docker cp " << node->exepath << " " << slave.containerID << ":/home/task/";
        // ret = system(ss.str().c_str());
        // printf("root:%d subtask:%d exe file transmission complete...\n", node->root_id, node->subtask_id);
        // //传前驱文件
        // int num = 0;
        // SubTaskResult *temp = node->prev_head->next;
        // while (num < node->prev_num)
        // {
        //     ss.str("");
        //     ss << "docker cp " << temp->fname << " " << slave.containerID << ":/home/task/";
        //     ret = system(ss.str().c_str());
        //     temp = temp->next;
        //     num++;
        // }
        // printf("root:%d subtask:%d prev file transmission complete...\n", node->root_id, node->subtask_id);
        //执行子任务
        ss.str("");
        ss << "./" << node->exepath;
        auto currenttime = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(currenttime);
        auto timestampMS = std::chrono::duration_cast<std::chrono::milliseconds>(currenttime.time_since_epoch()).count();
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));
        std::cout << "start timestampMS: " << timestampMS << std::endl;
        std::cout << "start timestamp: " << buffer << std::endl;

        system(ss.str().c_str());
        currenttime = std::chrono::system_clock::now();
        timestamp = std::chrono::system_clock::to_time_t(currenttime);
        timestampMS = std::chrono::duration_cast<std::chrono::milliseconds>(currenttime.time_since_epoch()).count();
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));
        std::cout << "end   timestampMS: " << timestampMS << std::endl;
        std::cout << "end   timestamp: " << buffer << std::endl;

        // ss.str("");
        // ss << "docker exec -w /home/task -d " << slave.containerID << " ./" << node->exepath;
        // ret = system(ss.str().c_str());
        // //查询子任务运行状态
        // ss.str("");
        // ss << "docker top " << slave.containerID << " > " << logname;
        // bool flag = false;
        // ifstream ifs;
        // do{
        //     flag = false;
        //     ret = system(ss.str().c_str());
        //     cout << "ret: " << ret << endl;
        //     ifs.open(logname);
        //     if(!ifs.good())
        //     {
        //         perror("log file open error");
        //         return;
        //     }
        //     string line;
        //     while(getline(ifs, line))
        //     {
        //         if(line.find(node->exepath) != string::npos)
        //         {
        //             flag = true;
        //             break;
        //         }
        //     }
        //     ifs.close();
        //     sleep(2);
        // }while(flag);
        printf("root:%d subtask:%d exe done...\n", node->root_id, node->subtask_id);
        //获取后继文件
        // ss.str("");
        // temp = node->succ_head->next;
        // num = 0;
        // while (num < node->next_num)
        // {
        //     ss.str("");
        //     ss << "docker cp " << slave.containerID << ":/home/task/" << temp->fname << " ./" << temp->fname;
        //     ret = system(ss.str().c_str());
        //     temp = temp->next;
        //     num++;
        // }
        // printf("root:%d subtask:%d succ file get...\n", node->root_id, node->subtask_id);
        // //删除容器内子任务执行相关文件
        // ss.str("");
        // ss << "docker exec -w /home " << slave.containerID << " ./rm.sh";
        // ret = system(ss.str().c_str());
        // printf("root:%d subtask:%d remove processing file...\n", node->root_id, node->subtask_id);

        //发送任务执行结果给后继
        SubTaskResult *tres;
        int sendnum = node->next_num;
        for(int i = 0; i < sendnum; i++)
        {
            tres = node->succ_head->next;
            //跳过后继任务在同一台设备的情况
            if(tres->client_id == slave.slave_id)
            {
                //找到子任务列表中对应的子任务节点，更新其前驱输入文件信息，释放后继输出文件对应资源
                subtask_input_update(node->root_id, tres->subtask_id, tres->fname);
                node->succ_head->next = tres->next;
                delete tres;
                tres = node->succ_head->next;
                node->next_num--;
                continue;
            }
            int count = 0;
            int connsock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            cout << "tres->client_id:" << tres->client_id << endl;
            map<int, struct sockaddr_in*>::iterator it = slave.work_slave_addr.find(tres->client_id);
            addr.sin_port = it->second->sin_port;
            addr.sin_addr.s_addr = it->second->sin_addr.s_addr;
            //暂时不对连接失败无法传送的文件进行处理======****
            while(count < MAX_CONN_COUNT)
            {
                int ret = connect(connsock, (struct sockaddr*)&addr, sizeof(addr));
                if(ret == 0)
                {
                    break;
                }
                count++;
            }

            //建立客户端发送/接收线程发送结果文件
            struct sockaddr_in localaddr;
            socklen_t addrlen = sizeof(localaddr);
            getsockname(connsock, (struct sockaddr *)&localaddr, &addrlen);
            PeerNode *peer = new PeerNode();
            peer->client_id = tres->client_id;
            peer->sock = connsock;
            peer->addr.sin_family = addr.sin_family;
            peer->addr.sin_addr.s_addr = addr.sin_addr.s_addr;
            peer->addr.sin_port = addr.sin_port;
            peer->local_port = ntohs(localaddr.sin_port);
            peer->self = LIST_HEAD_INIT(peer->self);
            peer->head = &peer_list;
            list_add_tail(&peer->self, peer->head);
            peer->status = PEER_STATUS_C_FILESEND_SEND_REQ;
            peer->current_file_trans_info = new FileTransInfo();
            peer->current_file_trans_info->base64flag = true;
            FileInfoInit(&peer->current_file_trans_info->info);
            FileInfoGet(tres->fname, &peer->current_file_trans_info->info);
            peer->current_file_trans_info->dst_rootid = node->root_id;
            peer->current_file_trans_info->dst_subtaskid = tres->subtask_id;
            peer->current_file_trans_info->file_type = FILE_TYPE_INPUT;
            if(peer->current_file_trans_info->info.exatsize > 10240)
            {
                peer->current_file_trans_info->splitflag = true;
                peer->current_file_trans_info->packsize = FILE_PACKAGE_SIZE;
                peer->current_file_trans_info->packnum = peer->current_file_trans_info->info.exatsize / ((FILE_PACKAGE_SIZE * 3) / 4);
            }
            else
            {
                peer->current_file_trans_info->splitflag = false;
            }
            peer->msg_send_threadID_C = std::thread(peerC_msg_send, peer);
            peer->msg_recv_threadID_C = std::thread(peerC_msg_recv, peer);
            if(peer->msg_recv_threadID_C.joinable())
            {
                peer->msg_recv_threadID_C.join();
            }
            if(peer->msg_send_threadID_C.joinable())
            {
                peer->msg_send_threadID_C.join();
            }
            //释放peer资源
            close(peer->sock);
            list_del(&peer->self);
            delete peer->current_file_trans_info;
            delete peer;

            //释放tres
            node->succ_head->next = tres->next;
            delete tres;
            node->next_num--;
        }
        delete node->succ_head;

        //向主节点通报子任务执行情况
        Json::Value root;
        root["type"] = Json::Value(MSG_TYPE_SUBTASK_RESULT);
	    root["slaveID"] = Json::Value(slave.slave_id);
        root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
        root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
        root["root_id"] = Json::Value(node->root_id);
        root["subtask_id"] = Json::Value(node->subtask_id);
        root["ret"] = Json::Value(true);

        Json::FastWriter fw;
        ss.str("");
        ss << fw.write(root);
        send(slave.sock, ss.str().c_str(), ss.str().length(), 0);

        //删除任务列表中的子任务节点，并释放子任务节点node的资源
        list_del(&node->self);
        SubTaskResult *rtemp;
        rtemp = node->prev_head->next;
        while(rtemp != NULL)
        {
            node->prev_head->next = rtemp->next;
            delete rtemp;
            rtemp = node->prev_head->next;
        }
        delete node->prev_head;
        delete node;
        slave.unexecutedTaskNum--;
        if(slave.unexecutedTaskNum == 0)
        {
            slave.runFlag = false;
            slave.downloadedTaskNum = 0;
            slave.task_num = 0;
            std::cout << "all subtasks done!" << std::endl;
        }
    }
}

int main()
{
    int sock = startup();
    thread slave_listen_threadID(slave_accept, sock);
    thread file_req_threadID(file_req);
    thread subtask_run_threadID(subtask_run);
    if(slave_listen_threadID.joinable())
    {
        slave_listen_threadID.join();
    }
    if(file_req_threadID.joinable())
    {
        file_req_threadID.join();
    }
    if(subtask_run_threadID.joinable())
    {
        subtask_run_threadID.join();
    }
    if(slave.master_msg_send_threadID.joinable())
    {
        slave.master_msg_send_threadID.join();
    }
    if(slave.master_msg_recv_threadID.joinable())
    {
        slave.master_msg_recv_threadID.join();
    }
    
    return 0;
}
