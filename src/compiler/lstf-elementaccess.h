#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"

struct _lstf_elementaccess {
    lstf_expression parent_struct;

    /**
     * base inner expression
     */
    lstf_expression *inner;

    /**
     * list of [lstf_expression]s
     */
    ptr_list *arguments;
};
typedef struct _lstf_elementaccess lstf_elementaccess;

lstf_expression *lstf_elementaccess_new(const lstf_sourceref *source_reference,
                                        lstf_expression      *inner,
                                        ptr_list             *arguments);
