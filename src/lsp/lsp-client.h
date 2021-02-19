#pragma once

#include "io/inputstream.h"
#include "io/outputstream.h"
#include <stdbool.h>
#include <limits.h>

struct _lsp_client {
    unsigned long refcount : sizeof(unsigned long)*CHAR_BIT - 1;
    bool floating : 1;

    /**
     * The stream to read incoming data from the client.
     */
    inputstream *istream;

    /**
     * The stream to write responses to the client.
     */
    outputstream *ostream;
};
typedef struct _lsp_client lsp_client;
