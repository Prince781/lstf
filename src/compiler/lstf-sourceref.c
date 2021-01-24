#include "lstf-sourceref.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

char *lstf_sourceref_get_string(lstf_sourceloc begin, lstf_sourceloc end)
{
    // end position is inclusive, so if the begin and end positions are one
    // past each other, it means that we are copying a zero-length string
    assert(end.pos - begin.pos >= -1 && "invalid source reference");

    return strndup(begin.pos, end.pos - begin.pos + 1);
}
