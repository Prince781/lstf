#include "lstf-ir-basicblock.h"
#include "lstf-ir-node.h"
#include "lstf-ir-function.h"
#include <stdio.h>
#include <stdlib.h>

static void
lstf_ir_basicblock_destruct(lstf_ir_node *node)
{
    lstf_ir_basicblock *bblock = (lstf_ir_basicblock *)node;

    for (unsigned i = 0; i < bblock->instructions_length; i++)
        lstf_ir_node_unref(bblock->instructions[i]);

    free(bblock->instructions);
}

lstf_ir_basicblock *lstf_ir_basicblock_new(void)
{
    lstf_ir_basicblock *bblock = calloc(1, sizeof *bblock);

    if (!bblock) {
        perror("failed to allocate IR basic block");
        abort();
    }

    lstf_ir_node_construct((lstf_ir_node *)bblock, lstf_ir_basicblock_destruct, lstf_ir_node_type_basicblock);
    bblock->instructions_buffer_size = 4;
    bblock->instructions = calloc(bblock->instructions_buffer_size, sizeof *bblock->instructions);

    return bblock;
}

void lstf_ir_basicblock_add_instruction(lstf_ir_basicblock  *block,
                                        lstf_ir_instruction *instruction)
{
    // we should not be inserting an instruction into a basic block that
    // already has a branch or return at the end of it, since this violates the
    // basic block property
    assert(!(block->instructions_length > 0 &&
                (block->instructions[block->instructions_length - 1]->insn_type == lstf_ir_instruction_type_branch ||
                 block->instructions[block->instructions_length - 1]->insn_type == lstf_ir_instruction_type_return)) &&
            "attempting to add instruction after branch or return in basic block!");

    if (block->instructions_length >= block->instructions_buffer_size) {
        block->instructions_buffer_size *= 2;
        lstf_ir_instruction **buffer = realloc(block->instructions,
                block->instructions_buffer_size * sizeof(*block->instructions));
        if (!buffer) {
            perror("failed to increase basic block IR instruction buffer");
            abort();
        }
        block->instructions = buffer;
    }

    if (instruction->insn_type == lstf_ir_instruction_type_branch) {
        lstf_ir_branchinstruction *branch = (lstf_ir_branchinstruction *)instruction;

        block->successors[0] = branch->taken;
        block->successors[1] = branch->not_taken;
    }

    block->instructions[block->instructions_length] = lstf_ir_node_ref(instruction);
    block->instructions_length++;
}
