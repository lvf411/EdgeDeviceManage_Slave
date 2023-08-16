#ifndef __MSG_TYPE_HPP
#define __MSG_TYPE_HPP

//发送内容为文件传输请求
#define MSG_TYPE_FILESEND_REQ 1
//发送内容为接收端对文件传输请求的应答，包含同意与否结果以及传输文件专用的端口号
#define MSG_TYPE_FILESEND_REQ_ACK 2
//发送内容指示发送端无法连接接收端的文件传输端口，文件传输取消
#define MSG_TYPE_FILESEND_CANCEL 3
//发送内容指示接收端收到了发送端进行的文件传输socket连接，打开了协商的文件准备好接收文件内容
#define MSG_TYPE_FILESEND_START 4

#endif //__MSG_TYPE_HPP