#pragma once

struct _lstf_sourceloc {
    int line;
    int column;
    const char *pos;
};
typedef struct _lstf_sourceloc lstf_sourceloc;
