#include "ptr-list.h"
#include "data-structures/collection.h"
#include "iterator.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

ptr_list *ptr_list_new(collection_item_ref_func   data_ref_func,
                       collection_item_unref_func data_unref_func)
{
    ptr_list *list = calloc(1, sizeof *list);

    if (!list) {
        perror("could not allocate pointer list");
        abort();
    }
    list->data_ref_func = data_ref_func;
    list->data_unref_func = data_unref_func;

    return list;
}

ptr_list_node *ptr_list_append(ptr_list *list, void *pointer)
{
    ptr_list_node *node = calloc(1, sizeof *node);

    if (!node) {
        perror("could not allocate pointer node");
        abort();
    }
    node->data = list->data_ref_func ? list->data_ref_func(pointer) : pointer;

    if (list->head == list->tail && !list->head) {
        list->head = node;
        list->tail = node;
    }

    list->tail->next = node;
    node->prev = list->tail;
    node->next = list->head;
    list->tail = node;

    list->length++;

    return node;
}

ptr_list_node *ptr_list_find(ptr_list *list, const void *pointer, collection_item_equality_func comparator)
{
    for (iterator it = ptr_list_iterator_create(list);
            it.has_next;
            it = iterator_next(it))
        if (comparator ? comparator(pointer, iterator_get_item(it)) : pointer == iterator_get_item(it))
            return (ptr_list_node *) it.data[0];
    return NULL;
}

void *ptr_list_remove_link(ptr_list *list, ptr_list_node *node)
{
    if (!node)
        return NULL;

    void *data = node->data;

    node->prev->next = node->next;
    node->next->prev = node->prev;

    if (node == list->head) {
        list->head = NULL;
        list->tail = NULL;
    } else if (node == list->tail) {
        list->tail = node->prev;
    }

    node->next = NULL;
    node->prev = NULL;
    if (list->data_unref_func) {
        list->data_unref_func(node->data);
        data = NULL;
    }
    node->data = NULL;
    free(node);

    list->length--;

    return data;
}

void *ptr_list_remove_last_link(ptr_list *list)
{
    return ptr_list_remove_link(list, list->tail);
}

void ptr_list_clear(ptr_list *list)
{
    while (list->tail)
        (void)ptr_list_remove_last_link(list);
}

void ptr_list_destroy(ptr_list *list)
{
    ptr_list_clear(list);
    free(list);
}

static iterator ptr_list_iterator_iterate(iterator it)
{
    ptr_list *list = it.collection;
    ptr_list_node *node = it.data[0];

    return (iterator) {
        .data = { node->next, NULL /* unused */ },
        .is_first = false,
        .has_next = node->next != NULL && node->next != list->head,
        .collection = list,
        .iterate = it.iterate,
        .get_item = it.get_item
    };
}

static void *ptr_list_iterator_get_item(iterator it)
{
    ptr_list_node *node = it.data[0];

    return node ? node->data : NULL;
}

bool ptr_list_is_empty(const ptr_list *list)
{
    return list->head == NULL;
}

iterator ptr_list_iterator_create(ptr_list *list)
{
    return (iterator) {
        .data = { list->head, NULL /* unused */ },
        .is_first = true,
        .has_next = list->head != NULL,
        .collection = list,
        .iterate = ptr_list_iterator_iterate,
        .get_item = ptr_list_iterator_get_item
    };
}
