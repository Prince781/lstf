#pragma once

#include "lstf-statement.h"
#include "lstf-sourceref.h"
#include "lstf-codenode.h"
#include "lstf-scope.h"
#include "data-structures/ptr-list.h"
#include <stddef.h>

struct _lstf_block {
    lstf_codenode parent_struct;

    /**
     * list of `(lstf_statement *)` objects
     */
    ptr_list *statement_list;

    lstf_scope *scope;
};
/**
 * An executable series of statements within a scope.
 */
typedef struct _lstf_block lstf_block;

static inline lstf_block *lstf_block_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_block)
        return (lstf_block *)code_node;
    return NULL;
}

lstf_block *lstf_block_new(void);

void lstf_block_add_statement(lstf_block *block, lstf_statement *stmt)
    __attribute__((nonnull (1, 2)));

void lstf_block_clear_statements(lstf_block *block);
