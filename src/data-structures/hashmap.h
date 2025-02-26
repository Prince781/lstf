#pragma once

#include "array.h"
#include "list.h"
#include <stdbool.h>

// requires C23
#define list_foreach(ls, elem, statements)                                     \
    for (auto p = (ls)->head; p; p = (typeof(p))p->next) {                     \
        auto elem = &p->data;                                                  \
        statements;                                                            \
    }

#define hashmap_entry(keyT, valT)                                              \
    struct {                                                                   \
        keyT key;                                                              \
        valT val;                                                              \
    }

#define hashmap(keyT, valT)                                                    \
    struct {                                                                   \
        array(list(hashmap_entry(keyT, valT))) buckets;                        \
        list(hashmap_entry(keyT, valT)) entries;                               \
        unsigned (*key_hash_func)(keyT);                                       \
        bool (*key_equal_func)(const keyT, const keyT);                        \
        keyT (*key_ref_func)(keyT);                                            \
        void (*key_unref_func)(keyT);                                          \
        valT (*val_ref_func)(valT);                                            \
        void (*val_unref_func)(valT);                                          \
    }

#define hashmap_init(hashmap, key_hash_fn, key_equal_fn, key_ref_fn,           \
                     key_unref_fn, val_ref_fn, val_unref_fn)                   \
    do {                                                                       \
        auto map = (hashmap);                                                  \
        array_init(&map->buckets);                                             \
        assert(key_hash_fn && "key hash function is required!");               \
        map->key_hash_func  = key_hash_fn;                                     \
        map->key_equal_func = key_equal_fn;                                    \
        map->key_ref_func   = key_ref_fn;                                      \
        map->key_unref_func = key_unref_fn;                                    \
        map->val_ref_func   = val_ref_fn;                                      \
        map->val_unref_func = val_unref_fn;                                    \
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
    do {                                                                       \
        auto     map  = (hashmap);                                             \
        unsigned hash = map->key_hash_func(key_expr) % map->buckets.length;    \
                                                                               \
        if (map->entries.length > 0.75f * map->buckets.length)                 \
            hashmap_resize_to_length(map, map->buckets.length * 2);            \
                                                                               \
        bool inserted = false;                                                 \
        list_foreach(&map->buckets.elements[hash], entry, {                    \
            if (map->key_equal_func(entry->key, key_expr)) {                   \
                if (map->key_unref_func) {                                     \
                    map->key_unref_func(entry->key);                           \
                    entry->key = map->key_ref_func(key_expr);                  \
                } else {                                                       \
                    entry->key = key_expr;                                     \
                }                                                              \
                if (map->val_unref_func) {                                     \
                    map->val_unref_func(entry->val);                           \
                    entry->val = map->val_ref_func(val_expr);                  \
                } else {                                                       \
                    entry->val = val_expr;                                     \
                }                                                              \
                inserted = true;                                               \
            }                                                                  \
        });                                                                    \
                                                                               \
        if (!inserted) {                                                       \
            typeof(map->buckets.elements[hash].tail->data) key_val = {         \
                key_expr, val_expr};                                           \
            list_append(&map->buckets.elements[hash], key_val);                \
            inserted = true;                                                   \
        }                                                                      \
    } while (0)

static inline void test(void) {
    hashmap(char *, double) hmap;
    hashmap_init(&hmap, NULL, NULL, NULL, NULL, NULL, NULL);
    hashmap_insert(&hmap, "hello", 3.4);
}
