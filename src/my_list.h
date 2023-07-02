#ifndef _MY_LIST_H_
#define _MY_LIST_H_

typedef struct list_node {
    void *data;
    struct list_node *next;
} list_node;

typedef struct {
    list_node *head;
    list_node *tail;
    int size;
} my_list;

void init_list(my_list *list);

void push(my_list *list, void *data);

/**
 * 从src列表中将所有元素按顺序加入dest列表尾部
 * 返回成功加入的元素个数
 */
int push_all(my_list *dest, my_list *src);

void *pop(my_list *list);

int is_empty(my_list *list);

void free_list(my_list *list);

#endif  // _MY_LIST_H_