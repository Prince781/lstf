#pragma once

#include "json/json-serializable.h"
#include "lsp-range.h"

json_serializable_decl_as_object(lsp_location, {
    char *uri;
    lsp_range range;
});
