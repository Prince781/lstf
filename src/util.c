#include "util.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

unsigned strhash(const char *str)
{
    unsigned hash = 0;
    while (*str) {
        hash = hash*31 + *str;
        ++str;
    }
    return hash;
}

bool strequal(const void *str1, const void *str2)
{
    return strcmp(str1, str2) == 0;
}

unsigned int ptrhash(const void *ptr)
{
    return (unsigned int)(intptr_t)ptr;
}
