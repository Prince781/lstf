#pragma once

#include <assert.h>
#include <stdlib.h>

#define list_node(T)                                                           \
    struct {                                                                   \
        T  data;                                                               \
        T *prev; /* cast to list_node(T), since [data] is at start */          \
        T *next; /* (same thing) */                                            \
    }

#define list(T)                                                                \
    struct {                                                                   \
        list_node(T) * head;                                                   \
        list_node(T) * tail;                                                   \
        unsigned length;                                                       \
        T (*data_ref_func)(T);                                                 \
        void (*data_unref_func)(T);                                            \
    }

/**
 * Append a new element [elem] to the list.
 */
#define list_append(ls, elem)                                                  \
    do {                                                                       \
        auto list = (ls);                                                      \
        if (!list->head) {                                                     \
            assert(!list->tail && "list tail must be null when head is null"); \
            list->head = (void *)(list->tail = calloc(1, sizeof *list->head)); \
            list->head->data =                                                 \
                list->data_ref_func ? list->data_ref_func(elem) : (elem);      \
        } else {                                                               \
            typeof(list->tail) node = calloc(1, sizeof *list->tail);           \
            node->data =                                                       \
                list->data_ref_func ? list->data_ref_func(elem) : (elem);      \
            list->tail->next = (void *)node;                                   \
            node->prev       = (void *)list->tail;                             \
            list->tail       = node;                                           \
        }                                                                      \
    } while (0)

#define list_foreach(ls, elem, statements)                                     \
    for (auto p = (ls)->head; p; p = (typeof(p))p->next) {                     \
        auto elem = &p->data;                                                  \
        statements;                                                            \
    }


#define list_pop(ls)                                                           \
    do {                                                                       \
        auto list = (ls);                                                      \
        assert(list->tail && "popping an empty list");                         \
        if (list->head == (void *)list->tail) {                                \
            free(list->tail);                                                  \
            list->head = (void *)(list->tail = nullptr);                       \
        } else {                                                               \
            auto old_tail    = list->tail;                                     \
            list->tail       = (void *)list->tail->prev;                       \
            list->tail->next = (void *)list->head;                             \
            free(old_tail);                                                    \
        }                                                                      \
    } while (0)
