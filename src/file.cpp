#include "file.hpp"
#include "slave.hpp"

extern Slave slave;
extern std::map<int, PeerNode *> free_client_list_map, work_client_list_map;
extern list_head file_req_list;

char SizeUnit[5] = {' ' , 'K', 'M', 'G', 'T'};

//FileInfo 实例初始化
void FileInfoInit(FileInfo *info)
{
    info->fname.clear();
    info->filesize = 0;
    info->exatsize = 0;
    info->unit = ' ';
    info->md5.clear();
}

//根据给定的文件路径获取文件的元数据
bool FileInfoGet(std::string path, FileInfo *info)
{
    std::ifstream f(path.c_str());
    if(!f.good())
    {
        printf("FileInfoGet open file error\n");
        return false;
    }
    info->fname = path;
    f.seekg(0, std::ios::end);
    info->exatsize = f.tellg();
    int u = 0;
    uint32_t size = info->exatsize;
    while(size > 10)
    {
        size /= 1000;
        u = u + 1;
    }
    info->filesize = size;
    info->unit = SizeUnit[u];
    info->md5 = FileDigest(path);
    return true;
}

// //将工作从节点链表导出为json文件，返回导出的文件名
// string work_client_list_export()
// {
//     Json::Value root, client;
//     root["work_client_num"] = Json::Value((int)work_client_list_map.size());
//     std::map<int, PeerNode*>::iterator it = work_client_list_map.begin();
//     while(it != work_client_list_map.end())
//     {
//         PeerNode *node = it->second;
//         Json::Value json_node;
//         json_node["client_id"] = Json::Value(node->client_id);
//         json_node["ip"] = Json::Value(inet_ntoa(node->addr.sin_addr));
//         json_node["listen_port"] = Json::Value(node->addr.sin_port);
//         client.append(json_node);
//         it++;
//     }
//     root["work_client"] = client;

//     Json::StyledWriter sw;
//     std::ofstream f;
//     std::stringstream ss;
//     ss << "work_client_list.json";
//     string fname = ss.str();
//     f.open(fname.c_str(), std::ios::trunc);
//     if(!f.is_open())
//     {
//         printf("open file %s error\n", fname.c_str());
//         return "";
//     }
//     f << sw.write(root);
//     f.close();

//     return fname;
// }

//解析工作从节点链表导出文件并同步至程序内部的slaveID与从节点地址的映射map实例中去
void work_client_list_descfile_parse_and_update(std::string fname)
{
    std::ifstream ifs(fname);
    Json::Value root;
    Json::Reader rd;
    rd.parse(ifs, root);

    //work_client_num
    int work_client_num = root["work_client_num"].asInt();
    //work_client 数组
    Json::Value work_client = root["work_client"];
    for(int i = 0; i < work_client_num; i++)
    {
        int client_id = work_client[i]["client_id"].asInt();
        struct sockaddr_in *addr = new struct sockaddr_in;
        addr->sin_addr.s_addr = inet_addr(work_client[i]["ip"].asCString());
        addr->sin_port = htons(work_client[i]["listen_port"].asInt());

        std::map<int, struct sockaddr_in*>::iterator it = slave.work_slave_addr.find(client_id);
        if(it == slave.work_slave_addr.end())
        {
            //新设备需更新
            slave.work_slave_addr.insert(std::make_pair(client_id, addr));
        }
        else
        {
            it->second->sin_addr.s_addr = addr->sin_addr.s_addr;
            it->second->sin_port = addr->sin_port;
        }
    }
    ifs.close();
}

// //根据客户端id导出当前分配的子任务链表为json文件，返回导出的文件名
// string client_task_list_export(int client_id)
// {
//     std::map<int, PeerNode *>::iterator it = work_client_list_map.find(client_id);
//     if(it == work_client_list_map.end()){
//         printf("目标从节点查找失败\n");
//         return NULL;
//     }
//     PeerNode *client = it->second;
//     Json::Value root;
//     root["client_id"] = Json::Value(client_id);
//     root["ip"] = Json::Value(inet_ntoa(client->addr.sin_addr));
//     root["port"] = Json::Value(client->addr.sin_port);
//     root["master_ip"] = Json::Value(inet_ntoa(master.addr.sin_addr));
//     root["master_port"] = Json::Value(master.addr.sin_port);
//     root["subtask_num"] = Json::Value(client->subtask_num);
//     Json::Value json_subtask;
//     int i = 0;
//     list_head subtask_head = client->head, *subtask_temp = client->head.next;
//     while(i < client->subtask_num && subtask_temp != &subtask_head)
//     {
//         i++;
//         SubTaskNode *node = (SubTaskNode *)(list_entry(subtask_temp, SubTaskNode, self));
//         Json::Value json_temp;
//         json_temp["root_id"] = Json::Value(node->root_id);
//         json_temp["subtask_id"] = Json::Value(node->subtask_id);
//         std::stringstream ss;
//         ss << node->root_id << '_' << node->subtask_id;
//         string fname = ss.str();
//         json_temp["exe_name"] = Json::Value(fname);
//         json_temp["input_num"] = Json::Value(node->prev_num);
//         Json::Value prev, next;
//         SubTaskResult *res_temp = node->prev_head->next;
//         int j = 0;
//         while(res_temp != NULL && j < node->prev_num)
//         {
//             Json::Value temp;
//             temp["subtask_id"] = Json::Value(res_temp->subtask_id);
//             temp["client_id"] = Json::Value(res_temp->client_id);
//             temp["fname"] = Json::Value(res_temp->fname);
//             prev.append(temp);
//         }
//         json_temp["input"] = prev;
//         json_temp["output_num"] = node->next_num;
//         res_temp = node->succ_head->next;
//         j = 0;
//         while(res_temp != NULL && j < node->next_num)
//         {
//             Json::Value temp;
//             temp["subtask_id"] = Json::Value(res_temp->subtask_id);
//             temp["client_id"] = Json::Value(res_temp->client_id);
//             temp["fname"] = Json::Value(res_temp->fname);
//             next.append(temp);
//         }
//         json_temp["output"] = next;
//         json_subtask.append(json_temp);
//     }
//     root["subtask"] = json_subtask;
    
//     Json::StyledWriter sw;
//     std::ofstream f;
//     std::stringstream ss;
//     ss << client_id << "_subtask_list.json";
//     string fname = ss.str();
//     f.open(fname.c_str(), std::ios::trunc);
//     if(!f.is_open())
//     {
//         printf("open file %s error\n", fname.c_str());
//         return "";
//     }
//     f << sw.write(root);
//     f.close();

//     return fname;
// }

bool find_subtask(int rootid, int subtaskid)
{
    list_head head = slave.task, *temp = slave.task.next;
    while(temp != &head)
    {
        SubTaskNode *node = list_entry(temp, SubTaskNode, self);
        if(node->root_id == rootid)
        {
            if(node->subtask_id == subtaskid)
            {
                return true;
            }
        }
        temp = temp->next;
    }
    return false;
}

//解析主节点导出的从节点分配子任务文件并更新slave实例中的子任务链表
void client_task_list_descfile_parse_and_update(std::string fname)
{
    std::ifstream ifs(fname);
    Json::Value root;
    Json::Reader rd;
    rd.parse(ifs, root);

    //client_id
    int client_id = root["client_id"].asInt();
    if(client_id != slave.slave_id)
    {
        perror("parse client task list descfile error: wrong file");
        ifs.close();
        return;
    }
    //subtask_num
    int subtask_num = root["subtask_num"].asInt();
    Json::Value subtask = root["subtask"];
    for(int i = 0; i < subtask_num; i++)
    {
        int rootid = subtask[i]["root_id"].asInt();
        int subtaskid = subtask[i]["subtask_id"].asInt();
        if(find_subtask(rootid, subtaskid) == false)
        {
            //子任务链表中未找到该任务，需插入该任务
            SubTaskNode *node = new SubTaskNode();
            node->client_id = slave.slave_id;
            node->root_id = rootid;
            node->subtask_id = subtaskid;
            node->exepath = subtask[i]["exe_name"].asString();
            node->exe_flag = false;     //还未从主节点接收文件
            node->prev_num = subtask[i]["input_num"].asInt();
            node->prev_head = new SubTaskResult();
            node->prev_head->next = NULL;
            SubTaskResult *temp = node->prev_head;
            for(int j = 0; j < node->prev_num; j++)
            {
                SubTaskResult *nnode = new SubTaskResult();
                nnode->subtask_id = subtask[i]["input"][j]["subtask_id"].asInt();
                nnode->client_id = subtask[i]["input"][j]["client_id"].asInt();
                nnode->fname = subtask[i]["input"][j]["fname"].asString();
                nnode->next = temp->next;
                temp->next = nnode;
                temp = nnode;
            }
            node->next_num = subtask[i]["output_num"].asInt();
            node->succ_head = new SubTaskResult();
            node->succ_head->next = NULL;
            temp = node->succ_head;
            for(int j = 0; j < node->next_num; j++)
            {
                SubTaskResult *nnode = new SubTaskResult();
                nnode->subtask_id = subtask[i]["output"][j]["subtask_id"].asInt();
                nnode->client_id = subtask[i]["output"][j]["client_id"].asInt();
                nnode->fname = subtask[i]["output"][j]["fname"].asString();
                nnode->next = temp->next;
                temp->next = nnode;
                temp = nnode;
            }
            node->head = slave.task;
            node->self = LIST_HEAD_INIT(node->self);
            list_add_tail(&node->self, &node->head);

            //插入到文件请求链表中
            FileReqNode *req_node = new FileReqNode();
            req_node->fname = node->exepath;
            req_node->self = LIST_HEAD_INIT(req_node->self);
            req_node->head = file_req_list;
            list_add_tail(&req_node->self, &req_node->head);
        }
    }

    //更新子任务数量
    slave.task_num = subtask_num;
    
    ifs.close();
}

void file_send(int sock, std::string path)
{
    FileInfo info;
    FileInfoInit(&info);
    FileInfoGet(path, &info);
    int packid = 0, packnum = ((info.exatsize / 3) * 4) / FILE_PACKAGE_SIZE;
    ifstream ifs(path, std::ios::binary);
    char file_readbuf[FILEBUF_MAX_LENGTH];
    char sendbuf[FILE_PACKAGE_SIZE];
    char recvbuf[1024];
    while(packid < packnum)
    {
        uint32_t sendlength;
        int ack = 0;
        memset(file_readbuf, 0, FILEBUF_MAX_LENGTH);
        memset(sendbuf, 0, FILE_PACKAGE_SIZE);
        memset(recvbuf, 0, 1024);
        ifs.read(file_readbuf, FILEBUF_MAX_LENGTH);
        Base64_Encode(file_readbuf, ifs.gcount(), sendbuf, &sendlength);
        do{
            send(sock, sendbuf, sendlength, 0);
            recv(sock, recvbuf, 1024, 0);
            Json::Value root;
            Json::Reader rd;
            rd.parse(recvbuf, root);
            ack = root["ret"].asInt();
        }while(ack < packid);
        packid++;
    }
    ifs.close();
}

//接收了对方的文件传输套接字的连接，还没发送确认信息,res_md5传引用
void file_recv(int sock, FileInfo *info, std::ofstream& ofs, std::string& res_md5)
{
    // std::ofstream ofs(info->fname, std::ios::binary | std::ios::app);
    char recvbuf[FILE_PACKAGE_SIZE];
    char file_writebuf[FILEBUF_MAX_LENGTH];
    char sendbuf[1024];
    long long int  whole_length = 0;
    uint32_t file_length = 0, recv_length = 0;
    int packid = 0, packnum = ((info->exatsize / 3) * 4) / FILE_PACKAGE_SIZE;
    while(packid < packnum)
    {
        recv_length = recv(sock, recvbuf, FILE_PACKAGE_SIZE, 0);
        Base64_Decode(recvbuf, recv_length, file_writebuf, &file_length);
        ofs.write(file_writebuf, file_length);
        whole_length += file_length;
        packid++;
        Json::Value root;
        root["ret"] = Json::Value(packid);
    }
    ofs.close();
    std::ifstream ifs(info->fname, std::ios::binary);
    MD5 md5;
    std::streamsize length;
    char buf[1024];
    while(!(ifs.eof()))
    {
        ifs.read(buf, 1024);
        length = ifs.gcount();
        if(length > 0)
        {
            md5.update(buf, length);
        }
    }
    ifs.close();
    res_md5 = md5.toString();
}

//检查文件是否存在，存在返回TRUE
bool isFileExists(string& name)
{
    ifstream f(name.c_str());
    return f.good();
}
