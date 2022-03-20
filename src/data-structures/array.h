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

static inline void array_resize__internal(array_generic *a, size_t element_size)
{
    const size_t array_minbufsiz = 1;
    a->elemsz = element_size;
    if (a->length < a->bufsiz) {
        if (a->bufsiz / 2 >= array_minbufsiz && a->length < a->bufsiz / 2)
            a->bufsiz /= 2;
        else
            return;
    } else {
        a->bufsiz = a->bufsiz ? a->bufsiz * 2 : array_minbufsiz;
    }
    void *new_elements = realloc(a->elements, a->elemsz * a->bufsiz);
    if (!new_elements) {
        perror("could not resize array");
        abort();
    }
    a->elements = new_elements;
}

#define array_init(container) \
do {\
    (container)->length = 0;\
    (container)->nofree = true;\
    (container)->bufsiz = 0;\
    (container)->elemsz = sizeof((container)->elements[0]);\
} while (0)

#define array_destroy(container) \
do {\
    array_generic *_a_ = (array_generic *)container;\
    free(_a_->elements);\
    if (!_a_->nofree)\
        free(_a_);\
} while (0)

#define array_add(container, element) \
do {\
    /* XXX: (container) evaluated two times */\
    array_generic *_a_ = (array_generic *)(container);\
    if (_a_->length >= _a_->bufsiz)\
        array_resize__internal(_a_, sizeof (container)->elements[0]);\
    (container)->elements[_a_->length++] = element;\
} while (0)

#define array_remove(container, index) \
do {\
    array_generic *_a_ = (array_generic *)(container);\
    size_t _i_ = index;\
    assert(_i_ < _a_->length && "array_remove(): index out of bounds");\
    if (_i_ == _a_->length - 1u) {\
        _a_->length--;\
    } else {\
        memmove((char *)_a_->elements + _i_ * _a_->elemsz,\
                (char *)_a_->elements + (_i_+1) * _a_->elemsz,\
                _a_->elemsz * (_a_->length - (_i_+1)));\
        _a_->length--;\
    }\
    if (_a_->length <= _a_->bufsiz / 2)\
        array_resize__internal(_a_, _a_->elemsz);\
} while (0)
