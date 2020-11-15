#pragma once

#include "iterator.h"
#include <stdbool.h>

struct _ptr_list_node {
    void *data;
    struct _ptr_list_node *next;
    struct _ptr_list_node *prev;
};
/**
 * NOTE: to retrieve the value, any (ptr_list_node *) can be cast to 
 * (data_type **), where data_type is the type of the data held in the list.
 */
typedef struct _ptr_list_node ptr_list_node;

/**
 * @node: a (ptr_list_node *)
 * @type: the pointer data type held in the list
 */
#define ptr_list_node_get_data(node, type) (*(type *)node)

typedef void (*ptr_list_data_free_func)(void *);
typedef bool (*ptr_list_data_equality_func)(const void *pointer1, const void *pointer2);

struct _ptr_list {
    ptr_list_node *head;
    ptr_list_node *tail;
    int length;
    ptr_list_data_free_func data_free_func;
};
typedef struct _ptr_list ptr_list;

ptr_list *ptr_list_new(ptr_list_data_free_func data_free_func);

ptr_list_node *ptr_list_append(ptr_list *list, void *pointer);

ptr_list_node *ptr_list_find(ptr_list *list, const void *pointer, ptr_list_data_equality_func comparator);

void *ptr_list_remove_link(ptr_list *list, ptr_list_node *node);

void *ptr_list_remove_last_link(ptr_list *list);

void ptr_list_clear(ptr_list *list);

void ptr_list_destroy(ptr_list *list);

bool ptr_list_is_empty(const ptr_list *list);

iterator ptr_list_iterator_create(ptr_list *list);
