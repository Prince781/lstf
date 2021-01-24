#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"

struct _lstf_elementaccess {
    lstf_expression parent_struct;

    /**
     * expression representing the container we want to access
     */
    lstf_expression *container;

    /**
     * list of [lstf_expression]s
     */
    ptr_list *arguments;
};
typedef struct _lstf_elementaccess lstf_elementaccess;

lstf_expression *lstf_elementaccess_new(const lstf_sourceref *source_reference,
                                        lstf_expression      *container,
                                        ptr_list             *arguments)
    __attribute__((nonnull (2, 3)));
