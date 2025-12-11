#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>

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

/**
 * Maps the the return value of `iterator_get_item()`.
 */
typedef void *(*iterator_item_map_func)(void *);

struct _iterator {
    /**
     * State data used by the collection
     */
    void *data;
    bool is_first : 1;
    bool has_next : 1;
    unsigned long counter : sizeof(unsigned long)*CHAR_BIT - (1 + 1);
    void *collection;
    iterator_iterate_func iterate;

    /**
     * If the first item is NULL, then [data] is returned.
     */
    iterator_get_item_func get_item;

    /**
     * A list of functions to compose on the result of `iterator_get_item()`
     */
    iterator_item_map_func item_maps[1];
};

/**
 * Advances the iterator
 */
static inline iterator iterator_next(iterator it)
{
    assert(it.has_next && "next() on ended iterator");

    if (!it.iterate) {
        return (iterator) {
            .data = NULL,
            .is_first = false,
            .has_next = false,
            .collection = NULL,
            .iterate = NULL,
            .get_item = NULL,
            .item_maps = { NULL }
        };
    }

    return it.iterate(it);
}

/**
 * Returns the current element pointed to by the iterator.
 *
 * Under the hood:
 * Calls `get_item()` vfunc and then the composition of all the functions in
 * `item_maps` on that item.
 */
static inline void *iterator_get_item(iterator it)
{
    assert(it.has_next && "get_item() on ended iterator");

    void *item = it.get_item ? it.get_item(it) : it.data;

    for (size_t i = 0; i < sizeof(it.item_maps) / sizeof(it.item_maps[0]); i++) {
        if (!it.item_maps[i])
            break;
        item = it.item_maps[i](item);
    }

    return item;
}


/**
 * Gets the iterator name of an element. Useful in `*_foreach()` constructs.
 */
#define iterator_of(element_name) element_name##_it

#define index_of(element_name) ((unsigned long)((element_name##_it).counter))

#define foreach(iter_begin, elem, type, statements)                            \
    for (iterator iterator_of(elem) = (iter_begin);                            \
         iterator_of(elem).has_next;                                           \
         iterator_of(elem) = iterator_next(iterator_of(elem))) {               \
        type elem = (type)(uintptr_t)iterator_get_item(iterator_of(elem));     \
        statements;                                                            \
    }
