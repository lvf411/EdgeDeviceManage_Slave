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
//发送内容指示接收端收到的文件的传输结果
#define MSG_TYPE_FILESEND_RES 5

//发送内容为从节点向主节点的文件请求
#define MSG_TYPE_FILEREQ_REQ 11
//发送内容为主节点对从节点的文件请求的应答
#define MSG_TYPE_FILEREQ_ACK 12
    //应答内容为同意
    #define FILEREQ_ACK_OK 0
    //应答内容为等待后重新发送请求
    #define FILEREQ_ACK_WAIT 1
    //应答内容为请求文件错误，拒绝
    #define FILEREQ_ACK_ERROR 2

//发送内容为从节点子任务执行情况上报
#define MSG_TYPE_SUBTASK_RESULT 21

//发送内容为指示任务x的子任务开始执行
#define MSG_TYPE_SUBTASK_RUN 100


#endif //__MSG_TYPE_HPP