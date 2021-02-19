#include "lstf-patterntest.h"
#include "data-structures/ptr-hashset.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned next_pattern_test_id = 1;

static void lstf_patterntest_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codevisitor_visit_pattern_test(visitor, stmt);
}

static void lstf_patterntest_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codenode_accept(stmt->pattern, visitor);
    lstf_codenode_accept(stmt->expression, visitor);
}

static void lstf_patterntest_destruct(lstf_codenode *code_node)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codenode_unref(stmt->pattern);
    lstf_codenode_unref(stmt->expression);
    ptr_hashset_destroy(stmt->captured_locals);
    lstf_codenode_unref(stmt->hidden_scope);
}

static const lstf_codenode_vtable patterntest_vtable = {
    lstf_patterntest_accept,
    lstf_patterntest_accept_children,
    lstf_patterntest_destruct
};

lstf_statement *lstf_patterntest_new(const lstf_sourceref     *source_reference,
                                     lstf_expression          *pattern,
                                     lstf_expression          *expression)
{
    lstf_patterntest *stmt = calloc(1, sizeof *stmt);

    if (!stmt) {
        perror("failed to create lstf_patterntest");
        abort();
    }

    lstf_statement_construct((lstf_statement *)stmt,
            &patterntest_vtable,
            source_reference,
            lstf_statement_type_patterntest);

    stmt->pattern = lstf_codenode_ref(pattern);
    stmt->expression = lstf_codenode_ref(expression);
    stmt->captured_locals = ptr_hashset_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref,
            NULL);
    stmt->hidden_scope = lstf_codenode_ref(lstf_scope_new((lstf_codenode *)stmt));
    stmt->id = next_pattern_test_id++;

    return (lstf_statement *) stmt;
}

void lstf_patterntest_add_captured_local(lstf_patterntest *stmt,
                                         lstf_symbol      *var_or_fn)
{
    assert(lstf_variable_cast(var_or_fn) ||
            (lstf_function_cast(var_or_fn) &&
             !ptr_hashset_is_empty(lstf_function_cast(var_or_fn)->captured_locals)));
    ptr_hashset_insert(stmt->captured_locals, var_or_fn);
}
