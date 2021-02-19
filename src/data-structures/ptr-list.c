#include "ptr-list.h"
#include "data-structures/collection.h"
#include "iterator.h"
#include <assert.h>
#include <stdarg.h>
#include <stdarg.h>
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

ptr_list *ptr_list_new_with_data(collection_item_ref_func   data_ref_func,
                                 collection_item_unref_func data_unref_func,
                                 ...)
{
    ptr_list *list = ptr_list_new(data_ref_func, data_unref_func);
    va_list ap;

    va_start(ap, data_unref_func);
    void *arg = NULL;
    while ((arg = va_arg(ap, void *)))
        ptr_list_append(list, arg);
    va_end(ap);

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
    list->head->prev = node;
    node->prev = list->tail;
    node->next = list->head;
    list->tail = node;

    list->length++;

    return node;
}

ptr_list_node *ptr_list_prepend(ptr_list *list, void *pointer)
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

    list->head->prev = node;
    list->tail->next = node;
    node->prev = list->tail;
    node->next = list->head;
    list->head = node;

    list->length++;

    return node;
}

ptr_list_node *ptr_list_find(ptr_list                     *list,
                             const void                   *query,
                             collection_item_equality_func comparator)
{
    for (iterator it = ptr_list_iterator_create(list);
            it.has_next;
            it = iterator_next(it))
        if (comparator ? comparator(query, iterator_get_item(it)) : query == iterator_get_item(it))
            return (ptr_list_node *) it.data;
    return NULL;
}

ptr_list_node *ptr_list_query(ptr_list *list,
                              bool    (*tester)(const void *, void *),
                              void     *user_data)
{
    for (iterator it = ptr_list_iterator_create(list);
            it.has_next;
            it = iterator_next(it))
        if (tester(iterator_get_item(it), user_data))
            return (ptr_list_node *) it.data;
    return NULL;
}

ptr_list_node *ptr_list_replace(ptr_list                     *list,
                                const void                   *query,
                                collection_item_equality_func comparator,
                                void                         *replacement)
{
    ptr_list_node *found_node = ptr_list_find(list, query, comparator);

    if (!found_node)
        return NULL;

    if (list->data_unref_func)
        list->data_unref_func(found_node->data);
    found_node->data = list->data_ref_func ? list->data_ref_func(replacement) : replacement;
    return found_node;
}

long ptr_list_index_of(ptr_list                     *list,
                       const void                   *query,
                       collection_item_equality_func comparator)
{
    for (iterator it = ptr_list_iterator_create(list);
            it.has_next;
            it = iterator_next(it))
        if (comparator ? comparator(query, iterator_get_item(it)) : query == iterator_get_item(it))
            return it.counter;
    return -1;
}

ptr_list_node *ptr_list_nth_element(ptr_list *list, unsigned long index)
{
    if (index >= list->length)
        return NULL;

    for (iterator it = ptr_list_iterator_create(list);
            it.has_next;
            it = iterator_next(it))
        if (it.counter == index)
            return (ptr_list_node *) it.data;
    return NULL;
}

void *ptr_list_remove_link(ptr_list *list, ptr_list_node *node)
{
    if (!node)
        return NULL;

    void *data = node->data;

    node->prev->next = node->next;
    node->next->prev = node->prev;

    if (list->head == list->tail && node == list->head) {
        list->head = NULL;
        list->tail = NULL;
    } else {
        if (node == list->head)
            list->head = node->next;
        if (node == list->tail)
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

void *ptr_list_remove_first_link(ptr_list *list)
{
    return ptr_list_remove_link(list, list->head);
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
    ptr_list_node *node = it.data;

    return (iterator) {
        .data = node->next,
        .is_first = false,
        .has_next = node->next != list->head,
        .counter = it.counter + 1,
        .collection = list,
        .iterate = it.iterate,
        .get_item = it.get_item,
        .item_maps = { it.item_maps[0] }
    };
}

static void *ptr_list_iterator_get_item(iterator it)
{
    ptr_list_node *node = it.data;

    return node ? node->data : NULL;
}

iterator ptr_list_iterator_create(ptr_list *list)
{
    return (iterator) {
        .data = list->head,
        .is_first = true,
        .has_next = list->head != NULL,
        .counter = 0,
        .collection = list,
        .iterate = ptr_list_iterator_iterate,
        .get_item = ptr_list_iterator_get_item,
        .item_maps = { NULL }
    };
}
