#pragma once

#include "collection.h"
#include "data-structures/iterator.h"
#include "ptr-list.h"
#include <stdbool.h>

struct _ptr_hashmap_bucket;
typedef struct _ptr_hashmap_bucket ptr_hashmap_bucket;

struct _ptr_hashmap_entry {
    void *key;
    void *value;
    ptr_list_node *node_in_bucket_entries_list;
    ptr_list_node *node_in_entries_list;
};
typedef struct _ptr_hashmap_entry ptr_hashmap_entry;

struct _ptr_hashmap {
    /**
     * Each element is a `(ptr_hashmap_bucket *)`
     */
    ptr_list *buckets_list;

    ptr_hashmap_bucket **buckets;

    /**
     * The elements of the hashmap when you flatten the buckets.
     * This preserves the order of the inserted elements.
     *
     * Each element is a `(ptr_hashmap_entry *)`
     */
    ptr_list *entries_list;

    unsigned long num_bucket_places;

    collection_item_hash_func key_hash_func;
    collection_item_ref_func key_ref_func;
    collection_item_unref_func key_unref_func;
    collection_item_equality_func key_equality_func;
    collection_item_ref_func value_ref_func;
    collection_item_unref_func value_unref_func;
};
typedef struct _ptr_hashmap ptr_hashmap;

static inline void ptr_hashmap_entry_set_value(ptr_hashmap *map, ptr_hashmap_entry *entry, void *value)
{
    if (map->value_unref_func)
        map->value_unref_func(entry->value);
    entry->value = map->value_ref_func ? map->value_ref_func(value) : value;
}

/**
 * Should only be used by subclassing structures.
 */
void ptr_hashmap_construct(ptr_hashmap                     *map,
                           collection_item_hash_func        key_hash_func,
                           collection_item_ref_func         key_ref_func,
                           collection_item_unref_func       key_unref_func,
                           collection_item_equality_func    key_equality_func,
                           collection_item_ref_func         value_ref_func,
                           collection_item_unref_func       value_unref_func);

ptr_hashmap *ptr_hashmap_new(collection_item_hash_func      key_hash_func,
                             collection_item_ref_func       key_ref_func,
                             collection_item_unref_func     key_unref_func,
                             collection_item_equality_func  key_equality_func,
                             collection_item_ref_func       value_ref_func,
                             collection_item_unref_func     value_unref_func);

/**
 * Updates with a new entry for (new_key, new_value). [new_key] and [new_value]
 * may need to be non-null in case key_ref_func and/or value_ref_func are not NULL.
 */
ptr_hashmap_entry *ptr_hashmap_insert(ptr_hashmap *map, void *new_key, void *new_value);

ptr_hashmap_entry *ptr_hashmap_get(const ptr_hashmap *map, const void *key);

/**
 * Deletes the entry matching key. [key] must be non-const since it will be
 * modified if key_unref_func is not NULL.
 */
void ptr_hashmap_delete(ptr_hashmap *map, void *key);

bool ptr_hashmap_is_empty(const ptr_hashmap *map);

static inline unsigned long
ptr_hashmap_num_elements(const ptr_hashmap *map)
{
    return map->entries_list->length;
}

/**
 * Returns an iterator on the entries of the hash map.
 * Cast the result of `iterator_get_item()` to `(ptr_hashmap_entry *)`.
 * The entries are returned in the order they were inserted.
 */
static inline iterator ptr_hashmap_iterator_create(ptr_hashmap *map)
{
    return ptr_list_iterator_create(map->entries_list);
}

void ptr_hashmap_destroy(ptr_hashmap *map);
