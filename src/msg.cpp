#include "msg.hpp"

extern Slave slave;

void msg_send()
{
    while(1)
    {

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
            default:
            {
                break;
            }
        }
    }
}
