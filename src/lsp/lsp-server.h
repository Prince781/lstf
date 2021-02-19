#pragma once

#include <stdbool.h>
#include <limits.h>
#include "lsp-client.h"

struct _lsp_server {
    unsigned long refcount : sizeof(unsigned long)*CHAR_BIT - 1;
    bool floating : 1;

    /**
     * The client to communicate with
     */
    lsp_client *client;
};
typedef struct _lsp_server lsp_server;
