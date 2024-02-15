#include "data-structures/ptr-hashset.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include <stdio.h>
#include <stdlib.h>

ptr_hashset *ptr_hashset_new(collection_item_hash_func     item_hash_func,
                             collection_item_ref_func      item_ref_func,
                             collection_item_unref_func    item_unref_func,
                             collection_item_equality_func item_equality_func)
{
    ptr_hashset *set = calloc(1, sizeof *set);

    if (!set) {
        perror("failed to create pointer hashset");
        abort();
    }

    ptr_hashmap_construct(super(set),
            item_hash_func,
            item_ref_func, item_unref_func,
            item_equality_func, NULL, NULL);

    return set;
}

void ptr_hashset_insert(ptr_hashset *set, void *new_element)
{
    ptr_hashmap_insert(super(set), new_element, NULL);
}

void ptr_hashset_delete(ptr_hashset *set, void *element)
{
    ptr_hashmap_delete(super(set), element);
}

void *ptr_hashset_get_last_element(const ptr_hashset *set)
{
    const ptr_list_node *node = super(set)->entries_list->tail;

    return ptr_list_node_get_data(node, const ptr_hashmap_entry *)->key;
}

bool ptr_hashset_contains(const ptr_hashset *set, const void *element)
{
    return ptr_hashmap_get(super(set), element);
}

bool ptr_hashset_is_empty(const ptr_hashset *set)
{
    return ptr_hashmap_is_empty((const ptr_hashmap *)set);
}

static void *
ptr_hashset_iterator_get_item(const ptr_hashmap_entry *entry)
{
    return entry->key;
}

iterator ptr_hashset_iterator_create(ptr_hashset *set)
{
    iterator it = ptr_hashmap_iterator_create((ptr_hashmap *)set);

    it.item_maps[0] = (iterator_item_map_func)ptr_hashset_iterator_get_item;

    return it;
}

void ptr_hashset_destroy(ptr_hashset *set)
{
    ptr_hashmap_destroy((ptr_hashmap *)set);
}
