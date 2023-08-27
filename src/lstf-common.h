#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define fileno _fileno
#define strdup _strdup
#endif

// Win32 compatibility
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x) 
#endif

#define LSTF_VM_MAX_CAPTURES (unsigned)UINT8_MAX

/**
 * Returns the pointer as its parent type. This is a type-safe way of
 * upcasting.
 */
#define super(ptr) (&(ptr)->parent_struct)

/**
 * Take a pointer variable and initialize it on the heap. Will abort on failure.
 */
#define box(type, ptrvar) \
if (!((ptrvar) = calloc(1, sizeof *(ptrvar)))) { \
    fprintf(stderr, "error: failed to box " #type ": %s\n", strerror(errno)); \
    abort(); \
} \
*ptrvar = (type)
