#include "lstf-sourceref.h"
#include <assert.h>
#include <string.h>

char *lstf_sourceref_get_string(lstf_sourceloc begin, lstf_sourceloc end)
{
    assert(end.pos - begin.pos >= 0 && "invalid source reference");

    return strndup(begin.pos, end.pos - begin.pos + 1);
}
