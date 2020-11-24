#pragma once

#include "lstf-statement.h"
#include "lstf-sourceref.h"
#include "lstf-codenode.h"
#include "data-structures/ptr-list.h"

struct _lstf_block {
    lstf_codenode parent_struct;

    /**
     * list of `(lstf_statement *)` objects
     */
    ptr_list *statement_list;
};
typedef struct _lstf_block lstf_block;

lstf_block *lstf_block_new(void);

void lstf_block_add_statement(lstf_block *block, lstf_statement *stmt);

void lstf_block_clear_statements(lstf_block *block);
