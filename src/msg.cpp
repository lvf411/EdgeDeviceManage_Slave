#include "msg.hpp"

#define ListenPortStart 10000
#define ListenPortEnd   10020

extern Slave slave;
FileTransInfo current_file_trans_info;
int file_recv_status = FILE_RECV_STATUS_NONE;
extern char SizeUnit[5];

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

//接收文件传输socket连接线程
void file_trans_socket_accept()
{
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    while(1)
    {
        slave.file_trans_connect_sock = accept(slave.file_trans_listen_sock, (struct sockaddr *)&client, &len);
        if(slave.file_trans_connect_sock >= 0)
        {
            //成功接收文件传输发送方连接
            //打开文件，接收数据
            std::ofstream ofs(current_file_trans_info.info.fname, std::ios::binary | std::ios::app);
            std::string res_md5;
            std::thread file_recv_threadID(file_recv, slave.file_trans_connect_sock, current_file_trans_info.info, ofs, std::ref(res_md5));
            //发送开始传输请求
            Json::Value root;
            root["type"] = Json::Value(MSG_TYPE_FILESEND_START);
            root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
            root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
            Json::FastWriter fw;
            std::stringstream ss;
            ss << fw.write(root);
            send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
            root.clear();
            //等待文件传输结束
            file_recv_threadID.join();
            while(res_md5.compare(current_file_trans_info.info.md5) != 0)
            {
                //md5检测，文件内容有误
                //清空文件
                std::ofstream ofstrunc(current_file_trans_info.info.fname, std::ios::trunc);
                ofstrunc.close();
                std::ofstream ofsrewrite(current_file_trans_info.info.fname, std::ios::binary | std::ios::app);
                res_md5.clear();
                std::thread file_recv_threadID(file_recv, slave.file_trans_connect_sock, current_file_trans_info.info, ofsrewrite, std::ref(res_md5));
                //发送重传请求
                root["type"] = Json::Value(MSG_TYPE_FILESEND_RES);
                root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
                root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
                root["res"] = Json::Value(false);
                root["resend"] = Json::Value(true);
                root["fname"] = Json::Value(current_file_trans_info.info.fname);
                ss.str("");
                ss << fw.write(root);
                send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
                file_recv_threadID.join();
            }
            //md5检测，文件内容正确
            //发送确认消息
            root["type"] = Json::Value(MSG_TYPE_FILESEND_RES);
            root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
            root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
            root["res"] = Json::Value(true);
            ss.str("");
            ss << fw.write(root);
            send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
            //关闭文件传输监听与连接文件描述符
            close(slave.file_trans_connect_sock);
            slave.file_trans_connect_sock = -1;
            close(slave.file_trans_listen_sock);
            slave.file_trans_listen_sock = -1;
            slave.status = SLAVE_STATUS_ORIGINAL;
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
            bool flag = true;        //发送同意请求标志
            if(randport < 0)
            {
                //获取随机端口失败
                flag = false;
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

                flag = true;
            }
            Json::Value root;
            root["type"] = Json::Value(MSG_TYPE_FILESEND_REQ_ACK);
            root["src_ip"] = Json::Value(inet_ntoa(slave.addr.sin_addr));
            root["src_port"] = Json::Value(ntohs(slave.addr.sin_port));
            if(flag == false)
            {
                root["ret"] = Json::Value(false);
                Json::FastWriter fw;
                std::stringstream ss;
                ss << fw.write(root);
                send(slave.sock, ss.str().c_str(), ss.str().length(), 0);
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
                slave.status = SLAVE_STATUS_FILERECV_WAIT_CONN;
            }
            break;
        }
        default:
            break;
        }
    }
}

void msg_recv()
{
    char recvbuf[MSG_BUFFER_SIZE] = {0};
    while(1)
    {
        memset(recvbuf, 0, MSG_BUFFER_SIZE);
        recv(slave.sock, recvbuf, MSG_BUFFER_SIZE, 0);
        Json::Value root;
        Json::Reader rd;
        rd.parse(recvbuf, root);

        int msg_type = root["type"].asInt();
        switch(msg_type)
        {
            case MSG_TYPE_FILESEND_REQ:
            {
                //文件发送请求附带文件信息
                if(file_recv_status == FILE_RECV_STATUS_NONE)
                {
                    file_recv_status = FILE_RECV_STATUS_RECVING;
                    current_file_trans_info.info.fname.clear();
                    current_file_trans_info.info.md5.clear();
                    current_file_trans_info.info.fname = root["fname"].asString();
                    current_file_trans_info.info.exatsize = root["exatsize"].asInt64();
                    long long int temp = root["exatsize"].asInt64();
                    int unit = 0;
                    while(temp > 10000)
                    {
                        temp /= 1000;
                        unit++;
                    }
                    current_file_trans_info.info.filesize = temp;
                    current_file_trans_info.info.unit = SizeUnit[unit];
                    current_file_trans_info.info.md5 = root["md5"].asString();
                    current_file_trans_info.base64flag = root["base64"].asBool();
                    current_file_trans_info.splitflag = root["split"].asBool();
                    if(current_file_trans_info.splitflag == true)
                    {
                        current_file_trans_info.packnum = root["pack_num"].asInt();
                        current_file_trans_info.packsize = root["pack_size"].asInt();
                    }
                    slave.status = SLAVE_STATUS_FILERECV_REQ_RECV;
                }
                else
                {
                    break;
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
