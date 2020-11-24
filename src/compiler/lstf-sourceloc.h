#pragma once

struct _lstf_sourceloc {
    unsigned line;
    unsigned column;
    const char *pos;
};
typedef struct _lstf_sourceloc lstf_sourceloc;
