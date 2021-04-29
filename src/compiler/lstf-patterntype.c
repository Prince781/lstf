#include "lstf-patterntype.h"
#include "lstf-uniontype.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void lstf_patterntype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_patterntype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_patterntype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable patterntype_vtable = {
    lstf_patterntype_accept,
    lstf_patterntype_accept_children,
    lstf_patterntype_destruct
};

static bool lstf_patterntype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    (void) self;
    switch (other->datatype_type) {
        case lstf_datatype_type_booleantype:
        case lstf_datatype_type_doubletype:
        case lstf_datatype_type_arraytype:
        case lstf_datatype_type_integertype:
        case lstf_datatype_type_enumtype:
        case lstf_datatype_type_nulltype:
        case lstf_datatype_type_numbertype:
        case lstf_datatype_type_objecttype:
        case lstf_datatype_type_interfacetype:
        case lstf_datatype_type_stringtype:
        case lstf_datatype_type_patterntype:
            return true;
        case lstf_datatype_type_anytype:
        case lstf_datatype_type_functiontype:
        case lstf_datatype_type_unresolvedtype:
        case lstf_datatype_type_voidtype:
        case lstf_datatype_type_future:
            return false;
        case lstf_datatype_type_uniontype:
            for (iterator it = ptr_list_iterator_create(lstf_uniontype_cast(other)->options);
                    it.has_next; it = iterator_next(it))
                if (!lstf_datatype_is_supertype_of(self, (lstf_datatype *)iterator_get_item(it)))
                    return false;
            return true;
    }

    fprintf(stderr, "%s: unexpected data type `%u'\n", __func__, other->datatype_type);
    abort();
}

static lstf_datatype *lstf_patterntype_copy(lstf_datatype *self)
{
    return lstf_patterntype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_patterntype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("pattern");
}

static const lstf_datatype_vtable patterntype_datatype_vtable = {
    lstf_patterntype_is_supertype_of,
    lstf_patterntype_copy,
    lstf_patterntype_to_string,
    /* add_type_parameter = */ NULL,
    /* replace_type_parameter = */ NULL
};

lstf_datatype *lstf_patterntype_new(const lstf_sourceref *source_reference)
{
    lstf_patterntype *pattern_type = calloc(1, sizeof *pattern_type);

    if (!pattern_type) {
        perror("failed to create lstf_patterntype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)pattern_type,
            &patterntype_vtable,
            source_reference,
            lstf_datatype_type_patterntype,
            &patterntype_datatype_vtable);

    return (lstf_datatype *)pattern_type;
}
