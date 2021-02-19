#include "lstf-ir-function.h"
#include "lstf-ir-basicblock.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "lstf-ir-node.h"
#include "util.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void lstf_ir_function_destruct(lstf_ir_node *node)
{
    lstf_ir_function *fn = (lstf_ir_function *)node;

    free(fn->name);
    lstf_ir_node_unref(fn->entry_block);
    lstf_ir_node_unref(fn->exit_block);
    if (fn->basic_blocks)
        ptr_hashset_destroy(fn->basic_blocks);
}

lstf_ir_function *lstf_ir_function_new_for_userfn(const char *name,
                                                  uint8_t     parameters,
                                                  uint8_t     upvalues,
                                                  bool        has_result)
{
    lstf_ir_function *fn = calloc(1, sizeof *fn);

    if (!fn) {
        perror("failed to create IR function");
        abort();
    }

    lstf_ir_node_construct((lstf_ir_node *)fn, lstf_ir_function_destruct, lstf_ir_node_type_function);

    fn->name = strdup(name);
    fn->entry_block = lstf_ir_node_ref(lstf_ir_basicblock_new());
    fn->exit_block = lstf_ir_node_ref(lstf_ir_basicblock_new());
    fn->entry_block->successors[0] = fn->exit_block;
    fn->basic_blocks = ptr_hashset_new(ptrhash,
            (collection_item_ref_func) lstf_ir_node_ref, (collection_item_unref_func) lstf_ir_node_unref,
            NULL);
    fn->parameters = parameters;
    fn->upvalues = upvalues;
    fn->has_result = has_result;
    fn->is_vm_defined = false;

    ptr_hashset_insert(fn->basic_blocks, fn->entry_block);
    ptr_hashset_insert(fn->basic_blocks, fn->exit_block);

    return fn;
}

lstf_ir_function *lstf_ir_function_new_for_instruction(const char        *name,
                                                       uint8_t            parameters,
                                                       bool               has_result,
                                                       bool               does_return,
                                                       lstf_vm_opcode     vm_opcode,
                                                       lstf_vm_vmcallcode vm_callcode)
{
    lstf_ir_function *fn = calloc(1, sizeof *fn);

    if (!fn) {
        perror("failed to create IR function");
        abort();
    }

    lstf_ir_node_construct((lstf_ir_node *)fn, lstf_ir_function_destruct, lstf_ir_node_type_function);

    fn->name = strdup(name);
    fn->parameters = parameters;
    fn->has_result = has_result;
    fn->does_return = does_return;
    fn->vm_opcode = vm_opcode;
    fn->vm_callcode = vm_callcode;
    fn->is_vm_defined = true;

    return fn;
}

void lstf_ir_function_add_basic_block(lstf_ir_function *fn, lstf_ir_basicblock *block)
{
    assert(!ptr_hashset_contains(fn->basic_blocks, block) && "cannot insert the same basic block twice!");

    // inser the new block and reorder the exit block so that it always comes last
    ptr_hashset_delete(fn->basic_blocks, fn->exit_block);
    ptr_hashset_insert(fn->basic_blocks, block);
    ptr_hashset_insert(fn->basic_blocks, fn->exit_block);
}
