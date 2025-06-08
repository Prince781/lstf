#pragma once

#include "json/json-serializable.h"

/**
 * `namespace MessageType`
 */
typedef enum {
    lsp_message_type_error = 1,
    lsp_message_type_warning,
    lsp_message_type_info,
    lsp_message_type_log
} lsp_message_type;

json_serializable_decl_as_enum(lsp_message_type);

/**
 * `interface ShowMessageParams`
 */
json_serializable_decl_as_object(lsp_showmessageparams, {
    lsp_message_type type;
    char *message;
});
