#include "lstf-datatype.h"

bool lstf_datatype_is_compatible_with(const lstf_datatype *self, const lstf_datatype *other)
{
    return self->vtable->is_compatible_with(self, other);
}
