#pragma once

#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"

/**
 * A statically-defined type symbol
 */
struct _lstf_typesymbol {
    lstf_symbol parent_struct;

    /**
     * `(char *) -> (lstf_symbol *)`
     */
    ptr_hashmap *members;
};
typedef struct _lstf_typesymbol lstf_typesymbol;

lstf_typesymbol *lstf_typesymbol_new(const lstf_sourceref *source_reference, char *name);

void lstf_typesymbol_add_member(lstf_typesymbol *self, lstf_symbol *member);

lstf_symbol *lstf_typesymbol_get_member(lstf_typesymbol *self, const char *member_name);
