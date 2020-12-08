#include "lstf-block.h"
#include "compiler/lstf-scope.h"
#include "lstf-patterntest.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include "lstf-expressionstatement.h"
#include "lstf-statement.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "lstf-file.h"
#include <stdlib.h>

static void lstf_block_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_block(visitor, (lstf_block *)code_node);
}

static void lstf_block_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_block *block = (lstf_block *)code_node;

    for (iterator it = ptr_list_iterator_create(block->statement_list); it.has_next; it = iterator_next(it)) {
        lstf_statement *stmt = iterator_get_item(it);

        switch (stmt->stmt_type) {
        case lstf_statement_type_assignment:
            lstf_codevisitor_visit_assignment(visitor, (lstf_assignment *)stmt);
            break;
        case lstf_statement_type_expression:
            lstf_codevisitor_visit_expression_statement(visitor, (lstf_expressionstatement *)stmt);
            break;
        case lstf_statement_type_patterntest:
            lstf_codevisitor_visit_pattern_test(visitor, (lstf_patterntest *)stmt);
            break;
        case lstf_statement_type_declaration:
            lstf_codevisitor_visit_declaration(visitor, (lstf_declaration *)stmt);
            break;
        case lstf_statement_type_return:
            lstf_codevisitor_visit_return_statement(visitor, (lstf_returnstatement *)stmt);
            break;
        }
    }
}

static void lstf_block_destruct(lstf_codenode *code_node) {
    lstf_block *block = (lstf_block *)code_node;

    ptr_list_destroy(block->statement_list);
    lstf_codenode_unref(block->scope);
}

static const lstf_codenode_vtable block_vtable = {
    lstf_block_accept,
    lstf_block_accept_children,
    lstf_block_destruct
};

lstf_block *lstf_block_new(void)
{
    lstf_block *block = calloc(1, sizeof *block);

    lstf_codenode_construct((lstf_codenode *)block, 
            &block_vtable,
            lstf_codenode_type_block,
            NULL);

    block->scope = lstf_codenode_ref(lstf_scope_new((lstf_codenode *)block));
    block->statement_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);

    return block;
}

void lstf_block_add_statement(lstf_block *block, lstf_statement *stmt)
{
    ptr_list_append(block->statement_list, stmt);
    lstf_codenode_set_parent(stmt, block);
}

void lstf_block_clear_statements(lstf_block *block)
{
    ptr_list_clear(block->statement_list);
}
