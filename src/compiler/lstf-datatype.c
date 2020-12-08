#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include <assert.h>

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
    return self->datatype_vtable->copy(self);
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
    return self->datatype_vtable->to_string(self);
}
