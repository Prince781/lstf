#pragma once

typedef void (*closure_func)(void *);
typedef void (*closure_data_unref_func)(void *);

typedef struct {
    closure_func func_ptr;
    void *user_data;
    closure_data_unref_func user_data_unref_func;
} closure;

closure *closure_new(closure_func            func_ptr,
                     void                   *user_data, 
                     closure_data_unref_func user_data_unref_func);

void closure_destroy(closure *cl);

/**
 * Invokes the closure [cl] by casting the function pointer to type [func_type]
 * and then calling it with any extra arguments provided.
 */
#define closure_vinvoke(cl, func_type, ...)                                    \
    ((func_type)((cl)->func_ptr))(__VA_ARGS__, (cl)->user_data)

/**
 * Invokes the closure [cl] with the user data. Provided as current C standard
 * in use (C17) doesn't allow you to provide 0 arguments to a varargs macro.
 */
#define closure_invoke(cl) ((cl)->func_ptr)((cl)->user_data)
