#include "lstf-datatype.h"
#include "compiler/lstf-interface.h"
#include "lstf-symbol.h"
#include "lstf-typealias.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const char *lstf_datatype_type_to_string(lstf_datatype_type datatype_type)
{
    switch (datatype_type) {
        case lstf_datatype_type_anytype:
            return "anytype";
        case lstf_datatype_type_arraytype:
            return "arraytype";
        case lstf_datatype_type_booleantype:
            return "booleantype";
        case lstf_datatype_type_doubletype:
            return "doubletype";
        case lstf_datatype_type_enumtype:
            return "enumtype";
        case lstf_datatype_type_functiontype:
            return "functiontype";
        case lstf_datatype_type_integertype:
            return "integertype";
        case lstf_datatype_type_interfacetype:
            return "interfacetype";
        case lstf_datatype_type_nulltype:
            return "nulltype";
        case lstf_datatype_type_numbertype:
            return "numbertype";
        case lstf_datatype_type_objecttype:
            return "objecttype";
        case lstf_datatype_type_patterntype:
            return "patterntype";
        case lstf_datatype_type_stringtype:
            return "stringtype";
        case lstf_datatype_type_uniontype:
            return "uniontype";
        case lstf_datatype_type_unresolvedtype:
            return "unresolvedtype";
        case lstf_datatype_type_voidtype:
            return "voidtype";
    }

    fprintf(stderr, "%s: invalid datatype `%u'", __func__, datatype_type);
    abort();
}

void lstf_datatype_construct(lstf_datatype              *datatype,
                             const lstf_codenode_vtable *codenode_vtable,
                             const lstf_sourceref       *source_reference,
                             lstf_datatype_type          datatype_type,
                             const lstf_datatype_vtable *datatype_vtable)
{
    assert(datatype_vtable->is_supertype_of && "is_supertype_of() must be implemented");
    assert(datatype_vtable->copy && "copy() must be implemented");

    lstf_codenode_construct((lstf_codenode *)datatype, 
            codenode_vtable, 
            lstf_codenode_type_datatype, 
            source_reference);
    datatype->datatype_vtable = datatype_vtable;
    datatype->datatype_type = datatype_type;
}

void lstf_datatype_destruct(lstf_datatype *datatype)
{
    // do nothing
    (void) datatype;
}

bool lstf_datatype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    return self->datatype_vtable->is_supertype_of(self, other);
}

lstf_datatype *lstf_datatype_copy(lstf_datatype *self)
{
    lstf_datatype *copy = self->datatype_vtable->copy(self);
    lstf_datatype_set_symbol(copy, self->symbol);
    return copy;
}

bool lstf_datatype_equals(lstf_datatype *self, lstf_datatype *other)
{
    return lstf_datatype_is_supertype_of(self, other) &&
        lstf_datatype_is_supertype_of(other, self);
}

char *lstf_datatype_to_string(lstf_datatype *self)
{
    if (!self)
        return NULL;
    if (self->symbol) {
        lstf_interface *interface = lstf_interface_cast(self->symbol);
        if (!interface || !interface->is_anonymous)
            return strdup(self->symbol->name);
    }
    return self->datatype_vtable->to_string(self);
}
