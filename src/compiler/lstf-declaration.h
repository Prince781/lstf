#pragma once

#include "lstf-function.h"
#include "lstf-typealias.h"
#include "lstf-enum.h"
#include "lstf-interface.h"
#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include "lstf-statement.h"

#if defined(interface)
#undef interface        // MSVC
#endif

struct _lstf_declaration {
    lstf_statement parent_struct;
    lstf_symbol *declared_symbol;
};
typedef struct _lstf_declaration lstf_declaration;

static inline lstf_declaration *
lstf_declaration_cast(void *node)
{
    lstf_statement *stmt = lstf_statement_cast(node);

    if (stmt && stmt->stmt_type == lstf_statement_type_declaration)
        return node;
    return NULL;
}

lstf_statement *lstf_declaration_new_from_enum(const lstf_sourceref *source_reference,
                                               lstf_enum            *enum_symbol);

lstf_statement *lstf_declaration_new_from_function(const lstf_sourceref *source_reference,
                                                   lstf_function        *function);

lstf_statement *lstf_declaration_new_from_interface(const lstf_sourceref *source_reference,
                                                    lstf_interface       *interface);

lstf_statement *lstf_declaration_new_from_type_alias(const lstf_sourceref *source_reference,
                                                     lstf_typealias       *type_alias);

lstf_statement *lstf_declaration_new_from_variable(const lstf_sourceref *source_reference,
                                                   lstf_variable        *variable);
