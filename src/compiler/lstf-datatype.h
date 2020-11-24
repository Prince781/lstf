#pragma once

#include "lstf-sourceref.h"
#include "lstf-codenode.h"
#include "lstf-typesymbol.h"
#include <stdbool.h>

typedef struct _lstf_datatype lstf_datatype;
typedef struct _lstf_datatype_vtable lstf_datatype_vtable;

struct _lstf_datatype_vtable {
    bool (*is_compatible_with)(const lstf_datatype *self, const lstf_datatype *other);
};

enum _lstf_datatype_type {
    lstf_datatype_type_value,
    lstf_datatype_type_object,
    lstf_datatype_type_array,
    lstf_datatype_type_function
};
typedef enum _lstf_datatype_type lstf_datatype_type;

struct _lstf_datatype {
    lstf_codenode parent_struct;
    const lstf_datatype_vtable *vtable;
    lstf_datatype_type type;
};

void lstf_datatype_construct(lstf_datatype          *datatype,
                             const lstf_sourceref   *source_reference,
                             lstf_codenode_dtor_func dtor_func,
                             lstf_typesymbol         type_symbol);

bool lstf_datatype_is_compatible_with(const lstf_datatype *self, const lstf_datatype *other);
