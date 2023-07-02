#include "my_list.h"
#include <stdlib.h>

void init_list(my_list *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void push(my_list *list, void *data) {
    list_node *node = malloc(sizeof(list_node));
    node->data = data;
    node->next = NULL;
    if (is_empty(list)) list->head = list->tail = node;
    else {
        list->tail->next = node;
        list->tail = node;
    }
    list->size++;
}

int push_all(my_list *dest, my_list *src) {
    if (dest == NULL || src == NULL || is_empty(src)) return 0;
    list_node *node = src->head;
    int i = 0;
    while (node != NULL && i < src->size) {
        push(dest, node->data);
        node = node->next;
        i++;
    }
    return i;
}

void *pop(my_list *list) {
    if (is_empty(list)) return NULL;
    list_node *head = list->head;
    void *data = head->data;
    list->head = list->head->next;
    if (list->head == NULL) list->tail = NULL;
    list->size--;
    free(head);
    return data;
}

int is_empty(my_list *list) {
    return list == NULL || list->size <= 0;
}

void free_list(my_list *list) {
    while (!is_empty(list)) free(pop(list));
    free(list);
}