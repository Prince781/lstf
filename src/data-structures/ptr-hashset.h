#pragma once

#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include <stdbool.h>

struct _ptr_hashset {
    /**
     * (subclassing a ptr_hashmap)
     */
    ptr_hashmap parent_struct;
};
typedef struct _ptr_hashset ptr_hashset;

ptr_hashset *ptr_hashset_new(collection_item_hash_func     item_hash_func,
                             collection_item_ref_func      item_ref_func,
                             collection_item_unref_func    item_unref_func,
                             collection_item_equality_func item_equality_func);

/**
 * Updates with a new entry for [new_element], which needs to be non-null in
 * case item_ref_func was not NULL.
 *
 * After this operation, it is guaranteed that [new_element] will occupy a
 * space in the hash set.
 */
void ptr_hashset_insert(ptr_hashset *set, void *new_element);

/**
 * Deletes an entry matching element. [element] must be non-const since it will
 * be modified if item_unref_func is not NULL.
 */
void ptr_hashset_delete(ptr_hashset *set, void *element);

void *ptr_hashset_get_last_element(const ptr_hashset *set);

bool ptr_hashset_contains(const ptr_hashset *set, const void *element);

static inline void ptr_hashset_clear(ptr_hashset *set)
{
    ptr_hashmap_clear((ptr_hashmap *)set);
}

bool ptr_hashset_is_empty(const ptr_hashset *set);

static inline unsigned long
ptr_hashset_num_elements(const ptr_hashset *set)
{
    return ptr_hashmap_num_elements((const ptr_hashmap *)set);
}

/**
 * Cast the result of `iterator_get_item()` to `(T)` where `T` is the type of
 * the element inserted into this hash set.
 */
iterator ptr_hashset_iterator_create(ptr_hashset *set);

void ptr_hashset_destroy(ptr_hashset *set);
