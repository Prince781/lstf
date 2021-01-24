#pragma once

#include "data-structures/collection.h"
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

struct _ptr_list {
    ptr_list_node *head;
    ptr_list_node *tail;
    unsigned long length;
    collection_item_ref_func data_ref_func;
    collection_item_unref_func data_unref_func;
};
typedef struct _ptr_list ptr_list;

/**
 * @node: a (ptr_list_node *)
 * @type: the pointer data type held in the list
 */
#define ptr_list_node_get_data(node, type) (*(type *)node)

ptr_list *ptr_list_new(collection_item_ref_func   data_ref_func,
                       collection_item_unref_func data_unref_func);

ptr_list_node *ptr_list_append(ptr_list *list, void *pointer);

ptr_list_node *ptr_list_find(ptr_list                     *list,
                             const void                   *query,
                             collection_item_equality_func comparator);

ptr_list_node *ptr_list_replace(ptr_list                     *list,
                                const void                   *query,
                                collection_item_equality_func comparator,
                                void                         *replacement);

/**
 * Returns `-1` if [query] is not found.
 */
long ptr_list_index_of(ptr_list                     *list,
                       const void                   *query,
                       collection_item_equality_func comparator);

ptr_list_node *ptr_list_nth_element(ptr_list *list, unsigned long index);

void *ptr_list_remove_link(ptr_list *list, ptr_list_node *node);

void *ptr_list_remove_first_link(ptr_list *list);

void *ptr_list_remove_last_link(ptr_list *list);

void ptr_list_clear(ptr_list *list);

void ptr_list_destroy(ptr_list *list);

bool ptr_list_is_empty(const ptr_list *list);

/**
 * Calling `iterator_get_item()` on the returned iterator gets the element
 * data.
 */
iterator ptr_list_iterator_create(ptr_list *list);
