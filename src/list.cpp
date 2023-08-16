#include "list.hpp"

//在 head 节点后添加新节点 n
void list_add(struct list_head *n, struct list_head *head)
{
    head->next->prev = n;
    n->next = head->next;
    n->prev = head;
    head->next = n;
}

//在 head 节点前添加新节点 n
void list_add_tail(struct list_head *n, struct list_head *head)
{
    head->prev->next = n;
    n->prev = head->prev;
    n->next = head;
    head->prev = n;
}

//将 entry 节点在链表中删除。执行后节点指向的前驱后继都为节点自身，若是动态分配则还需进行内存释放
void list_del(struct list_head *entry)
{
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->next = entry;
    entry->prev = entry;
}

//链表判空，若为空则返回1
int list_empty(struct list_head* entry)
{
    return (entry->next == entry);
}
