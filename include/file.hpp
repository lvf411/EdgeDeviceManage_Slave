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
#include "master.hpp"

#define FILEBUF_MAX_LENGTH 3072     //3KB
#define FILE_PACKAGE_SIZE 4096      //4KB

struct FileInfo
{
    std::string fname;
    long long int exatsize;
    int filesize;
    char unit;
    std::string md5;
};

void FileInfoInit(FileInfo *info);

bool FileInfoGet(std::string path, FileInfo *info);

string client_task_list_export(int client_id);

string work_client_list_export();

void file_send(int sock, std::string path);

void file_recv(int sock, FileInfo *info);

#endif //__FILE_HPP
