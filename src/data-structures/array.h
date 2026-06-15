#pragma once

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A generic array that holds elements of some type.
 */
typedef struct {
    size_t length : sizeof(size_t) * CHAR_BIT - 1;
    bool nofree : 1;
    size_t bufsiz;
    size_t elemsz;
} array;

#define array(element_type) \
struct {\
    size_t length : sizeof(size_t) * CHAR_BIT - 1;\
    bool nofree : 1;\
    size_t bufsiz;\
    size_t elemsz;\
    element_type *elements;\
}

typedef array(void) array_generic;

static inline void *array_new(void)
{
    array *a = calloc(1, sizeof(array(void)));
    if (!a) {
        perror("could not allocate array");
        abort();
    }
    return a;
}

#define array_resize__internal(container) \
do {\
    const size_t array_minbufsiz = 1;\
    (container)->elemsz = sizeof((container)->elements[0]);\
    if ((container)->length < (container)->bufsiz) {\
        if ((container)->bufsiz / 2 >= array_minbufsiz && (container)->length < (container)->bufsiz / 2)\
            (container)->bufsiz /= 2;\
        else\
            break;\
    } else {\
        (container)->bufsiz = (container)->bufsiz ? (container)->bufsiz * 2 : array_minbufsiz;\
    }\
    void *new_elements = realloc((container)->elements, (container)->elemsz * (container)->bufsiz);\
    if (!new_elements) {\
        perror("could not resize array");\
        abort();\
    }\
    (container)->elements = new_elements;\
} while (0)

#define array_init(container) \
do {\
    (container)->length = 0;\
    (container)->nofree = true;\
    (container)->bufsiz = 0;\
    (container)->elemsz = sizeof((container)->elements[0]);\
    (container)->elements = NULL; \
} while (0)

#define array_destroy(container) \
do {\
    free((container)->elements);\
    if (!(container)->nofree)\
        free(container);\
} while (0)

#define array_add(container, element) \
do {\
    /* XXX: (container) evaluated two times */\
    if ((container)->length >= (container)->bufsiz)\
        array_resize__internal(container);\
    (container)->elements[(container)->length++] = (element);\
} while (0)

#define array_remove(container, index) \
do {\
    size_t _i_ = index;\
    assert(_i_ < (container)->length && "array_remove(): index out of bounds");\
    if (_i_ == (container)->length - 1u) {\
        (container)->length--;\
    } else {\
        memmove((char *)(container)->elements + _i_ * (container)->elemsz,\
                (char *)(container)->elements + (_i_+1) * (container)->elemsz,\
                (container)->elemsz * ((container)->length - (_i_+1)));\
        (container)->length--;\
    }\
    if ((container)->length <= (container)->bufsiz / 2)\
        array_resize__internal(container);\
} while (0)

#define array_pop(container) \
do {\
    array_remove(container, (container)->length - 1u); \
} while (0)
