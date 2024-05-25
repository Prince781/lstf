#pragma once

#include <assert.h>
#include <stdlib.h>

#define list_node(T)                                                           \
  struct {                                                                     \
    T data;                                                                    \
    T *prev; /* cast to list_node(T), since [data] is at start of struct */    \
    T *next; /* (same thing) */                                                \
  }

#define list(T)                                                                \
  struct {                                                                     \
    list_node(T) *head;                                                        \
    list_node(T) *tail;                                                        \
    unsigned length;                                                           \
    T (*data_ref_func)(T);                                                     \
    void (*data_unref_func)(T);                                                \
  }

/**
 * Append a new element [elem] to the list.
 */
#define list_append(ls, elem)                                                  \
  do {                                                                         \
    typeof(ls) list = ls;                                                      \
    if (!list->head) {                                                         \
      assert(!list->tail && "list tail must be null when head is null");       \
      list->head = (void *)(list->tail = calloc(1, sizeof *list->head));       \
      list->head->data =                                                       \
          list->data_ref_func ? list->data_ref_func(elem) : (elem);            \
    } else {                                                                   \
      typeof(list->tail) node = calloc(1, sizeof *list->tail);                 \
      node->data = list->data_ref_func ? list->data_ref_func(elem) : (elem);   \
      list->tail->next = (void *)node;                                         \
      node->prev = (void *)list->tail;                                         \
      list->tail = node;                                                       \
    }                                                                          \
  } while (0)

static void x(void) {
  list(int) l;
  list_append(&l, 3);
  list_append(&l, 4);
}
