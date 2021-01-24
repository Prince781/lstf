#include "lstf-uniontype.h"
#include "compiler/lstf-report.h"
#include "lstf-codevisitor.h"
#include "data-structures/iterator.h"
#include "data-structures/string-builder.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-sourceref.h"
#include "data-structures/ptr-list.h"
#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void lstf_uniontype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_uniontype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_uniontype *union_type = (lstf_uniontype *)node;

    for (iterator it = ptr_list_iterator_create(union_type->options); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);
}

static void lstf_uniontype_destruct(lstf_codenode *node)
{
    lstf_uniontype *union_type = (lstf_uniontype *)node;

    ptr_list_destroy(union_type->options);
    union_type->options = NULL;
    lstf_datatype_destruct((lstf_datatype *)union_type);
}

static const lstf_codenode_vtable uniontype_vtable = {
    lstf_uniontype_accept,
    lstf_uniontype_accept_children,
    lstf_uniontype_destruct
};

static bool lstf_uniontype_is_supertype_of(lstf_datatype *self_dt, lstf_datatype *other)
{
    lstf_uniontype *self = (lstf_uniontype *)self_dt;

    for (iterator it = ptr_list_iterator_create(self->options); it.has_next; it = iterator_next(it)) {
        if (lstf_datatype_is_supertype_of(iterator_get_item(it), other))
            return true;
    }

    // if the other is a union type, then check that every option of [other]
    // is present in [self]
    if (other->datatype_type == lstf_datatype_type_uniontype) {
        lstf_uniontype *other_ut = lstf_uniontype_cast(other);
        for (iterator it = ptr_list_iterator_create(other_ut->options); it.has_next; it = iterator_next(it)) {
            lstf_datatype *other_option = iterator_get_item(it);
            ptr_list_node *found = ptr_list_find(self->options, other_option, 
                    (collection_item_equality_func) lstf_datatype_is_supertype_of);

            if (!found)
                return false;
        }
        return true;
    }

    return false;
}

static lstf_datatype *lstf_uniontype_copy(lstf_datatype *self_dt)
{
    lstf_uniontype *self = (lstf_uniontype *)self_dt;
    iterator it = ptr_list_iterator_create(self->options);
    lstf_uniontype *new_union_type = (lstf_uniontype *)
        lstf_uniontype_new(&((lstf_codenode *)self)->source_reference, lstf_datatype_copy(iterator_get_item(it)), NULL);

    for (it = iterator_next(it); it.has_next; it = iterator_next(it))
        lstf_uniontype_add_option(new_union_type, lstf_datatype_copy(iterator_get_item(it)));

    return (lstf_datatype *)new_union_type;
}

static char *lstf_uniontype_to_string(lstf_datatype *self_dt)
{
    lstf_uniontype *self = (lstf_uniontype *)self_dt;
    string *representation = string_new();
    bool at_least_one = false;

    for (iterator it = ptr_list_iterator_create(self->options); it.has_next; it = iterator_next(it)) {
        if (at_least_one)
            string_appendf(representation, " | ");
        char *element_type_to_string = lstf_datatype_to_string(iterator_get_item(it));
        string_appendf(representation, "%s", element_type_to_string);
        free(element_type_to_string);
        at_least_one = true;
    }

    return string_destroy(representation);
}

static const lstf_datatype_vtable uniontype_datatype_vtable = {
    lstf_uniontype_is_supertype_of,
    lstf_uniontype_copy,
    lstf_uniontype_to_string
};

lstf_datatype *lstf_uniontype_new(const lstf_sourceref *source_reference,
                                  lstf_datatype        *first_type,
                                  ...)
{
    lstf_uniontype *union_type = calloc(1, sizeof *union_type);

    if (!union_type) {
        perror("failed to create lstf_uniontype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)union_type,
            &uniontype_vtable,
            source_reference,
            lstf_datatype_type_uniontype,
            &uniontype_datatype_vtable);

    union_type->options = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    lstf_uniontype_add_option(union_type, first_type);

    va_list args;

    va_start(args, first_type);
    void *arg = NULL;
    while ((arg = va_arg(args, lstf_datatype *))) {
        lstf_datatype *next_type = lstf_datatype_cast(arg);

        assert(next_type != NULL && "(lstf_datatype *) expected in argument list");
        lstf_uniontype_add_option(union_type, next_type);
    }
    va_end(args);

    return (lstf_datatype *) union_type;
}

void lstf_uniontype_add_option(lstf_uniontype *union_type, lstf_datatype *type_option)
{
    assert(type_option != ((lstf_datatype *)union_type) && "cannot add union type to itself!");

    if (ptr_list_find(union_type->options, type_option, 
                (collection_item_equality_func) lstf_datatype_equals))
        return;

    if (lstf_codenode_cast(type_option)->parent_node)
        type_option = lstf_datatype_copy(type_option);

    ptr_list_append(union_type->options, type_option);
    lstf_codenode_set_parent(type_option, union_type);
}

void lstf_union_type_replace_option(lstf_uniontype *union_type,
                                    lstf_datatype  *old_data_type,
                                    lstf_datatype  *new_data_type)
{
    if (lstf_codenode_cast(new_data_type)->parent_node)
        new_data_type = lstf_datatype_copy(new_data_type);
    ptr_list_node *result = ptr_list_replace(union_type->options, old_data_type, NULL, new_data_type);
    assert(result && "attempting to replace an option that doesn't exist in a union type!");
    lstf_codenode_set_parent(new_data_type, union_type);
}
