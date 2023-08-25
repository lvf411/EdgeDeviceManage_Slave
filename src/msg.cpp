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
    while(1)
    {
        randport = rand() % (ListenPortEnd - ListenPortStart + 1) + ListenPortStart;
        sprintf(get_port_cmd, "netstat -an | grep :%d > /dev/null", randport);
        if(system(get_port_cmd))
        {
            //若端口被占用，system返回0,；若未被占用，system返回256
            break;
        }
    }
    return randport;
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

                //获取待接收文件的信息后获取空闲端口，建立监听

                break;
            }
            default:
            {
                break;
            }
        }
    }
}
