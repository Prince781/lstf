#pragma once

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
};

/**
 * The real class of a `lstf_datatype` instance.
 */
enum _lstf_datatype_type {
    lstf_datatype_type_anytype,
    lstf_datatype_type_nulltype,
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
    lstf_datatype_type_patterntype
};
typedef enum _lstf_datatype_type lstf_datatype_type;

struct _lstf_datatype {
    lstf_codenode parent_struct;
    const lstf_datatype_vtable *datatype_vtable;

    /**
     * The class type
     */
    lstf_datatype_type datatype_type;
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
