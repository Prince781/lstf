#include "lstf-declaration.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include "lstf-statement.h"
#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include <assert.h>
#include <stdlib.h>

static void lstf_declaration_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_declaration(visitor, (lstf_declaration *)node);
}

static void lstf_declaration_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codenode_accept(((lstf_declaration *)node)->declared_symbol, visitor);
}

static void lstf_declaration_destruct(lstf_codenode *node)
{
    lstf_declaration *declaration = (lstf_declaration *)node;

    lstf_codenode_unref(declaration->declared_symbol);
    declaration->declared_symbol = NULL;
}

static const lstf_codenode_vtable declaration_vtable = {
    lstf_declaration_accept,
    lstf_declaration_accept_children,
    lstf_declaration_destruct
};

static lstf_statement *
lstf_declaration_new(const lstf_sourceref *source_reference,
                     lstf_symbol          *symbol)
{
    assert(symbol && "symbol must be non-NULL");

    lstf_declaration *decl = calloc(1, sizeof *decl);

    lstf_statement_construct((lstf_statement *)decl,
            &declaration_vtable,
            source_reference,
            lstf_statement_type_declaration);

    decl->declared_symbol = lstf_codenode_ref(symbol);
    lstf_codenode_set_parent(decl->declared_symbol, decl);

    return (lstf_statement *) decl;
}

lstf_statement *lstf_declaration_new_from_enum(const lstf_sourceref *source_reference,
                                               lstf_enum            *enum_symbol)
{
    return lstf_declaration_new(source_reference, lstf_symbol_cast(enum_symbol));
}

lstf_statement *lstf_declaration_new_from_function(const lstf_sourceref *source_reference,
                                                   lstf_function    *function)
{
    return lstf_declaration_new(source_reference, lstf_symbol_cast(function));
}

lstf_statement *lstf_declaration_new_from_interface(const lstf_sourceref *source_reference,
                                                    lstf_interface       *interface)
{
    return lstf_declaration_new(source_reference, lstf_symbol_cast(interface));
}

lstf_statement *lstf_declaration_new_from_type_alias(const lstf_sourceref *source_reference,
                                                     lstf_typealias       *type_alias)
{
    return lstf_declaration_new(source_reference, lstf_symbol_cast(type_alias));
}

lstf_statement *lstf_declaration_new_from_variable(const lstf_sourceref *source_reference,
                                                   lstf_variable        *variable)
{
    return lstf_declaration_new(source_reference, lstf_symbol_cast(variable));
}
