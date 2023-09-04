#ifndef __FILE_HPP
#define __FILE_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>
#include <map>
#include "md5.hpp"
#include "base64.hpp"
#include "list.hpp"

#define FILEBUF_MAX_LENGTH 768      //768B
#define FILE_PACKAGE_SIZE 1024      //1KB

#define FILE_RECV_STATUS_NONE       0
#define FILE_RECV_STATUS_RECVING    1

struct FileInfo
{
    std::string fname;          //文件名
    uint exatsize;     //实际大小，单位字节
    int filesize;               //单位换算后的显示大小
    char unit;                  //单位
    std::string md5;            //计算出的md5值
};

struct FileTransInfo
{
    FileInfo info;              //文件元数据
    bool splitflag;             //是否进行拆包发送标志，拆包为TRUE，未拆包为FALSE
    bool base64flag;            //包内数据是否进行base64转码
    int packnum;                //拆包后的分包总数量
    int packsize;               //每个包所含数据的大小
    int file_type;              //文件类型
    int dst_rootid;             //指示发送给哪个任务的文件
    int dst_subtaskid;          //指示发送给 dst_rootid 的哪一个子任务的文件
};

#define FILE_TYPE_ORDINARY      0
#define FILE_TYPE_EXE           1
#define FILE_TYPE_INPUT         2
#define FILE_TYPE_OUTPUT        3
#define FILE_TYPE_KEY           4

struct FileReqNode
{
    std::string fname;          //请求的目标文件名
    int rootid;                 //指示该执行文件属于哪个任务
    int subtaskid;              //指示该执行文件属于 rootid 下的哪个子任务
    list_head head;             //指向文件请求链表的头结点
    list_head self;             //指向自身在链表中的指针
};

void FileInfoInit(FileInfo *info);

bool FileInfoGet(std::string path, FileInfo *info);

void file_send(int sock, std::string path);

void file_recv(int sock, FileInfo *info, std::ofstream& ofs, std::string& res_md5);

bool isFileExists(string& name);

void work_client_list_descfile_parse_and_update(std::string fname);

void client_task_list_descfile_parse_and_update(std::string fname);

#endif //__FILE_HPP
