#include "data-structures/iterable.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int *int_new(int value) {
    int *p = malloc(sizeof *p);
    if (!p) {
        perror("failed to alloc() int");
        abort();
    }
    *p = value;
    return p;
}

int main(void) {
    int retval = 0;
    ptr_list *list = ptr_list_new(NULL, free);
    int i = 0;

    for (i = 0; i < 1024*1024; i++)
        ptr_list_append(list, int_new(i));

    i = 0;
    for (iterator it = ptr_list_iterator_create(list);
            it.has_next;
            it = iterator_next(it)) {
        if (*(int *)iterator_get_item(it) != i++)
            retval = 1;
    }

    ptr_list_destroy(list);
    return retval;
}
