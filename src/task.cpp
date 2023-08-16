#include "task.hpp"

extern Master master;
extern list_head free_client_list, work_client_list, deployed_task_list, uninit_task_list;
extern std::mutex mutex_slave_list;
extern std::map<int, ClientNode *> free_client_list_map, work_client_list_map;

int task_increment_id = 0;
std::mutex mutex_task_list, mutex_uninit_task_list, mutex_task_id;

//根据任务描述文件添加任务
bool task_add(std::string path){
    std::ifstream ifs(path);
    Json::Reader reader;
    Json::Value obj;
    reader.parse(ifs, obj);

    Task *task = new(Task);
    mutex_task_id.lock();
    task->id = ++task_increment_id;
    mutex_task_id.unlock();
    task->task_id = obj["task_id"].asString();
    task->subtask_num = obj["subtask_num"].asInt();

    list_head *subtask_list_head = new(list_head);
    *subtask_list_head = LIST_HEAD_INIT(*subtask_list_head);
    task->subtask_head = *subtask_list_head;
    //解析描述文件取出任务内容
    if(!obj["subtask"].isArray()){
        perror("desc file error");
        return false;
    }
    for(int i = 0; i < obj["subtask"].size(); i++)
    {
        SubTaskNode *node = new(SubTaskNode);
        node->subtask_id = obj["subtask"][i]["subtaskid"].asInt();
        node->root_id = task->id;
        node->exepath = obj["subtask"][i]["exe_path"].asString();
        node->prev_num = obj["subtask"][i]["input_src_num"].asInt();
        if(node->prev_num ==0)
        {
            node->prev_head = NULL;
        }
        else{
            node->prev_head = new(SubTaskResult);
            SubTaskResult *temp = node->prev_head;
            temp->next = NULL;
            for(int j = 0; j < node->prev_num; j++)
            {
                SubTaskResult *n = new(SubTaskResult);
                n->client_id = 0;
                n->subtask_id = obj["subtask"][i]["input_src"][j].asInt();
                n->next = temp->next;
                temp->next = n;
            }
        }
        node->next_num = obj["subtask"][i]["output_dst_num"].asInt();
        if(node->next_num ==0)
        {
            node->succ_head = NULL;
        }
        else{
            node->succ_head = new(SubTaskResult);
            SubTaskResult *temp = node->succ_head;
            temp->next = NULL;
            for(int j = 0; j < node->next_num; j++)
            {
                SubTaskResult *n = new(SubTaskResult);
                n->client_id = 0;
                n->subtask_id = obj["subtask"][i]["output_dst"][j].asInt();
                n->next = temp->next;
                temp->next = n;
            }
        }

        node->head = task->subtask_head;
        list_head *s = new(list_head);
        *s = LIST_HEAD_INIT(*s);
        node->self = *s;
        list_add_tail(s, &task->subtask_head);
    }

    //将任务节点插入到待分配任务链表尾部
    list_head *self = new(list_head);
    *self = LIST_HEAD_INIT(*self);
    task->self = *self;
    mutex_uninit_task_list.lock();
    list_add_tail(self, &uninit_task_list);
    mutex_uninit_task_list.unlock();

    //任务数量加一
    master.uninit_task_num++;
    
    return true;
}

//分配任务给各个节点
void task_deploy()
{
    while(!list_empty(&uninit_task_list)){
        //0、保证至少有两个设备可供分配
        while(master.work_client_num < 2 && master.free_client_num > 0){
            mutex_slave_list.lock();
            list_head *temp = free_client_list.next;
            ClientNode *slave = (ClientNode *)(list_entry(temp, ClientNode, self));
            list_del(free_client_list.next);
            free_client_list_map.erase(slave->client_id);
            master.free_client_num--;
            list_add_tail(temp, &(master.work_client_head));
            work_client_list_map.insert(std::map<int, ClientNode*>::value_type(slave->client_id, slave));
            master.work_client_num++;
            mutex_slave_list.unlock();
        }
        //没有大于2个从节点设备可供调配，休眠一段时间等待有无新从节点加入
        if(master.work_client_num < 2)
        {
            printf("从节点数量不足\n");
            sleep(10);
            continue;
        }
        
        //1、从未分配任务链表中取出任务
        mutex_uninit_task_list.lock();
        list_head task_node = *uninit_task_list.next;
        list_del(uninit_task_list.next);
        mutex_uninit_task_list.unlock();

        //2、分配子任务执行的从节点，并更新对应从节点结构体中的信息
        //此处将子任务交替依次分配给工作从节点链表头两个节点
        int i = 0;
        Task *task = (Task *)(list_entry(&task_node, Task, self));
        list_head subt_head = task->subtask_head, *subt_temp = task->subtask_head.next;
        ClientNode *slave[2];
        slave[0] = (ClientNode *)(list_entry(master.work_client_head.next, ClientNode, self));
        slave[1] = (ClientNode *)(list_entry(master.work_client_head.next->next, ClientNode, self));
        int pick = 0;   //指定当前子任务分配给slave[0]还是slave[1]
        std::vector<int> task_workclient_a;      //记录每个子任务按顺序被分配的执行从节点
        while(i < task->subtask_num)
        {
            i++;
            SubTaskNode *subt = (SubTaskNode *)(list_entry(&subt_temp, SubTaskNode, self));
            subt->client_id = slave[pick]->client_id;
            slave[pick]->subtask_num++;
            if(slave[pick]->flag == -1)
            {
                slave[pick]->flag = 0;
            }
            slave[pick]->modified = 1;
            list_add_tail(subt_temp, &(slave[pick]->head));
            task_workclient_a.push_back(pick);
            pick = 1 - pick;
            subt_temp = subt_temp->next;
        }

        //3、更新子任务节点中的前驱后继内容
        subt_temp = subt_head.next;
        while(subt_temp != &subt_head)
        {
            int j = 0;
            SubTaskNode *subt = (SubTaskNode *)(list_entry(&subt_temp, SubTaskNode, self));
            SubTaskResult *res_temp = subt->prev_head->next;
            while(j < subt->prev_num)
            {
                j++;
                res_temp->client_id = slave[task_workclient_a[res_temp->subtask_id]]->client_id;
                res_temp = res_temp->next;
            }
            j = 0;
            res_temp = subt->succ_head->next;
            while(j < subt->next_num)
            {
                j++;
                res_temp->client_id = slave[task_workclient_a[res_temp->subtask_id]]->client_id;
                res_temp = res_temp->next;
            }
        }

        //4、将任务插入任务链表
        mutex_task_list.lock();
        list_add_tail(&task_node, &deployed_task_list);
        mutex_task_list.unlock();
    }
    return;
}
