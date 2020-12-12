#include "lstf-sourceref.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
static inline char *strndup(const char *str, size_t n)
{
    size_t string_length = strlen(str);
    if (string_length < n)
        n = string_length;
    char *new_str = malloc(n + 1);
    memcpy(new_str, str, n);
    new_str[n] = '\0';
    return new_str;
}
#endif

char *lstf_sourceref_get_string(lstf_sourceloc begin, lstf_sourceloc end)
{
    assert(end.pos - begin.pos >= 0 && "invalid source reference");

    return strndup(begin.pos, end.pos - begin.pos + 1);
}
