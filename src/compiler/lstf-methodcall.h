#pragma once

#include "data-structures/ptr-list.h"
#include "lstf-sourceref.h"
#include "lstf-expression.h"

struct _lstf_methodcall {
    lstf_expression parent_struct;

    /**
     * Whether this is invoked with the `await` keyword.
     */
    bool is_awaited;

    /**
     * The expression from which we are invoking
     */
    lstf_expression *call;

    /**
     * list of [lstf_expression]s passed into this call
     */
    ptr_list *arguments;
};
typedef struct _lstf_methodcall lstf_methodcall;

lstf_expression *lstf_methodcall_new(const lstf_sourceref *source_reference,
                                     lstf_expression      *call,
                                     ptr_list             *arguments);
