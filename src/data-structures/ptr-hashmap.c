#include "data-structures/ptr-hashmap.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _ptr_hashmap_bucket {
    ptr_list_node *node_in_buckets_list;
    ptr_list *entries_list;
};

static void ptr_hashmap_bucket_destroy(ptr_hashmap_bucket *bucket)
{
    ptr_list_destroy(bucket->entries_list);
    bucket->entries_list = NULL;
    bucket->node_in_buckets_list = NULL;
    free(bucket);
}

static bool ptr_hashmap_default_key_equality_func(const void *pointer1, const void *pointer2)
{
    return pointer1 == pointer2;
}

ptr_hashmap *ptr_hashmap_new(collection_item_hash_func      key_hash_func,
                             collection_item_ref_func       key_ref_func,
                             collection_item_unref_func     key_unref_func,
                             collection_item_equality_func  key_equality_func,
                             collection_item_ref_func       value_ref_func,
                             collection_item_unref_func     value_unref_func)
{
    assert(key_hash_func && "key_hash_func required at the very least");

    ptr_hashmap *map = calloc(1, sizeof *map);

    if (!map) {
        perror("failed to allocate new hash map");
        abort();
    }

    map->buckets_list = ptr_list_new(NULL, (collection_item_unref_func) ptr_hashmap_bucket_destroy);
    map->num_bucket_places = 8;
    map->buckets = calloc(map->num_bucket_places, sizeof *map->buckets);

    if (!map->buckets) {
        perror("failed to create buckets array for hash map");
        abort();
    }

    map->entries_list = ptr_list_new(NULL, NULL);

    map->key_hash_func = key_hash_func;
    map->key_ref_func = key_ref_func;
    map->key_unref_func = key_unref_func;
    map->key_equality_func = key_equality_func ? key_equality_func : ptr_hashmap_default_key_equality_func;
    map->value_ref_func = value_ref_func;
    map->value_unref_func = value_unref_func;

    return map;
}

static void
ptr_hashmap_resize_to_length(ptr_hashmap *map, unsigned new_num_bucket_places)
{
    ptr_list *new_buckets_list = 
        ptr_list_new(map->buckets_list->data_ref_func, map->buckets_list->data_unref_func);
    ptr_hashmap_bucket **new_buckets = calloc(new_num_bucket_places, sizeof *new_buckets);

    if (!new_buckets) {
        perror("failed to allocate new buckets array");
        abort();
    }

    for (iterator buckets_it = ptr_list_iterator_create(map->buckets_list);
            buckets_it.has_next; 
            buckets_it = iterator_next(buckets_it)) {
        ptr_hashmap_bucket *bucket = iterator_get_item(buckets_it);
        for (iterator entries_it = ptr_list_iterator_create(bucket->entries_list);
                entries_it.has_next;
                entries_it = iterator_next(entries_it)) {
            ptr_hashmap_entry *entry = iterator_get_item(entries_it);
            unsigned hash = map->key_hash_func(entry->key) % new_num_bucket_places;
            ptr_hashmap_bucket *new_bucket = NULL;

            if (!(new_bucket = new_buckets[hash])) {
                new_bucket = calloc(1, sizeof *new_bucket);
                if (!new_bucket) {
                    perror("failed to create bucket for hashmap");
                    abort();
                }
                new_bucket->entries_list = 
                    ptr_list_new(bucket->entries_list->data_ref_func, bucket->entries_list->data_unref_func);
                new_bucket->node_in_buckets_list = ptr_list_append(new_buckets_list, new_bucket);
                new_buckets[hash] = new_bucket;
            }

            entry->node_in_bucket_entries_list = ptr_list_append(new_bucket->entries_list, entry);
        }
    }

    ptr_list_destroy(map->buckets_list);
    map->buckets_list = new_buckets_list;
    free(map->buckets);
    map->buckets = new_buckets;
    map->num_bucket_places = new_num_bucket_places;
}

ptr_hashmap_entry *ptr_hashmap_insert(ptr_hashmap *map, void *new_key, void *new_value)
{
    ptr_hashmap_entry *entry = NULL;

    if ((entry = ptr_hashmap_get(map, new_key))) {
        void *old_key = entry->key;
        void *old_value = entry->value;

        entry->key = map->key_ref_func ? map->key_ref_func(new_key) : new_key;
        entry->value = map->value_ref_func ? map->value_ref_func(new_value) : new_value;

        if (map->key_unref_func)
            map->key_unref_func(old_key);
        if (map->value_unref_func)
            map->value_unref_func(old_value);
    } else {
        unsigned hash = map->key_hash_func(new_key) % map->num_bucket_places;
        ptr_hashmap_bucket *bucket = NULL;

        if (!(entry = calloc(1, sizeof *entry))) {
            perror("failed to create entry for hash map");
            abort();
        }

        if (!(bucket = map->buckets[hash])) {
            bucket = calloc(1, sizeof *bucket);
            bucket->entries_list = ptr_list_new(NULL, NULL);
            bucket->node_in_buckets_list = ptr_list_append(map->buckets_list, bucket);
            map->buckets[hash] = bucket;
        }

        entry->key = map->key_ref_func ? map->key_ref_func(new_key) : new_key;
        entry->value = map->value_ref_func ? map->value_ref_func(new_value) : new_value;

        entry->node_in_bucket_entries_list = ptr_list_append(bucket->entries_list, entry);
        entry->node_in_entries_list = ptr_list_append(map->entries_list, entry);

        if (map->entries_list->length > 0.75 * map->num_bucket_places)
            ptr_hashmap_resize_to_length(map, map->num_bucket_places * 2);
    }

    return entry;
}

ptr_hashmap_entry *ptr_hashmap_get(const ptr_hashmap *map, const void *key)
{
    unsigned hash = map->key_hash_func(key) % map->num_bucket_places;
    ptr_hashmap_bucket *bucket = NULL;

    if ((bucket = map->buckets[hash])) {
        for (iterator it = ptr_list_iterator_create(bucket->entries_list);
                it.has_next;
                it = iterator_next(it)) {
            ptr_hashmap_entry *entry = iterator_get_item(it);

            if (map->key_equality_func(entry->key, key))
                return entry;
        }
    }

    return NULL;
}

static void 
ptr_hashmap_delete_entry(ptr_hashmap *map, ptr_hashmap_entry *entry, unsigned hash)
{
    ptr_hashmap_bucket *bucket = map->buckets[hash];

    ptr_list_remove_link(bucket->entries_list, entry->node_in_bucket_entries_list);
    entry->node_in_bucket_entries_list = NULL;
    ptr_list_remove_link(map->entries_list, entry->node_in_entries_list);
    entry->node_in_entries_list = NULL;

    if (ptr_list_is_empty(bucket->entries_list)) {
        ptr_list_destroy(bucket->entries_list);
        bucket->entries_list = NULL;
        ptr_list_remove_link(map->buckets_list, bucket->node_in_buckets_list);
        bucket = NULL;
    }

    if (map->key_unref_func)
        map->key_unref_func(entry->key);
    if (map->value_unref_func)
        map->value_unref_func(entry->value);
    free(entry);
}

void ptr_hashmap_delete(ptr_hashmap *map, void *key)
{
    unsigned hash = map->key_hash_func(key) % map->num_bucket_places;
    ptr_hashmap_bucket *bucket = NULL;

    if ((bucket = map->buckets[hash])) {
        ptr_list_node *entry_node = NULL;

        if ((entry_node = ptr_list_find(bucket->entries_list, key, map->key_equality_func)))
            ptr_hashmap_delete_entry(map, ptr_list_node_get_data(entry_node, ptr_hashmap_entry *), hash);
    }
}

bool ptr_hashmap_is_empty(const ptr_hashmap *map) {
    return ptr_list_is_empty(map->entries_list);
}

iterator ptr_hashmap_iterator_create(ptr_hashmap *map)
{
    return ptr_list_iterator_create(map->entries_list);
}

void ptr_hashmap_destroy(ptr_hashmap *map)
{
    for (iterator buckets_it = ptr_list_iterator_create(map->buckets_list);
            buckets_it.has_next; 
            buckets_it = iterator_next(buckets_it)) {
        ptr_hashmap_bucket *bucket = iterator_get_item(buckets_it);
        for (iterator entries_it = ptr_list_iterator_create(bucket->entries_list);
                entries_it.has_next;
                entries_it = iterator_next(entries_it)) {
            ptr_hashmap_entry *entry = iterator_get_item(entries_it);
            if (map->key_unref_func)
                map->key_unref_func(entry->key);
            if (map->value_unref_func)
                map->value_unref_func(entry->value);
            entry->key = NULL;
            entry->value = NULL;
            free(entry);
        }
    }
    ptr_list_destroy(map->buckets_list);
    map->buckets_list = NULL;
    ptr_list_destroy(map->entries_list);
    map->entries_list = NULL;

    free(map->buckets);
    map->buckets = NULL;
    map->num_bucket_places = 0;
    free(map);
}
