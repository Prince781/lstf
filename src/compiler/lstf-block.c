#include "lstf-block.h"
#include "compiler/lstf-codenode.h"
#include "data-structures/ptr-list.h"
#include "lstf-file.h"
#include <stdlib.h>

void lstf_block_destruct(lstf_codenode *code_node) {
    lstf_block *block = (lstf_block *)code_node;

    ptr_list_destroy(block->statement_list);
}

lstf_block *lstf_block_new(void)
{
    lstf_block *block = calloc(1, sizeof *block);

    lstf_codenode_construct((lstf_codenode *)block, 
            lstf_codenode_type_block,
            NULL, 
            lstf_block_destruct);

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
