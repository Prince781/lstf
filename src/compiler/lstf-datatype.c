#include "lstf-datatype.h"
#include "lstf-interface.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
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
        case lstf_datatype_type_future:
            return "futuretype";
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

void lstf_datatype_construct(lstf_datatype              *self,
                             const lstf_codenode_vtable *codenode_vtable,
                             const lstf_sourceref       *source_reference,
                             lstf_datatype_type          datatype_type,
                             const lstf_datatype_vtable *datatype_vtable)
{
    assert(datatype_vtable->is_supertype_of && "is_supertype_of() must be implemented");
    assert(datatype_vtable->copy && "copy() must be implemented");

    lstf_codenode_construct((lstf_codenode *)self, 
            codenode_vtable, 
            lstf_codenode_type_datatype, 
            source_reference);
    self->datatype_vtable = datatype_vtable;
    self->datatype_type = datatype_type;
    self->parameters = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
}

void lstf_datatype_destruct(lstf_datatype *self)
{
    ptr_list_destroy(self->parameters);
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
    char *representation = NULL;

    lstf_interface *interface = lstf_interface_cast(self->symbol);
    if (self->symbol && (!interface || !interface->is_anonymous))
        representation = strdup(self->symbol->name);
    else
        representation = self->datatype_vtable->to_string(self);

    if (!ptr_list_is_empty(self->parameters)) {
        string *sb = string_new_take_data(representation);

        string_appendf(sb, "<");
        for (iterator param_it = ptr_list_iterator_create(self->parameters); param_it.has_next; param_it = iterator_next(param_it)) {
            lstf_datatype *param_dt = iterator_get_item(param_it);

            if (!param_it.is_first)
                string_appendf(sb, ", ");

            char *param_dt_string = lstf_datatype_to_string(param_dt);
            string_appendf(sb, "%s", param_dt_string);
            free(param_dt_string);
        }
        string_appendf(sb, ">");

        representation = string_destroy(sb);
    }

    return representation;
}

bool lstf_datatype_is_type_parameter(lstf_datatype *self)
{
    lstf_datatype *parent_dt = lstf_datatype_cast(lstf_codenode_cast(self)->parent_node);

    if (parent_dt && ptr_list_find(parent_dt->parameters, self, NULL))
        return true;
    return false;
}

bool lstf_datatype_add_type_parameter(lstf_datatype *self, lstf_datatype *type_parameter)
{
    assert(self != type_parameter && "cannot parameterize self with self");
    if (!self->datatype_vtable->add_type_parameter)
        return false;
    return self->datatype_vtable->add_type_parameter(self, type_parameter);
}

bool lstf_datatype_replace_type_parameter(lstf_datatype *self, lstf_datatype *type_parameter, lstf_datatype *replacement_type)
{
    assert(!(self == type_parameter || self == replacement_type) && "cannot parameterize self with self");
    if (!self->datatype_vtable->replace_type_parameter)
        return false;
    return self->datatype_vtable->replace_type_parameter(self, type_parameter, replacement_type);
}
