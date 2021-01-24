#pragma once

#include <stddef.h>
#include <stdbool.h>

struct _iterator;
typedef struct _iterator iterator;

/**
 * Takes the collection and iterator and advances it.
 */
typedef iterator (*iterator_iterate_func)(iterator);

/**
 * Takes the iterator and returns the element by interpreting the state data.
 */
typedef void *(*iterator_get_item_func)(iterator);

struct _iterator {
    void *data[2];          // state data used by the collection
    bool is_first;
    bool has_next;
    size_t counter;
    void *collection;
    iterator_iterate_func iterate;
    iterator_get_item_func get_item;        // if NULL, then [data] is the item
};

iterator iterator_next(iterator it);

void *iterator_get_item(iterator it);
