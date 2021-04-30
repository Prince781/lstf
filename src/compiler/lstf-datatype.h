#pragma once

#include "data-structures/ptr-list.h"
#include "lstf-common.h"
#include "lstf-symbol.h"
#include "lstf-sourceref.h"
#include "lstf-codenode.h"
#include "lstf-typesymbol.h"
#include <stdbool.h>

typedef struct _lstf_datatype lstf_datatype;
typedef struct _lstf_datatype_vtable lstf_datatype_vtable;

struct _lstf_datatype_vtable {
    bool (*is_supertype_of)(lstf_datatype *self, lstf_datatype *other);
    lstf_datatype *(*copy)(lstf_datatype *self);
    char *(*to_string)(lstf_datatype *self);
    bool (*add_type_parameter)(lstf_datatype *self, lstf_datatype *type_parameter);
    bool (*replace_type_parameter)(lstf_datatype *self, lstf_datatype *type_parameter, lstf_datatype *replacement_type);
};

/**
 * The real class of a `lstf_datatype` instance.
 */
enum _lstf_datatype_type {
    lstf_datatype_type_anytype,
    lstf_datatype_type_nulltype,
    lstf_datatype_type_voidtype,
    lstf_datatype_type_unresolvedtype,
    lstf_datatype_type_uniontype,
    lstf_datatype_type_objecttype,
    lstf_datatype_type_interfacetype,
    lstf_datatype_type_enumtype,
    lstf_datatype_type_numbertype,
    lstf_datatype_type_integertype,
    lstf_datatype_type_doubletype,
    lstf_datatype_type_booleantype,
    lstf_datatype_type_stringtype,
    lstf_datatype_type_arraytype,
    lstf_datatype_type_functiontype,
    lstf_datatype_type_patterntype,
    lstf_datatype_type_future
};
typedef enum _lstf_datatype_type lstf_datatype_type;

const char *lstf_datatype_type_to_string(lstf_datatype_type datatype_type);

struct _lstf_datatype {
    lstf_codenode parent_struct;
    const lstf_datatype_vtable *datatype_vtable;

    /**
     * The class type
     */
    lstf_datatype_type datatype_type;

    /**
     * (weak ref) (nullable) the symbol that creates this data type
     */
    lstf_symbol *symbol;

    /**
     * type parameters: `ptr_list<lstf_datatype *>`
     */
    ptr_list *parameters;
};

static inline lstf_datatype *lstf_datatype_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_datatype)
        return (lstf_datatype *)code_node;
    return NULL;
}

void lstf_datatype_construct(lstf_datatype              *datatype,
                             const lstf_codenode_vtable *codenode_vtable,
                             const lstf_sourceref       *source_reference,
                             lstf_datatype_type          datatype_type,
                             const lstf_datatype_vtable *datatype_vtable)
    __attribute__((nonnull (1, 2, 5)));

void lstf_datatype_destruct(lstf_datatype *datatype);

static inline lstf_symbol *
lstf_datatype_set_symbol(void *self, void *symbol)
{
    return lstf_datatype_cast(self)->symbol = lstf_symbol_cast(symbol);
}

/**
 * determines whether @self can receive a type @other
 * 
 * For example, if we have:
 * `[self] a = ([other]) b`
 *
 * Then this assignment only succeeds if `self.is_supertype_of(other)`
 */
bool lstf_datatype_is_supertype_of(lstf_datatype *self, lstf_datatype *other);

lstf_datatype *lstf_datatype_copy(lstf_datatype *self);

bool lstf_datatype_equals(lstf_datatype *self, lstf_datatype *other);

char *lstf_datatype_to_string(lstf_datatype *self);

/**
 * Returns true if this is a type parameter for a data type, or false otherwise.
 */
bool lstf_datatype_is_type_parameter(lstf_datatype *self);

/**
 * Attempt to add a type parameter to `self`. Returns false if the type does
 * not support parameterization or is already fully parameterized, true otherwise.
 */
bool lstf_datatype_add_type_parameter(lstf_datatype *self, lstf_datatype *type_parameter);

/**
 * Attempt to replace a type parameter for `self` with `replacement_type`.
 * Returns false if the type does not support parameterization or is already
 * fully parameterized, true otherwise.
 */
bool lstf_datatype_replace_type_parameter(lstf_datatype *self, lstf_datatype *type_parameter, lstf_datatype *replacement_type);
