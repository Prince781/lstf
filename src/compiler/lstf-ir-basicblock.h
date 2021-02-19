#pragma once

#include "lstf-ir-instruction.h"
#include "lstf-ir-node.h"
#include <limits.h>
#include <stddef.h>

typedef struct _lstf_ir_basicblock lstf_ir_basicblock;
struct _lstf_ir_basicblock {
    lstf_ir_node parent_struct;
    lstf_ir_instruction **instructions;
    unsigned instructions_length : sizeof(unsigned) * CHAR_BIT - 1;
    bool serialized : 1;
    unsigned instructions_buffer_size;

    /**
     * A basic block can contain one successor if it does not end with a branch
     * instruction or it ends with an unconditional branch, or it can contain
     * two successors if it ends with a conditional branch.
     */
    lstf_ir_basicblock *successors[2];

    /**
     * Number of local variable deallocations (`pop`) needed at the end of this
     * basic block.
     */
    uint8_t variables_killed;
};

lstf_ir_basicblock *lstf_ir_basicblock_new(void);

/**
 * Adds an instruction to the basic block. An instruction cannot be added after
 * a branch.
 *
 * Returns the instruction added.
 */
void lstf_ir_basicblock_add_instruction(lstf_ir_basicblock  *block,
                                        lstf_ir_instruction *instruction);

static inline bool lstf_ir_basicblock_is_empty(const lstf_ir_basicblock *block)
{
    return block->instructions_length == 0;
}

static inline lstf_ir_instruction *lstf_ir_basicblock_get_last_instruction(const lstf_ir_basicblock *block)
{
    if (block->instructions_length == 0)
        return NULL;

    return block->instructions[block->instructions_length - 1];
}
