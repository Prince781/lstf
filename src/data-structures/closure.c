#include "data-structures/closure.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

closure *closure_new(closure_func            func_ptr,
                     void                   *user_data, 
                     closure_data_unref_func user_data_unref_func)
{
    closure *cl = calloc(1, sizeof *cl);

    if (!cl) {
        fprintf(stderr, "%s: failed to allocate closure struct: %s\n",
                __func__, strerror(errno));
        abort();
    }

    cl->func_ptr = func_ptr;
    cl->user_data = user_data;
    cl->user_data_unref_func = user_data_unref_func;

    return cl;
}

void closure_destroy(closure *cl)
{
    if (cl->user_data_unref_func)
        cl->user_data_unref_func(cl->user_data);
    cl->func_ptr = NULL;
    cl->user_data = NULL;
    cl->user_data_unref_func = NULL;
    free(cl);
}
