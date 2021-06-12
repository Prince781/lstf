#pragma once

#include <stdint.h>
#include "json/json-serializable.h"

json_serializable_decl_as_object(lsp_position, {
    int64_t line;
    int64_t character;
});
