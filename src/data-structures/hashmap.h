#pragma once

#include "array.h"
#include <stdbool.h>

#define list_node(T)                                                           \
  struct {                                                                     \
    T data;                                                                    \
    T *next;                                                                   \
  }

#define list(T)                                                                \
  struct {                                                                     \
    list_node(T) *head;                                                        \
    size_t length;                                                             \
  }

// requires C23
#define list_foreach(ls, elem, statements)                                     \
  for (auto p = (ls)->head; p; p = (typeof(p))p->next) {                       \
    auto elem = &p->data;                                                      \
    statements;                                                                \
  }

#define hashmap_entry(keyT, valT)                                              \
  struct {                                                                     \
    keyT key;                                                                  \
    valT val;                                                                  \
  }

#define hashmap(keyT, valT)                                                    \
  struct {                                                                     \
    array(list(hashmap_entry(keyT, valT))) buckets;                            \
    list(hashmap_entry(keyT, valT)) entries;                                   \
    unsigned (*key_hash_func)(keyT);                                           \
    bool (*key_equal_func)(const keyT, const keyT);                            \
    keyT (*key_ref_func)(keyT);                                                \
    void (*key_unref_func)(keyT);                                              \
    valT (*val_ref_func)(valT);                                                \
    void (*val_unref_func)(valT);                                              \
  }

#define hashmap_init(hashmap, key_hash_fn, key_equal_fn, key_ref_fn,           \
                     key_unref_fn, val_ref_fn, val_unref_fn)                   \
  do {                                                                         \
    array_init(&(hashmap)->buckets);                                           \
    assert(key_hash_fn && "key hash function is required!");                   \
    (hashmap)->key_hash_func = key_hash_fn;                                    \
    (hashmap)->key_equal_func = key_equal_fn;                                  \
    (hashmap)->key_ref_func = key_ref_fn;                                      \
    (hashmap)->key_unref_func = key_unref_fn;                                  \
    (hashmap)->val_ref_func = val_ref_fn;                                      \
    (hashmap)->val_unref_func = val_unref_fn;                                  \
  } while (0)

// TODO:
#define hashmap_resize_to_length(hashmap, new_length) \
    do { \
    } while (0)

// TODO: use typeof() in C23 to avoid evaluating (hashmap) more than once
// 1. get the bucket
// 2. insert into the bucket
// 3. if the
#define hashmap_insert(hashmap, key_expr, val_expr)                            \
  do {                                                                         \
    unsigned hash = (hashmap)->key_hash_func(key_expr);                        \
    hash %= (hashmap)->buckets.length;                                         \
                                                                               \
    if ((hashmap)->num_elements > 0.75f * (hashmap)->buckets.length)           \
      hashmap_resize_to_length(hashmap, (hashmap)->buckets.length * 2);        \
                                                                               \
    bool inserted = false;                                                     \
    list_foreach(&(hashmap)->buckets.elements[hash], entry, {                  \
      if ((hashmap)->key_equal_func(entry->key, key_expr)) {                   \
        if ((hashmap)->key_unref_func) {                                       \
          (hashmap)->key_unref_func(entry->key);                               \
          entry->key = (hashmap)->key_ref_func(key_expr);                      \
        } else {                                                               \
          entry->key = key_expr;                                               \
        }                                                                      \
        if ((hashmap)->val_unref_func) {                                       \
          (hashmap)->val_unref_func(entry->val);                               \
          entry->val = (hashmap)->val_ref_func(val_expr);                      \
        } else {                                                               \
          entry->val = val_expr;                                               \
        }                                                                      \
        inserted = true;                                                       \
      }                                                                        \
    });                                                                        \
                                                                               \
    if (!inserted) {                                                           \
      list_append(&(hashmap)->buckets[hash], {key_expr, val_expr});            \
      inserted = true;                                                         \
    }                                                                          \
  } while (0)

static inline void test(void) {
  hashmap(char *, double) map;
  hashmap_init(&map, NULL, NULL, NULL, NULL, NULL, NULL);
  hashmap_insert(&map, "hello", 3.4);
}
