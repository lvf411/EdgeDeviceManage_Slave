#include "msg.hpp"

#define ListenPortStart 10000
#define ListenPortEnd   10100

extern Slave slave;
extern char SizeUnit[5];
extern list_head file_req_list;

//获取随机的可用端口
int getRandAvaliPort()
{
    int randport;
    char get_port_cmd[100];
    int count = 0;
    //最多尝试15次
    while(count < 15)
    {
        randport = rand() % (ListenPortEnd - ListenPortStart + 1) + ListenPortStart;
        sprintf(get_port_cmd, "netstat -an | grep :%d > /dev/null", randport);
        if(system(get_port_cmd))
        {
            //若端口被占用，system返回0,；若未被占用，system返回256
            return randport;
        }
        count++;
    }
    return -1;
}

//检查文件是否为关键文件，若是则解包并更新系统信息
bool key_file_check(std::string fname)
{
    static std::string slave_subtask_sync_fname;
    static bool initflag = false;

    if(!initflag)
    {
        std::ofstream f;
        std::stringstream ss;
        ss << slave.slave_id << "_subtask_list.json";
        slave_subtask_sync_fname = ss.str();
        initflag = true;
    }

    if(fname.compare(slave_subtask_sync_fname) == 0)
    {
        client_task_list_descfile_parse_and_update(fname);
    }
    else
    {
        return false;
    }
    return true;
}

//为文件赋予可执行权限
void ExePermissionGrant(std::string fname)
{
    std::string cmd = "sudo chmod 777 " + fname;
    system(cmd.c_str());
}

//收到主节点传来的子任务执行文件后将子任务描述节点中的exe存在标志置位
void exeflag_update(int root_id, int subtask_id)
{
    list_head *temp = slave.task->next;
    while(temp != slave.task)
    {
        SubTaskNode *node = list_entry(temp, SubTaskNode, self);
        if(node->root_id == root_id)
        {
            if(node->subtask_id == subtask_id)
            {
                node->exe_flag =true;
                break;
            }
        }
        temp = temp->next;
    }
}

void subtask_input_update(int root_id, int subtask_id, std::string fname)
{
    list_head *temp = slave.task->next;
    while(temp != slave.task)
    {
        SubTaskNode *node = list_entry(temp, SubTaskNode, self);
        if(node->root_id == root_id)
        {
            if(node->subtask_id == subtask_id)
            {
                SubTaskResult *tret = node->prev_head;
                while(tret->next != NULL)
                {
                    if(tret->next->fname.compare(fname) == 0)
                    {
                        node->cprev_num--;
                        return;
                    }
                    tret = tret->next;
                }
            }
        }
        temp = temp->next;
    }
}

//接收文件传输socket连接线程
void file_trans_socket_accept(int instruction_sock, int listen_sock, FileTransInfo *current_file_trans_info, int &status, bool &file_trans_flag)
{
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int connect_sock;
    while(1)
    {
        connect_sock = accept(listen_sock, (struct sockaddr *)&client, &len);
        if(connect_sock >= 0)
        {
            //成功接收文件传输发送方连接
            //打开文件，接收数据，若文件存在则先清空文件内容
            if(isFileExists(current_file_trans_info->info.fname))
            {
                std::ofstream ofstrunc(current_file_trans_info->info.fname, std::ios::trunc);
                ofstrunc.close();
            }
            std::ofstream ofs(current_file_trans_info->info.fname, std::ios::binary | std::ios::app);
            std::string res_md5;
            std::thread file_recv_threadID(file_recv, connect_sock, &(current_file_trans_info->info), std::ref(ofs), std::ref(res_md5));
            //发送开始传输请求
            Json::Value root;
            root["type"] = Json::Value(MSG_TYPE_FILESEND_START);
            root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
            root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
            Json::FastWriter fw;
            std::stringstream ss;
            ss << fw.write(root);
            send(instruction_sock, ss.str().c_str(), ss.str().length(), 0);
            std::cout << ss.str() << std::endl;
            root.clear();

            //等待文件传输结束
            if(file_recv_threadID.joinable())
            {
                file_recv_threadID.join();
            }
            if(res_md5.compare(current_file_trans_info->info.md5) != 0)
            {
                //md5检测，文件内容有误
                //清空文件
                std::ofstream ofstrunc(current_file_trans_info->info.fname, std::ios::trunc);
                ofstrunc.close();
                std::ofstream ofsrewrite(current_file_trans_info->info.fname, std::ios::binary | std::ios::app);
                res_md5.clear();
                //std::thread file_recv_threadID(file_recv, connect_sock, &(current_file_trans_info->info), std::ref(ofsrewrite), std::ref(res_md5));
                //发送重传请求
                root["type"] = Json::Value(MSG_TYPE_FILESEND_RES);
                root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
                root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
                root["res"] = Json::Value(false);
                root["resend"] = Json::Value(true);
                root["fname"] = Json::Value(current_file_trans_info->info.fname);
                ss.str("");
                ss << fw.write(root);
                send(instruction_sock, ss.str().c_str(), ss.str().length(), 0);
                std::cout << ss.str() << std::endl;
                file_recv_threadID.join();
                close(connect_sock);
                close(listen_sock);
                status = SLAVE_STATUS_ORIGINAL;
                file_trans_flag = false;
                return;
            }
            //md5检测，文件内容正确
            //发送确认消息
            root["type"] = Json::Value(MSG_TYPE_FILESEND_RES);
            root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
            root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
            root["res"] = Json::Value(true);
            ss.str("");
            ss << fw.write(root);
            send(instruction_sock, ss.str().c_str(), ss.str().length(), 0);
            std::cout << ss.str() << std::endl;
            //关闭文件传输连接文件描述符
            close(connect_sock);
            close(listen_sock);
            status = SLAVE_STATUS_ORIGINAL;
            file_trans_flag = false;

            //检查文件类型，响应做出更新
            switch(current_file_trans_info->file_type)
            {
                case FILE_TYPE_WORK_CLIENT_LIST:
                {
                    work_client_list_descfile_parse_and_update(current_file_trans_info->info.fname);
                    break;
                }
                case FILE_TYPE_KEY:
                {
                    //为关键文件，若是则解包并更新系统信息
                    key_file_check(current_file_trans_info->info.fname);
                    break;
                }
                case FILE_TYPE_EXE:
                {
                    //为文件赋予可执行权限
                    ExePermissionGrant(current_file_trans_info->info.fname);
                    //主节点传来的执行文件，记录对应子任务的 exe_flag
                    exeflag_update(current_file_trans_info->dst_rootid, current_file_trans_info->dst_subtaskid);
                    break;
                }
                case FILE_TYPE_INPUT:
                {
                    //其他从节点传来的子任务执行输入文件，更新对应子任务执行需要等待的输入信息
                    subtask_input_update(current_file_trans_info->dst_rootid, current_file_trans_info->dst_subtaskid, current_file_trans_info->info.fname);
                    break;
                }
                default:
                {
                    //其他
                    break;
                }
            }
            return;
        }
    }
}

void msg_send()
{
    while(1)
    {
        switch (slave.status)
        {
            case SLAVE_STATUS_ORIGINAL:
            {
                sleep(1);
                break;
            }
            case SLAVE_STATUS_FILERECV_REQ_RECV:
            {
                //获取待接收文件的信息后获取空闲端口，建立监听
                int randport = getRandAvaliPort();
                bool file_trans_allow_flag = true;        //发送同意请求标志
                if(randport < 0)
                {
                    //获取随机端口失败，不接收文件
                    file_trans_allow_flag = false;
                }
                else
                {
                    //获取随机端口成功，先建立listen
                    slave.file_trans_listen_sock = socket(AF_INET,SOCK_STREAM,0);
                    if(slave.file_trans_listen_sock < 0){
                        perror("socket");
                        exit(1);
                    }

                    struct sockaddr_in local;
                    local.sin_family = AF_INET;
                    local.sin_port = htons(randport);
                    local.sin_addr.s_addr = slave.addr.sin_addr.s_addr;
                    //closesocket 后不经历 TIME_WAIT 的过程，继续重用该socket
                    bool bReuseaddr=true;
                    setsockopt(slave.file_trans_listen_sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr));
                    
                    if(bind(slave.file_trans_listen_sock,(struct sockaddr*)&local,sizeof(local)) < 0)
                    {
                        perror("bind");
                        exit(2);
                    }
                    if(listen(slave.file_trans_listen_sock, 1000) < 0)
                    {
                        perror("listen");
                        exit(3);
                    }
                    //创建线程接收来自主节点的连接
                    slave.file_trans_threadID = std::thread(file_trans_socket_accept, slave.sock, slave.file_trans_listen_sock, slave.current_file_trans_info, std::ref(slave.status), std::ref(slave.file_trans_flag));
                    //可以接收文件
                    file_trans_allow_flag = true;
                }
                Json::Value root;
                root["type"] = Json::Value(MSG_TYPE_FILESEND_REQ_ACK);
                root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
                root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
                root["fname"] = Json::Value(slave.current_file_trans_info->info.fname);
                if(file_trans_allow_flag == false)
                {
                    root["ret"] = Json::Value(false);
                    Json::FastWriter fw;
                    std::stringstream ss;
                    ss << fw.write(root);
                    send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
                    std::cout << ss.str() << std::endl;
                    slave.status = SLAVE_STATUS_ORIGINAL;
                }
                else
                {
                    //发送消息通知主节点
                    root["ret"] = Json::Value(true);
                    root["listen_port"] = Json::Value(randport);
                    Json::FastWriter fw;
                    std::stringstream ss;
                    ss << fw.write(root);
                    send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
                    std::cout << ss.str() << std::endl;
                    slave.status = SLAVE_STATUS_FILERECV_WAIT_CONN;
                    if(slave.file_trans_threadID.joinable())
                    {
                        slave.file_trans_threadID.join();
                    }
                }
                break;
            }
            default:
                //休眠100ms
                usleep(100000);
                break;
        }
    }
}

void msg_recv()
{
    char recvbuf[MSG_BUFFER_SIZE] = {0};
    //先获取主节点分配的从节点ID
    recv(slave.sock, recvbuf, MSG_BUFFER_SIZE, 0);
    std::cout << "recv: " <<  recvbuf << std::endl;
    Json::Value root;
    Json::Reader rd;
    rd.parse(recvbuf, root);
    slave.slave_id = root["slaveID"].asInt();
    while(1)
    {
        memset(recvbuf, 0, MSG_BUFFER_SIZE);
        recv(slave.sock, recvbuf, MSG_BUFFER_SIZE, 0);
        std::cout << "recv: " <<  recvbuf << std::endl;
        Json::Value root;
        Json::Reader rd;
        rd.parse(recvbuf, root);

        int msg_type = root["type"].asInt();
        switch(msg_type)
        {
            case MSG_TYPE_FILESEND_REQ:
            {
                //文件发送请求附带文件信息
                if(slave.file_trans_flag == false)
                {
                    slave.file_trans_flag = true;
                    slave.current_file_trans_info->info.fname.clear();
                    slave.current_file_trans_info->info.md5.clear();
                    slave.current_file_trans_info->info.fname = root["fname"].asString();
                    slave.current_file_trans_info->info.exatsize = root["exatsize"].asUInt();
                    uint temp = root["exatsize"].asUInt();
                    int unit = 0;
                    while(temp > 10000)
                    {
                        temp /= 1000;
                        unit++;
                    }
                    slave.current_file_trans_info->info.filesize = temp;
                    slave.current_file_trans_info->info.unit = SizeUnit[unit];
                    slave.current_file_trans_info->info.md5 = root["md5"].asString();
                    slave.current_file_trans_info->base64flag = root["base64"].asBool();
                    slave.current_file_trans_info->splitflag = root["split"].asBool();
                    slave.current_file_trans_info->file_type = root["file_type"].asInt();
                    slave.current_file_trans_info->dst_rootid = root["dst_rootid"].asInt();
                    slave.current_file_trans_info->dst_subtaskid = root["dst_subtaskid"].asInt();
                    if(slave.current_file_trans_info->splitflag == true)
                    {
                        slave.current_file_trans_info->packnum = root["pack_num"].asInt();
                        slave.current_file_trans_info->packsize = root["pack_size"].asInt();
                    }
                    slave.status = SLAVE_STATUS_FILERECV_REQ_RECV;
                }
                else
                {
                    break;
                }
                break;
            }
            case MSG_TYPE_FILEREQ_ACK:
            {
                int ret = root["ret"].asInt();
                switch(ret)
                {
                    case FILEREQ_ACK_OK:
                    {
                        //主节点同意文件请求，不需要做什么，主节点会自动发起传送文件请求
                        break;
                    }
                    case FILEREQ_ACK_WAIT:
                    {
                        //主节点正忙，等待后重新请求，将文件请求重新加入请求链表中
                        FileReqNode *req_node = new FileReqNode();
                        req_node->fname = root["fname"].asString();
                        req_node->rootid = root["rootid"].asInt();
                        req_node->subtaskid = root["subtaskid"].asInt();
                        req_node->self = LIST_HEAD_INIT(req_node->self);
                        req_node->head = &file_req_list;
                        list_add_tail(&req_node->self, req_node->head);
                        break;
                    }
                    case FILEREQ_ACK_ERROR:
                    {
                        //主节点处没有请求的目标文件，发生错误
                        perror("filereq: master doesnot have the target file\n");
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            default:
            {
                break;
            }
        }
    }
}

void peerS_msg_send(PeerNode *peer)
{
    while(1)
    {
        switch (peer->status)
        {
            case PEER_STATUS_S_ORIGINAL:
            {
                sleep(1);
                break;
            }
            case PEER_STATUS_S_FILERECV_REQ_RECV:
            {
                //获取待接收文件的信息后获取空闲端口，建立监听
                peer->file_trans_port = getRandAvaliPort();
                bool file_trans_allow_flag = true;        //发送同意请求标志
                if(peer->file_trans_port < 0)
                {
                    //获取随机端口失败，不接收文件
                    file_trans_allow_flag = false;
                }
                else
                {
                    //获取随机端口成功，先建立listen，此处为接收端， file_trans_sock 为监听sock
                    peer->file_trans_sock = socket(AF_INET,SOCK_STREAM,0);
                    if(peer->file_trans_sock < 0){
                        perror("socket");
                        exit(1);
                    }

                    struct sockaddr_in local;
                    local.sin_family = AF_INET;
                    local.sin_port = htons(peer->file_trans_port);
                    local.sin_addr.s_addr = slave.addr.sin_addr.s_addr;
                    //closesocket 后不经历 TIME_WAIT 的过程，继续重用该socket
                    bool bReuseaddr=true;
                    setsockopt(peer->file_trans_sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr));
                    
                    if(bind(peer->file_trans_sock,(struct sockaddr*)&local,sizeof(local)) < 0)
                    {
                        perror("bind");
                        exit(2);
                    }
                    std::cout << peer->file_trans_sock << std::endl;
                    std::cout << inet_ntoa(local.sin_addr) << ":" << ntohs(local.sin_port) << std::endl;
                    if(listen(peer->file_trans_sock, 1000) < 0)
                    {
                        perror("listen");
                        exit(3);
                    }
                    //创建线程接收来自对方的连接
                    peer->file_trans_threadID = std::thread(file_trans_socket_accept, peer->sock, peer->file_trans_sock, peer->current_file_trans_info, std::ref(peer->status), std::ref(peer->file_trans_flag));
                    //可以接收文件
                    file_trans_allow_flag = true;
                }
                Json::Value root;
                root["type"] = Json::Value(MSG_TYPE_FILESEND_REQ_ACK);
                root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
                root["src_port"] = Json::Value(slave.listen_port);
                root["fname"] = Json::Value(peer->current_file_trans_info->info.fname);
                if(file_trans_allow_flag == false)
                {
                    root["ret"] = Json::Value(false);
                    Json::FastWriter fw;
                    std::stringstream ss;
                    ss << fw.write(root);
                    send(peer->sock, ss.str().c_str(), ss.str().length(), 0);
                    std::cout << ss.str() << std::endl;
                    peer->status = PEER_STATUS_S_ORIGINAL;
                }
                else
                {
                    //发送消息通知对方
                    root["ret"] = Json::Value(true);
                    root["listen_port"] = Json::Value(peer->file_trans_port);
                    Json::FastWriter fw;
                    std::stringstream ss;
                    ss << fw.write(root);
                    send(peer->sock, ss.str().c_str(), ss.str().length(), 0);
                    std::cout << ss.str() << std::endl;
                    peer->status = PEER_STATUS_S_FILERECV_WAIT_CONN;
                    if(peer->file_trans_threadID.joinable())
                    {
                        peer->file_trans_threadID.join();
                        close(peer->sock);
                        list_del(&peer->self);
                        delete peer->current_file_trans_info;
                        delete peer;
                        return;
                    }
                }
                break;
            }
            default:
            {
                //休眠100ms
                usleep(100000);
                break;
            }
        }
    }
}

void peerS_msg_recv(PeerNode *peer)
{
    char recvbuf[MSG_BUFFER_SIZE] = {0};
    while(1)
    {
        memset(recvbuf, 0, MSG_BUFFER_SIZE);
        recv(peer->sock, recvbuf, MSG_BUFFER_SIZE, 0);
        std::cout << "recv: " <<  recvbuf << std::endl;
        Json::Value root;
        Json::Reader rd;
        rd.parse(recvbuf, root);
        
        switch (root["type"].asInt())
        {
            case MSG_TYPE_FILESEND_REQ:
            {
                //文件发送请求附带文件信息
                if(peer->file_trans_flag == false)
                {
                    peer->file_trans_flag = true;
                    peer->current_file_trans_info->info.fname.clear();
                    peer->current_file_trans_info->info.md5.clear();
                    peer->current_file_trans_info->info.fname = root["fname"].asString();
                    peer->current_file_trans_info->info.exatsize = root["exatsize"].asUInt();
                    uint temp = root["exatsize"].asUInt();
                    int unit = 0;
                    while(temp > 10000)
                    {
                        temp /= 1000;
                        unit++;
                    }
                    peer->current_file_trans_info->info.filesize = temp;
                    peer->current_file_trans_info->info.unit = SizeUnit[unit];
                    peer->current_file_trans_info->info.md5 = root["md5"].asString();
                    peer->current_file_trans_info->base64flag = root["base64"].asBool();
                    peer->current_file_trans_info->dst_rootid = root["dst_rootid"].asInt();
                    peer->current_file_trans_info->dst_subtaskid = root["dst_subtaskid"].asInt();
                    peer->current_file_trans_info->file_type = root["file_type"].asInt();
                    peer->current_file_trans_info->splitflag = root["split"].asBool();
                    if(peer->current_file_trans_info->splitflag == true)
                    {
                        peer->current_file_trans_info->packnum = root["pack_num"].asInt();
                        peer->current_file_trans_info->packsize = root["pack_size"].asInt();
                    }
                    peer->status = PEER_STATUS_S_FILERECV_REQ_RECV;
                    return;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

void peerC_msg_send(PeerNode *peer)
{
    while(1)
    {
        switch (peer->status)
        {
            case PEER_STATUS_C_FILESEND_SEND_REQ:
            {
                Json::Value root;
                root["type"] = Json::Value(MSG_TYPE_FILESEND_REQ);
                root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
                root["src_port"] = Json::Value(peer->local_port);
                root["fname"] = Json::Value(peer->current_file_trans_info->info.fname);
                root["exatsize"] = Json::Value(peer->current_file_trans_info->info.exatsize);
                root["md5"] = Json::Value(peer->current_file_trans_info->info.md5);
                //开启base64转码
                root["base64"] = Json::Value(true);
                root["dst_rootid"] = Json::Value(peer->current_file_trans_info->dst_rootid);
                root["dst_subtaskid"] = Json::Value(peer->current_file_trans_info->dst_subtaskid);
                root["file_type"] = Json::Value(peer->current_file_trans_info->file_type);
                if(peer->current_file_trans_info->info.exatsize > (FILE_PACKAGE_SIZE / 4) * 3)
                {
                    //文件大小大于单个包长度，需进行拆包发送
                    root["split"] = Json::Value(true);
                    root["pack_num"] = Json::Value(peer->current_file_trans_info->info.exatsize / ((FILE_PACKAGE_SIZE * 3) / 4) + 1);
                    root["pack_size"] = Json::Value(FILE_PACKAGE_SIZE);
                }
                else
                {
                    //文件大小小于单个包长度，不需要拆包发送
                    root["split"] = Json::Value(false);
                }

                //生成字符串
                Json::FastWriter fw;
                std::stringstream ss;
                ss << fw.write(root);
                send(peer->sock, ss.str().c_str(), ss.str().length(), 0);
                std::cout << ss.str() << std::endl;
                peer->status = PEER_STATUS_C_FILESEND_WAIT_ACK;
                break;
            }
            case PEER_STATUS_C_FILESEND_CONNECT:
            {
                peer->file_trans_sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = peer->addr.sin_addr.s_addr;
                addr.sin_port = htons(peer->file_trans_port);
                int count = 0;
                while(count < 10)
                {
                    count++;
                    int ret = connect(peer->file_trans_sock, (struct sockaddr *)&addr, sizeof(struct sockaddr));
                    if(ret == 0)
                    {
                        break;
                    }
                    if(ret != 0)
                    {
                        perror("connect error:");
                    }
                }
                if(count >= 10)
                {
                    printf("file send: failed to connect to the target port\n");
                    Json::Value root;
                    root["type"] = Json::Value(MSG_TYPE_FILESEND_CANCEL);
                    root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
                    root["src_port"] = Json::Value(ntohs(peer->local_port));

                    //生成字符串
                    Json::FastWriter fw;
                    std::stringstream ss;
                    ss << fw.write(root);
                    send(peer->sock, ss.str().c_str(), ss.str().length(), 0);
                    std::cout << ss.str() << std::endl;
                    pthread_cancel(peer->msg_recv_threadID_C.native_handle());
                    return;
                }
                peer->sem.Wait();
                break;
            }
            case PEER_STATUS_C_FILESEND_SENDFILE:
            {
                peer->file_trans_threadID = std::thread(file_send, peer->file_trans_sock, peer->current_file_trans_info->info.fname);
                peer->file_trans_threadID.join();
                peer->sem.Wait();
                if(peer->status == PEER_STATUS_C_FILESEND_SENDFILE)
                {
                    continue;
                }
                close(peer->file_trans_sock);
                return;
            }
            default:
            {
                //休眠100ms
                usleep(100000);
                break;
            }
        }
    }
}

void peerC_msg_recv(PeerNode *peer)
{
    char recvbuf[MSG_BUFFER_SIZE] = {0};
    while(1)
    {
        memset(recvbuf, 0, MSG_BUFFER_SIZE);
        recv(peer->sock, recvbuf, MSG_BUFFER_SIZE, 0);
        std::cout << "recv: " << recvbuf << std::endl;
        Json::Value root;
        Json::Reader rd;
        rd.parse(recvbuf, root);

        switch (root["type"].asInt())
        {
            case MSG_TYPE_FILESEND_REQ_ACK:
            {
                bool ret = root["ret"].asBool();
                if(ret == true)
                {
                    peer->file_trans_port = root["listen_port"].asInt();
                    peer->status = PEER_STATUS_C_FILESEND_CONNECT;
                }
                else
                {
                    pthread_cancel(peer->msg_send_threadID_C.native_handle());
                    return;
                }
                
                break;
            }
            case MSG_TYPE_FILESEND_START:
            {
                peer->status = PEER_STATUS_C_FILESEND_SENDFILE;
                peer->sem.Signal();
                break;
            }
            case MSG_TYPE_FILESEND_RES:
            {
                int res = root["res"].asBool();
                if(res == false)
                {
                    peer->status = PEER_STATUS_C_FILESEND_SENDFILE;
                    peer->sem.Signal();
                    break;
                }
                else
                {
                    peer->status = PEER_STATUS_C_ROOT;
                    peer->sem.Signal();
                    return;
                }
            }
            default:
            {
                break;
            }
        }
    }
}
