#include "iterator.h"
#include <stddef.h>
#include <assert.h>

iterator iterator_next(iterator it)
{
    assert(it.has_next && "next() on ended iterator");

    if (!it.iterate) {
        return (iterator) {
            .data = { NULL, NULL },
            .is_first = false,
            .has_next = false,
            .collection = NULL,
            .iterate = NULL,
            .get_item = NULL
        };
    }

    return it.iterate(it);
}

void *iterator_get_item(iterator it)
{
    assert(it.has_next && "get_item() on ended iterator");

    return it.get_item ? it.get_item(it) : it.data[0];
}
