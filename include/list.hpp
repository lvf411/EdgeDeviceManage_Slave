#ifndef __LIST_HPP
#define __LIST_HPP

//链表节点
struct list_head{
    struct list_head *prev, *next;
};

#ifndef container_of
#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - (char *) &((type *)0)->member)
#endif

//给出某一type类型的结构体中的member元素的地址ptr，获得该type类型结构体的首地址
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

//初始化双向链表
#define LIST_HEAD_INIT(name) { &(name), &(name) } 
//声明并初始化双向链表
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

void list_add(struct list_head *n, struct list_head *head);
void list_add_tail(struct list_head *n, struct list_head *head);
void list_del(struct list_head *entry);
int list_empty(struct list_head* entry);

#endif //__LIST_HPP
