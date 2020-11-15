#pragma once

typedef void (*closure_func)(void *);
typedef void (*closure_data_unref_func)(void *);

struct _closure {
    closure_func func_ptr;
    void *user_data;
    closure_data_unref_func user_data_unref_func;
};
typedef struct _closure closure;

closure *closure_new(closure_func            func_ptr,
                     void                   *user_data, 
                     closure_data_unref_func user_data_unref_func);

void closure_destroy(closure *cl);
