#pragma once

#include "json/json-serializable.h"
#include "lsp-position.h"

json_serializable_decl_as_object(lsp_range, {
    lsp_position start;
    lsp_position end;
});
