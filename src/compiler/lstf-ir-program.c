#include "lstf-ir-program.h"
#include "lstf-ir-basicblock.h"
#include "lstf-ir-instruction.h"
#include "lstf-report.h"
#include "bytecode/lstf-bc-function.h"
#include "bytecode/lstf-bc-instruction.h"
#include "bytecode/lstf-bc-program.h"
#include "data-structures/string-builder.h"
#include "data-structures/ptr-hashset.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "data-structures/intset.h"
#include "util.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
lstf_ir_program_free(lstf_ir_program *program)
{
    ptr_list_destroy(program->functions);
    free(program);
}

lstf_ir_program *lstf_ir_program_new(void)
{
    lstf_ir_program *program = calloc(1, sizeof *program);

    if (!program) {
        perror("failed to create IR program struct");
        abort();
    }

    program->floating = true;
    program->functions = ptr_list_new((collection_item_ref_func) lstf_ir_node_ref,
            (collection_item_unref_func) lstf_ir_node_unref);

    return program;
}

lstf_ir_program *lstf_ir_program_ref(lstf_ir_program *program)
{
    if (!program)
        return NULL;

    assert(program->floating || program->refcount > 0);

    if (program->floating) {
        program->floating = false;
        program->refcount = 1;
    } else {
        program->refcount++;
    }

    return program;
}

void lstf_ir_program_unref(lstf_ir_program *program)
{
    if (!program)
        return;

    assert(program->floating || program->refcount > 0);

    if (program->floating || --program->refcount == 0)
        lstf_ir_program_free(program);
}

void lstf_ir_program_add_function(lstf_ir_program *program,
                                  lstf_ir_function *function)
{
    ptr_list_append(program->functions, function);
}

typedef struct {
    intset *in;
    intset *out;
    intset *gen;
    intset *kill;
} lstf_ir_basicblock_sets;

static lstf_ir_basicblock_sets *lstf_ir_basicblock_sets_new(unsigned set_size)
{
    lstf_ir_basicblock_sets *sets = calloc(1, sizeof *sets);

    sets->in = intset_new(set_size);
    sets->out = intset_new(set_size);
    sets->gen = intset_new(set_size);
    sets->kill = intset_new(set_size);

    return sets;
}

static void lstf_ir_basicblock_sets_destroy(lstf_ir_basicblock_sets *sets)
{
    free(sets->in);
    free(sets->out);
    free(sets->gen);
    free(sets->kill);
    free(sets);
}

/**
 * Simplifies the control-flow graph of a function:
 *
 * - removes empty basic blocks
 */
static void lstf_ir_program_analysis_simplify_cfg(const lstf_ir_function *fn,
                                                  ptr_hashmap            *predecessors)
{
    if (fn->is_vm_defined)
        // don't perform the analysis in this case
        return;

    ptr_list *empty_bbs = ptr_list_new(lstf_ir_node_ref, lstf_ir_node_unref);

    for (iterator bb_it = ptr_hashset_iterator_create(fn->basic_blocks);
            bb_it.has_next; bb_it = iterator_next(bb_it)) {
        lstf_ir_basicblock *bb_empty = iterator_get_item(bb_it);

        if (lstf_ir_basicblock_is_empty(bb_empty) && bb_empty != fn->entry_block && bb_empty != fn->exit_block)
            ptr_list_append(empty_bbs, bb_empty);
    }

    // now remove these empty basic blocks
    for (iterator bb_empty_it = ptr_list_iterator_create(empty_bbs);
            bb_empty_it.has_next; bb_empty_it = iterator_next(bb_empty_it)) {
        lstf_ir_basicblock *bb_empty = iterator_get_item(bb_empty_it);
        const ptr_hashmap_entry *preds_entry = ptr_hashmap_get(predecessors, bb_empty);

        if (preds_entry) {
            assert(!(bb_empty->successors[0] && bb_empty->successors[1]) &&
                    "empty basic block with predecessors and two successors!");
            // update all predecessors to point to the successor
            ptr_hashset *preds = preds_entry->value;
            lstf_ir_basicblock *bb_succ =
                bb_empty->successors[0] ? bb_empty->successors[0] : bb_empty->successors[1];
            if (bb_succ) {
                // update the list of predecessors for the successor
                ptr_hashset *bb_succ_preds = ptr_hashmap_get(predecessors, bb_succ)->value;

                ptr_hashset_delete(bb_succ_preds, bb_empty);
                for (iterator pred_it = ptr_hashset_iterator_create(preds);
                        pred_it.has_next; pred_it = iterator_next(pred_it)) {
                    lstf_ir_basicblock *bb_pred = iterator_get_item(pred_it);

                    // this predecessor becomes a new predecessor of the successor
                    ptr_hashset_insert(bb_succ_preds, bb_pred);

                    // update the predecessor's successor to point to the successor
                    for (unsigned i = 0; i < sizeof(bb_pred->successors) / sizeof(bb_pred->successors[0]); i++) {
                        if (bb_pred->successors[i] == bb_empty)
                            bb_pred->successors[i] = bb_succ;
                    }

                    // the list of basic blocks is already topologically
                    // sorted, so the previous basic block should be non-empty
                    // unless it is a starting block like the entry and exit
                    // blocks (because otherwise it would already be removed)
                    if (!lstf_ir_basicblock_is_empty(bb_pred)) {
                        lstf_ir_instruction *last_inst = lstf_ir_basicblock_get_last_instruction(bb_pred);

                        if (last_inst->insn_type == lstf_ir_instruction_type_branch) {
                            lstf_ir_branchinstruction *branch_inst = (lstf_ir_branchinstruction *)last_inst;

                            if (branch_inst->taken == bb_empty)
                                branch_inst->taken = bb_succ;
                            if (branch_inst->not_taken == bb_empty)
                                branch_inst->not_taken = bb_succ;
                        }
                    }
                }
            }
        } else {
            // just remove us from the predecessors of our successors
            for (unsigned i = 0; i < sizeof(bb_empty->successors) / sizeof(bb_empty->successors[0]); i++) {
                if (!bb_empty->successors[i])
                    continue;
                ptr_hashset *bb_succ_preds = ptr_hashmap_get(predecessors, bb_empty->successors[i])->value;
                ptr_hashset_delete(bb_succ_preds, bb_empty);
            }
        }

        // remove the basic block from the function
        for (unsigned i = 0; i < sizeof(bb_empty->successors) / sizeof(bb_empty->successors[0]); i++)
            bb_empty->successors[i] = NULL;

        ptr_hashmap_delete(predecessors, bb_empty);
        ptr_hashset_delete(fn->basic_blocks, bb_empty);
    }

    ptr_list_destroy(empty_bbs);
}

/**
 * Note: this is NOT live variable analysis, although it looks a bit similar.
 * The differences are:
 *
 * - this is a forwards, not a backwards analysis
 * - this determines the point at which we can insert a stack pop, which is at
 *   least when the local is dead but may be delayed until after temporaries
 *   have been consumed
 */
static void lstf_ir_program_analysis_stack_pop_points(const lstf_ir_function  *fn,
                                                      const ptr_hashmap       *predecessors)
{
    if (fn->is_vm_defined)
        // don't perform the analysis in this case
        return;

    // type: `ptr_hashmap<lstf_ir_basicblock *, lstf_ir_basicblock_sets *>`
    ptr_hashmap *analysis_sets = ptr_hashmap_new(ptrhash,
            lstf_ir_node_ref, lstf_ir_node_unref,
            NULL,
            NULL, (collection_item_unref_func) lstf_ir_basicblock_sets_destroy);
    // type: `ptr_hashmap<unsigned, lstf_ir_instruction *>`
    ptr_hashmap *ints_to_allocs = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, lstf_ir_node_ref, lstf_ir_node_unref);
    unsigned num_allocs = 0;

    // initialize the sets
    // printf("DFA initialization (%u locals) ---\n", fn->num_locals);
    for (iterator bb_it = ptr_hashset_iterator_create(fn->basic_blocks);
            bb_it.has_next; bb_it = iterator_next(bb_it)) {
        lstf_ir_basicblock *bb = iterator_get_item(bb_it);
        if (bb == fn->exit_block)
            continue;
        lstf_ir_basicblock_sets *bb_var_sets = lstf_ir_basicblock_sets_new(fn->num_locals);

        for (unsigned i = 0; i < bb->instructions_length; i++) {
            lstf_ir_instruction *inst = bb->instructions[i];
            
            if (inst->insn_type == lstf_ir_instruction_type_alloc &&
                    ((lstf_ir_allocinstruction *)inst)->initializer) {
                ptr_hashmap_insert(ints_to_allocs, (void *)(uintptr_t)num_allocs, inst);
                intset_set(bb_var_sets->gen, num_allocs);
                num_allocs++;
            }
        }

        // char *repr = intset_to_string(bb_var_sets->gen);
        // printf("  GEN[%lu] = %s\n", (unsigned long) bb_it.counter, repr);
        // free(repr);
        // repr = intset_to_string(bb_var_sets->kill);
        // printf("  KILL[%lu] = %s\n", (unsigned long) bb_it.counter, repr);
        // free(repr);

        ptr_hashmap_insert(analysis_sets, bb, bb_var_sets);
    }

    // iterate until steady state
    bool sets_changed = true;
    while (sets_changed) {
        // printf("DFA iteration ---\n");
        sets_changed = false;

        for (iterator bb_it = ptr_hashset_iterator_create(fn->basic_blocks);
                bb_it.has_next; bb_it = iterator_next(bb_it)) {
            lstf_ir_basicblock *bb = iterator_get_item(bb_it);
            if (bb == fn->exit_block)
                continue;
            lstf_ir_basicblock_sets *bb_var_sets = ptr_hashmap_get(analysis_sets, bb)->value;
            const unsigned old_out_count = intset_count(bb_var_sets->out);

            // IN = ∩ OUT[p] for all predecessors p
            const ptr_hashmap_entry *preds_set_entry = ptr_hashmap_get(predecessors, bb);
            if (preds_set_entry) {
                ptr_hashset *preds_set = preds_set_entry->value;
                for (iterator pred_it = ptr_hashset_iterator_create(preds_set);
                        pred_it.has_next; pred_it = iterator_next(pred_it)) {
                    const lstf_ir_basicblock *bb_pred = iterator_get_item(pred_it);
                    const lstf_ir_basicblock_sets *bb_pred_var_sets = ptr_hashmap_get(analysis_sets, bb_pred)->value;

                    if (pred_it.is_first)
                        intset_copy(bb_var_sets->in, bb_pred_var_sets->out);
                    else
                        intset_intersect(bb_var_sets->in, bb_pred_var_sets->out);
                }
            }

            // OUT = (IN ∪ GEN) - KILL
            intset_copy(bb_var_sets->out, bb_var_sets->in);
            intset_union(bb_var_sets->out, bb_var_sets->gen);
            intset_minus(bb_var_sets->out, bb_var_sets->kill);

            // char *repr = intset_to_string(bb_var_sets->in);
            // printf("  IN[%lu] = %s\n", (unsigned long) bb_it.counter, repr);
            // free(repr);
            // repr = intset_to_string(bb_var_sets->out);
            // printf("  OUT[%lu] = %s\n", (unsigned long) bb_it.counter, repr);
            // free(repr);

            if (intset_count(bb_var_sets->out) != old_out_count)
                sets_changed = true;
        }
    }

    // now we know the locations of the deallocations: wherever an element is
    // present in the (OUT ∪ GEN) set of a basic block but not present in
    // either of the basic block's successors' IN sets
    intset *locals_killed = intset_new(fn->num_locals);
    intset *locals_alive_in_successors = intset_new(fn->num_locals);
    // printf("Post-analysis ---\n");
    for (iterator bb_it = ptr_hashset_iterator_create(fn->basic_blocks); bb_it.has_next; bb_it = iterator_next(bb_it)) {
        lstf_ir_basicblock *bb = iterator_get_item(bb_it);
        if (bb == fn->exit_block)
            continue;
        const lstf_ir_basicblock_sets *bb_var_sets = ptr_hashmap_get(analysis_sets, bb)->value;

        intset_copy(locals_killed, bb_var_sets->out);
        intset_union(locals_killed, bb_var_sets->gen);
        intset_clear(locals_alive_in_successors);

        for (unsigned s = 0; s < sizeof(bb->successors) / sizeof(bb->successors[0]); s++) {
            if (!bb->successors[s] || bb->successors[s] == fn->exit_block)
                continue;
            const lstf_ir_basicblock_sets *bb_succ_var_sets =
                ptr_hashmap_get(analysis_sets, bb->successors[s])->value;
            intset_union(locals_alive_in_successors, bb_succ_var_sets->in);
        }

        intset_invert(locals_alive_in_successors);
        intset_intersect(locals_killed, locals_alive_in_successors);

        // char *repr = intset_to_string(locals_killed);
        // printf("  LOCALS_KILLED[%lu] = %s\n", (unsigned long)bb_it.counter, repr);
        // free(repr);

        bb->variables_killed = intset_count(locals_killed);
    }
    free(locals_killed);
    free(locals_alive_in_successors);

    ptr_hashmap_destroy(analysis_sets);
    ptr_hashmap_destroy(ints_to_allocs);
}

unsigned lstf_ir_program_analyze(lstf_ir_program *program)
{
    // --- compute predecessors:
    // type: `ptr_hashmap<lstf_ir_basicblock *, ptr_hashset<lstf_ir_basicblock *>>`
    ptr_hashmap *predecessors = ptr_hashmap_new(ptrhash,
            lstf_ir_node_ref, lstf_ir_node_unref,
            NULL,
            NULL, (collection_item_unref_func) ptr_hashset_destroy);

    // compute predecessors
    for (iterator fn_it = ptr_list_iterator_create(program->functions); fn_it.has_next; fn_it = iterator_next(fn_it)) {
        lstf_ir_function *fn = iterator_get_item(fn_it);
        if (fn->is_vm_defined)
            continue;
        for (iterator bb_pred_it = ptr_hashset_iterator_create(fn->basic_blocks);
                bb_pred_it.has_next; bb_pred_it = iterator_next(bb_pred_it)) {
            // the predecessor
            lstf_ir_basicblock *bb_pred = iterator_get_item(bb_pred_it);

            for (unsigned i = 0; i < sizeof(bb_pred->successors) / sizeof(bb_pred->successors[0]); i++) {
                if (!bb_pred->successors[i])
                    continue;

                lstf_ir_basicblock *bb = bb_pred->successors[i];
                const ptr_hashmap_entry *preds_set_entry = ptr_hashmap_get(predecessors, bb);

                if (!preds_set_entry) {
                    preds_set_entry = ptr_hashmap_insert(predecessors, bb,
                            ptr_hashset_new(ptrhash, lstf_ir_node_ref, lstf_ir_node_unref, NULL));
                }

                ptr_hashset *preds_set = preds_set_entry->value;
                ptr_hashset_insert(preds_set, bb_pred);
            }
        }
    }

    // run analyses
    for (iterator fn_it = ptr_list_iterator_create(program->functions); fn_it.has_next; fn_it = iterator_next(fn_it)) {
        const lstf_ir_function *fn = iterator_get_item(fn_it);

        lstf_ir_program_analysis_simplify_cfg(fn, predecessors);
        lstf_ir_program_analysis_stack_pop_points(fn, predecessors);
    }

    ptr_hashmap_destroy(predecessors);

    return 0;
}

/**
 * Modifies [bb] and sets variables_killed to 0.
 */
static void lstf_ir_program_add_deallocations_for_basic_block(lstf_ir_basicblock *bb,
                                                              lstf_bc_function   *bc_fn)
{
    for (unsigned i = 0; i < bb->variables_killed; i++)
        lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_pop_new());
    bb->variables_killed = 0;
}

static void lstf_ir_program_serialize_basic_block(lstf_ir_program    *ir,
                                                  lstf_ir_basicblock *bb,
                                                  lstf_ir_function   *ir_fn,
                                                  lstf_bc_program    *bc,
                                                  lstf_bc_function   *bc_fn,
                                                  ptr_hashmap        *ir_target_to_bc_branches,
                                                  ptr_hashmap        *bbs_ir_to_bcinsn,
                                                  ptr_hashmap        *fns_ir_to_bc,
                                                  int                 frame_offset)
{
    if (bb->serialized || bb == ir_fn->exit_block)
        return;

    assert(bb->instructions_length > 0 && "cannot serialize empty basic block!");

    for (unsigned i = 0; i < bb->instructions_length; i++) {
        lstf_ir_instruction *inst = bb->instructions[i];
        lstf_bc_instruction *bc_inst = NULL;

        if (bb->variables_killed &&
                (inst->insn_type == lstf_ir_instruction_type_branch ||
                 (inst->insn_type == lstf_ir_instruction_type_return &&
                  !((lstf_ir_returninstruction *)inst)->value)))
            // On function return, the stack is popped automatically. If there
            // is an element at the top of the stack that is not a parameter,
            // this becomes the return value.  Therefore we only want to have
            // the stack empty if this function does *not* return anything.
            // When a local variable has been created within the scope of a
            // block, we want to pop it before leaving the scope.
            lstf_ir_program_add_deallocations_for_basic_block(bb, bc_fn);

        switch (inst->insn_type) {
        case lstf_ir_instruction_type_phi:
            // this is a pseudo-instruction
            inst->frame_offset = frame_offset;
            for (iterator arg_it = ptr_list_iterator_create(((lstf_ir_phiinstruction *)inst)->arguments);
                    arg_it.has_next; arg_it = iterator_next(arg_it)) {
                lstf_ir_instruction *arg = iterator_get_item(arg_it);
                assert(arg->frame_offset == frame_offset && "phi args must all be at the same location on the stack");
            }
            break;

        case lstf_ir_instruction_type_alloc:
        {   // this is a pseudo-instruction
            lstf_ir_allocinstruction *alloc_inst = (lstf_ir_allocinstruction *)inst;
            if (alloc_inst->initializer) {
                assert(alloc_inst->initializer->frame_offset+1 == frame_offset &&
                        "variable alloc must alias previous instruction!");
                inst->frame_offset = alloc_inst->initializer->frame_offset;
            } else {
                inst->frame_offset = frame_offset++;
            }

            // lstf_report_note(&inst->code_node->source_reference,
            //         "frame(%d) = alloc", inst->frame_offset);
        }   break;

        case lstf_ir_instruction_type_load:
        {
            inst->frame_offset = frame_offset++;

            lstf_ir_loadinstruction *load_inst = (lstf_ir_loadinstruction *)inst;
            // lstf_report_note(&inst->code_node->source_reference,
            //         "frame(%d) = load frame(%d)",
            //         inst->frame_offset,  load_inst->source->frame_offset);
            bc_inst = lstf_bc_function_add_instruction(bc_fn,
                    lstf_bc_instruction_load_frameoffset_new(load_inst->source->frame_offset));
        }   break;

        case lstf_ir_instruction_type_store:
        {
            frame_offset--;
            lstf_ir_storeinstruction *store_inst = (lstf_ir_storeinstruction *)inst;

            assert(store_inst->destination->insn_type == lstf_ir_instruction_type_alloc &&
                    "store instruction must store to a local variable");
            assert(store_inst->source->frame_offset == frame_offset &&
                    "source is not on top of the stack for store instruction!");
            bc_inst = lstf_bc_function_add_instruction(bc_fn,
                    lstf_bc_instruction_store_new(store_inst->destination->frame_offset));
        }   break;

        case lstf_ir_instruction_type_constant:
        {
            inst->frame_offset = frame_offset++;
            lstf_ir_constantinstruction *cinst = (lstf_ir_constantinstruction *)inst;

            // char *json_str = json_node_to_string(cinst->json, false);
            // lstf_report_note(&inst->code_node->source_reference,
            //         "frame(%d) = constant %s", inst->frame_offset, json_str);
            // free(json_str);

            bc_inst = lstf_bc_function_add_instruction(bc_fn,
                    lstf_bc_instruction_load_expression_new(cinst->json));
        }   break;

        case lstf_ir_instruction_type_binary:
        {
            frame_offset -= 2;
            inst->frame_offset = frame_offset++;
            lstf_ir_binaryinstruction *binst = (lstf_ir_binaryinstruction *)inst;

            switch (binst->opcode) {
            case lstf_vm_op_add:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_add_new());
                break;
            case lstf_vm_op_div:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_div_new());
                break;
            case lstf_vm_op_mul:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_mul_new());
                break;
            case lstf_vm_op_sub:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_sub_new());
                break;
            case lstf_vm_op_pow:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_pow_new());
                break;
            case lstf_vm_op_land:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_land_new());
                break;
            case lstf_vm_op_lor:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_lor_new());
                break;
            case lstf_vm_op_lnot:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_lnot_new());
                break;
            case lstf_vm_op_lessthan:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_lessthan_new());
                break;
            case lstf_vm_op_lessthan_equal:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_lessthan_equal_new());
                break;
            case lstf_vm_op_equal:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_equal_new());
                break;
            case lstf_vm_op_greaterthan:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_greaterthan_new());
                break;
            case lstf_vm_op_greaterthan_equal:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_greaterthan_equal_new());
                break;
            case lstf_vm_op_mod:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_mod_new());
                break;
            case lstf_vm_op_and:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_and_new());
                break;
            case lstf_vm_op_or:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_or_new());
                break;
            case lstf_vm_op_xor:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_xor_new());
                break;
            case lstf_vm_op_lshift:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_lshift_new());
                break;
            case lstf_vm_op_rshift:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_rshift_new());
                break;
            case lstf_vm_op_in:
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_in_new());
                break;
            case lstf_vm_op_load_dataoffset:
            case lstf_vm_op_load_frameoffset:
            case lstf_vm_op_load_codeoffset:
            case lstf_vm_op_load_expression:
            case lstf_vm_op_store:
            case lstf_vm_op_get:
            case lstf_vm_op_set:
            case lstf_vm_op_pop:
            case lstf_vm_op_params:
            case lstf_vm_op_call:
            case lstf_vm_op_calli:
            case lstf_vm_op_schedule:
            case lstf_vm_op_schedulei:
            case lstf_vm_op_return:
            case lstf_vm_op_closure:
            case lstf_vm_op_upget:
            case lstf_vm_op_upset:
            case lstf_vm_op_vmcall:
            case lstf_vm_op_else:
            case lstf_vm_op_jump:
            case lstf_vm_op_neg:
            case lstf_vm_op_not:
            case lstf_vm_op_bool:
            case lstf_vm_op_print:
            case lstf_vm_op_exit:
            case lstf_vm_op_assert:
                fprintf(stderr, "%s: unreachable code: unexpected op `%u' for binary IR instruction\n",
                        __func__, binst->opcode);
                abort();
            }

        }   break;

        case lstf_ir_instruction_type_unary:
        {
            frame_offset--;
            inst->frame_offset = frame_offset++;
            lstf_ir_unaryinstruction *uinst = (lstf_ir_unaryinstruction *)inst;

            switch (uinst->opcode) {
                case lstf_vm_op_not:
                    bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_not_new());
                    break;
                case lstf_vm_op_lnot:
                    bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_lnot_new());
                    break;
                case lstf_vm_op_neg:
                    bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_neg_new());
                    break;
                case lstf_vm_op_bool:
                    bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_bool_new());
                    break;
                case lstf_vm_op_add:
                case lstf_vm_op_and:
                case lstf_vm_op_call:
                case lstf_vm_op_calli:
                case lstf_vm_op_closure:
                case lstf_vm_op_div:
                case lstf_vm_op_else:
                case lstf_vm_op_equal:
                case lstf_vm_op_exit:
                case lstf_vm_op_get:
                case lstf_vm_op_greaterthan:
                case lstf_vm_op_greaterthan_equal:
                case lstf_vm_op_in:
                case lstf_vm_op_jump:
                case lstf_vm_op_land:
                case lstf_vm_op_lessthan:
                case lstf_vm_op_lessthan_equal:
                case lstf_vm_op_load_codeoffset:
                case lstf_vm_op_load_dataoffset:
                case lstf_vm_op_load_expression:
                case lstf_vm_op_load_frameoffset:
                case lstf_vm_op_lor:
                case lstf_vm_op_lshift:
                case lstf_vm_op_mod:
                case lstf_vm_op_mul:
                case lstf_vm_op_or:
                case lstf_vm_op_params:
                case lstf_vm_op_pop:
                case lstf_vm_op_pow:
                case lstf_vm_op_print:
                case lstf_vm_op_return:
                case lstf_vm_op_rshift:
                case lstf_vm_op_schedule:
                case lstf_vm_op_schedulei:
                case lstf_vm_op_set:
                case lstf_vm_op_store:
                case lstf_vm_op_sub:
                case lstf_vm_op_upget:
                case lstf_vm_op_upset:
                case lstf_vm_op_vmcall:
                case lstf_vm_op_xor:
                case lstf_vm_op_assert:
                    fprintf(stderr, "%s: unreachable code: unexpected op `%u' for unary IR instruction\n",
                            __func__, uinst->opcode);
                    abort();
            }
        }   break;

        case lstf_ir_instruction_type_call:
        {
            lstf_ir_callinstruction *call = (lstf_ir_callinstruction *)inst;

            frame_offset -= call->arguments->length;
            if (call->function->has_result)
                inst->frame_offset = frame_offset++;

            if (call->function->is_vm_defined) {
                // TODO: serialize more instructions from opcode
                switch (call->function->vm_opcode) {
                    case lstf_vm_op_print:
                        bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_print_new());
                        break;
                    default:
                        fprintf(stderr, "%s: cannot serialize `%s' with VM opcode %u to bytecode\n",
                                __func__, call->function->name, call->function->vm_opcode);
                        abort();
                }
            } else {
                lstf_bc_function *called_bc_fn = ptr_hashmap_get(fns_ir_to_bc, call->function)->value;
                bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_call_new(called_bc_fn));
            }
        }   break;

        case lstf_ir_instruction_type_indirectcall:
        {
            lstf_ir_indirectcallinstruction *icall = (lstf_ir_indirectcallinstruction *)inst;
            
            frame_offset -= icall->arguments->length + 1 /* the function pointer or closure */;
            if (icall->has_return)
                inst->frame_offset = frame_offset++;

            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_calli_new());
        }   break;

        case lstf_ir_instruction_type_schedule:
        {
            lstf_ir_scheduleinstruction *sched_inst = (lstf_ir_scheduleinstruction *)inst;

            frame_offset -= sched_inst->arguments->length;
            if (sched_inst->function->has_result)
                inst->frame_offset = frame_offset++;

            lstf_bc_function *scheduled_bc_fn = ptr_hashmap_get(fns_ir_to_bc, sched_inst->function)->value;
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_schedule_new(scheduled_bc_fn));
        }   break;

        case lstf_ir_instruction_type_indirectschedule:
        {
            lstf_ir_indirectscheduleinstruction *isched_inst = (lstf_ir_indirectscheduleinstruction *)inst;

            frame_offset -= isched_inst->arguments->length + 1 /* the function pointer or closure */;
            if (isched_inst->has_return)
                inst->frame_offset = frame_offset++;

            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_schedulei_new());
        }   break;

        case lstf_ir_instruction_type_branch:
        {
            lstf_ir_branchinstruction *binst = (lstf_ir_branchinstruction *)inst;
            const ptr_hashmap_entry *jump_target_entry = NULL;
            lstf_ir_basicblock *bb_jump_target = NULL;
            lstf_bc_instruction jump_bc;
            if (binst->source) {
                bb_jump_target = binst->not_taken;
                jump_target_entry = ptr_hashmap_get(bbs_ir_to_bcinsn, bb_jump_target);
                jump_bc = lstf_bc_instruction_else_new(jump_target_entry ? jump_target_entry->value : NULL);

                // frame value is popped, but no value is pushed
                frame_offset--;
            } else {
                bb_jump_target = binst->taken;
                jump_target_entry = ptr_hashmap_get(bbs_ir_to_bcinsn, bb_jump_target);
                jump_bc = lstf_bc_instruction_jump_new(jump_target_entry ? jump_target_entry->value : NULL);
            }

            bc_inst = lstf_bc_function_add_instruction(bc_fn, jump_bc);
            if (!jump_target_entry) {
                // jump is unresolved
                // append to map of unresolved jumps
                if (!ptr_hashmap_get(ir_target_to_bc_branches, bb_jump_target))
                    ptr_hashmap_insert(ir_target_to_bc_branches, bb_jump_target, ptr_list_new(NULL, NULL));
                ptr_list *jump_bc_insns = ptr_hashmap_get(ir_target_to_bc_branches, bb_jump_target)->value;
                ptr_list_append(jump_bc_insns, bc_inst);
            }
        }   break;

        case lstf_ir_instruction_type_closure:
        {
            inst->frame_offset = frame_offset++;

            lstf_ir_closureinstruction *cl_inst = (lstf_ir_closureinstruction *)inst;
            lstf_bc_upvalue upvalues[LSTF_VM_MAX_CAPTURES] = { 0 };

            // lstf_report_note(&inst->code_node->source_reference,
            //         "frame(%d) = closure @%s, <environment ...>", inst->frame_offset, cl_inst->fn->name);

            for (iterator it = ptr_list_iterator_create(cl_inst->captures); it.has_next; it = iterator_next(it)) {
                const lstf_ir_captured *captured = iterator_get_item(it);
                upvalues[it.counter] = (lstf_bc_upvalue) {
                    .is_local = captured->is_local,
                    .index = captured->is_local ? captured->local->frame_offset : captured->upvalue_id
                };
            }

            lstf_bc_function *closure_bc_fn = ptr_hashmap_get(fns_ir_to_bc, cl_inst->fn)->value;
            bc_inst = lstf_bc_function_add_instruction(bc_fn,
                    lstf_bc_instruction_closure_new(closure_bc_fn, cl_inst->captures->length, upvalues));
        }   break;

        case lstf_ir_instruction_type_setupvalue:
        {
            frame_offset--;
            lstf_ir_setupvalueinstruction *suv_inst = (lstf_ir_setupvalueinstruction *)inst;

            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_upset_new(suv_inst->upvalue_id));
        }   break;

        case lstf_ir_instruction_type_getupvalue:
        {
            inst->frame_offset = frame_offset++;

            lstf_ir_getupvalueinstruction *guv_inst = (lstf_ir_getupvalueinstruction *)inst;

            // lstf_report_note(&inst->code_node->source_reference,
            //         "frame(%d) = getupvalue(%u)", inst->frame_offset, guv_inst->upvalue_id);

            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_upget_new(guv_inst->upvalue_id));
        }   break;

        case lstf_ir_instruction_type_getelement:
        {
            frame_offset -= 2;
            inst->frame_offset = frame_offset++;
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_get_new());
        }   break;

        case lstf_ir_instruction_type_setelement:
        {
            frame_offset -= 3;
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_set_new());
        }   break;

        case lstf_ir_instruction_type_match:
        {
            frame_offset -= 2;
            inst->frame_offset = frame_offset++;
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_equal_new());
        }   break;

        case lstf_ir_instruction_type_loadfunction:
        {
            inst->frame_offset = frame_offset++;

            lstf_ir_loadfunctioninstruction *lfi = (lstf_ir_loadfunctioninstruction *) inst;
            assert(!lfi->function->is_vm_defined && "cannot load built-in function!");
            lstf_bc_function *function_ref = ptr_hashmap_get(fns_ir_to_bc, lfi->function)->value;
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_load_codeoffset_new(function_ref));
        }   break;

        case lstf_ir_instruction_type_return:
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_return_new());
            break;

        case lstf_ir_instruction_type_assert:
            bc_inst = lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_assert_new());
            break;

        case lstf_ir_instruction_type_append:
            fprintf(stderr, "%s: unsupported IR instruction `%u`\n", __func__, inst->insn_type);
            abort();
        }

        // map this basic block to the first bytecode instruction that was
        // serialized for this basic block
        if (bc_inst && !ptr_hashmap_get(bbs_ir_to_bcinsn, bb)) {
            ptr_hashmap_insert(bbs_ir_to_bcinsn, bb, bc_inst);
            // check whether this instruction is the target of multiple branches
            if (ptr_hashmap_get(ir_target_to_bc_branches, bb)) {
                // this is a `ptr_list<lstf_bc_instruction *>`
                ptr_list *bc_branches = ptr_hashmap_get(ir_target_to_bc_branches, bb)->value;
                // resolve those branches
                for (iterator it = ptr_list_iterator_create(bc_branches); it.has_next; it = iterator_next(it))
                    lstf_bc_instruction_resolve_jump(iterator_get_item(it), bc_inst);
            }
        }
    }

    if (bb->variables_killed)
        lstf_ir_program_add_deallocations_for_basic_block(bb, bc_fn);

    // add an unconditional jump when we fall through to our successor
    if (bb->successors[0] && bb->successors[0]->serialized) {
        lstf_ir_instruction *last_inst =
            bb->instructions_length > 0 ? bb->instructions[bb->instructions_length - 1] : NULL;
        if (last_inst->insn_type != lstf_ir_instruction_type_branch) {
            // since our successor is already serialized, there already should be a target
            const ptr_hashmap_entry *jump_target_entry = ptr_hashmap_get(bbs_ir_to_bcinsn, bb->successors[0]);
            lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_jump_new(jump_target_entry->value));
        }
    }

    bb->serialized = true;

    // visit the successors
    if (bb->successors[0])
        lstf_ir_program_serialize_basic_block(ir, bb->successors[0], ir_fn, bc, bc_fn,
                ir_target_to_bc_branches, bbs_ir_to_bcinsn, fns_ir_to_bc, frame_offset);
    if (bb->successors[1])
        lstf_ir_program_serialize_basic_block(ir, bb->successors[1], ir_fn, bc, bc_fn,
                ir_target_to_bc_branches, bbs_ir_to_bcinsn, fns_ir_to_bc, frame_offset);
}

static void lstf_ir_program_serialize_function(lstf_ir_program  *ir,
                                               lstf_ir_function *ir_fn,
                                               lstf_bc_program  *bc,
                                               lstf_bc_function *bc_fn,
                                               ptr_hashmap      *fns_ir_to_bc)
{
    if (ir_fn->is_vm_defined)
        return;

    // maps (lstf_ir_basicblock *) -> (referents: ptr_list<lstf_bc_instruction *>)
    ptr_hashmap *ir_target_to_bc_branches = ptr_hashmap_new(ptrhash, NULL, NULL,
            NULL, NULL, (collection_item_unref_func) ptr_list_destroy);
    // maps (lstf_ir_basicblock *) -> (first bytecode instruction: lstf_bc_instruction *)
    ptr_hashmap *bbs_ir_to_bcinsn = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);

    lstf_bc_function_add_instruction(bc_fn, lstf_bc_instruction_params_new(ir_fn->parameters));
    lstf_ir_program_serialize_basic_block(ir, ir_fn->entry_block->successors[0], ir_fn, bc, bc_fn,
            ir_target_to_bc_branches, bbs_ir_to_bcinsn, fns_ir_to_bc, 0);

    ptr_hashmap_destroy(ir_target_to_bc_branches);
    ptr_hashmap_destroy(bbs_ir_to_bcinsn);
}

lstf_bc_program *lstf_ir_program_assemble(lstf_ir_program *program)
{
    lstf_bc_program *bc = lstf_bc_program_new(NULL);
    ptr_hashmap *fns_ir_to_bc = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);

    // create the bytecode functions first so that call instructions can
    // resolve their targets
    for (iterator it = ptr_list_iterator_create(program->functions);
            it.has_next; it = iterator_next(it)) {
        lstf_ir_function *ir_function = iterator_get_item(it);
        if (ir_function->is_vm_defined)
            continue;
        lstf_bc_function *bc_function = lstf_bc_function_new(ir_function->name);
        ptr_hashmap_insert(fns_ir_to_bc, ir_function, bc_function);
        lstf_bc_program_add_function(bc, bc_function);
    }

    // now fill the functions with bytecode translated from IR
    for (iterator it = ptr_list_iterator_create(program->functions);
            it.has_next; it = iterator_next(it)) {
        lstf_ir_function *ir_function = iterator_get_item(it);
        if (ir_function->is_vm_defined)
            continue;
        lstf_bc_function *bc_function = ptr_hashmap_get(fns_ir_to_bc, ir_function)->value;
        lstf_ir_program_serialize_function(program, ir_function, bc, bc_function, fns_ir_to_bc);
    }

    ptr_hashmap_destroy(fns_ir_to_bc);
    return bc;
}

bool lstf_ir_program_visualize(const lstf_ir_program *program, const char *path)
{
    FILE *dotfile = fopen(path, "w");

    if (!dotfile)
        return false;

    string *bb_insns_buffer = string_new();
    fprintf(dotfile, "digraph G {\n");
    fprintf(dotfile, "    node [shape=\"box\" fontname=\"monospace\" fontsize=5];\n");
    fprintf(dotfile, "    edge [fontsize=5];\n");
    for (iterator fn_it = ptr_list_iterator_create(program->functions); fn_it.has_next; fn_it = iterator_next(fn_it)) {
        const lstf_ir_function *fn = iterator_get_item(fn_it);
        // ptr_hashmap<lstf_ir_basicblock *, char *>
        ptr_hashmap *bb_names = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, (collection_item_ref_func) strdup, free);
        // ptr_hashmap<lstf_ir_instruction *, unsigned long>
        ptr_hashmap *insn_result_ids = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);
        unsigned long num_instructions = 0;

        // generate the names for the basic blocks first
        if (fn->basic_blocks) {
            for (iterator bb_it = ptr_hashset_iterator_create(fn->basic_blocks); bb_it.has_next; bb_it = iterator_next(bb_it)) {
                lstf_ir_basicblock *bb = iterator_get_item(bb_it);
                char bb_name[32] = { 0 };

                if (bb == fn->entry_block)
                    snprintf(bb_name, sizeof bb_name - 1, "bb_entry");
                else if (bb == fn->exit_block)
                    snprintf(bb_name, sizeof bb_name - 1, "bb_exit");
                else
                    snprintf(bb_name, sizeof bb_name - 1, "bb_%lu", (unsigned long)bb_it.counter);

                ptr_hashmap_insert(bb_names, bb, bb_name);
            }
        }

        fprintf(dotfile, "    subgraph \"%s\" {\n", fn->name);

        if (fn->basic_blocks) {
            for (iterator bb_it = ptr_hashset_iterator_create(fn->basic_blocks); bb_it.has_next; bb_it = iterator_next(bb_it)) {
                string_clear(bb_insns_buffer);
                const lstf_ir_basicblock *bb = iterator_get_item(bb_it);

                for (unsigned long i = 0; i < bb->instructions_length; i++) {
                    if (lstf_ir_instruction_has_result(bb->instructions[i]))
                        ptr_hashmap_insert(insn_result_ids, bb->instructions[i], (void *)(uintptr_t)num_instructions);

                    switch (bb->instructions[i]->insn_type) {
                        case lstf_ir_instruction_type_alloc:
                        {
                            lstf_ir_allocinstruction *alloc_inst = (lstf_ir_allocinstruction *)bb->instructions[i];
                            if (alloc_inst->initializer) {
                                unsigned long initializer_i =
                                    (uintptr_t)ptr_hashmap_get(insn_result_ids, alloc_inst->initializer)->value;
                                string_appendf(bb_insns_buffer, "%%%lu = alloc var %%%lu\\n", num_instructions, initializer_i);
                            } else {
                                string_appendf(bb_insns_buffer, "%%%lu = alloc param\\n", num_instructions);
                            }
                        } break;

                        case lstf_ir_instruction_type_append:
                        {
                            lstf_ir_appendinstruction *append_inst = (lstf_ir_appendinstruction *)bb->instructions[i];
                            unsigned long container_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, append_inst->container)->value;
                            unsigned long value_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, append_inst->value)->value;

                            string_appendf(bb_insns_buffer, "append %%%lu, %%%lu\\n", container_i, value_i);
                        }   break;

                        case lstf_ir_instruction_type_assert:
                        {
                            lstf_ir_assertinstruction *assert_inst = (lstf_ir_assertinstruction *)bb->instructions[i];
                            unsigned long source_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, assert_inst->source)->value;

                            string_appendf(bb_insns_buffer, "assert %%%lu\\n", source_i);
                        }   break;

                        case lstf_ir_instruction_type_branch:
                        {
                            lstf_ir_branchinstruction *branch_inst = (lstf_ir_branchinstruction *)bb->instructions[i];
                            const char *bb_taken_name = ptr_hashmap_get(bb_names, branch_inst->taken)->value;
                            const char *bb_not_taken_name = branch_inst->not_taken ?
                                ptr_hashmap_get(bb_names, branch_inst->not_taken)->value : NULL;

                            if (bb_not_taken_name) {
                                const unsigned long source_i =
                                    (uintptr_t)ptr_hashmap_get(insn_result_ids, branch_inst->source)->value;
                                string_appendf(bb_insns_buffer, "branch %%%lu, %s_%s, %s_%s\\n",
                                        source_i, fn->name, bb_taken_name, fn->name, bb_not_taken_name);
                            } else {
                                string_appendf(bb_insns_buffer, "branch %s\\n", bb_taken_name);
                            }
                        }   break;

                        case lstf_ir_instruction_type_binary:
                        {
                            lstf_ir_binaryinstruction *binary_inst = (lstf_ir_binaryinstruction *)bb->instructions[i];
                            unsigned long source0_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, binary_inst->sources[0])->value;
                            unsigned long source1_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, binary_inst->sources[1])->value;

                            string_appendf(bb_insns_buffer, "%%%lu = %s, %%%lu, %%%lu\\n",
                                    num_instructions,
                                    lstf_vm_opcode_to_string(binary_inst->opcode),
                                    source0_i,
                                    source1_i);
                        }   break;

                        case lstf_ir_instruction_type_constant:
                        {
                            lstf_ir_constantinstruction *const_inst = (lstf_ir_constantinstruction *)bb->instructions[i];
                            
                            char *repr = json_node_to_string(const_inst->json, false);
                            char *repr_escaped = json_string_escape(repr);
                            string_appendf(bb_insns_buffer, "%%%lu = constant %s\\n", num_instructions, repr_escaped);
                            free(repr);
                            free(repr_escaped);
                        }   break;

                        case lstf_ir_instruction_type_call:
                        {
                            lstf_ir_callinstruction *call_inst = (lstf_ir_callinstruction *)bb->instructions[i];

                            if (call_inst->function->has_result)
                                string_appendf(bb_insns_buffer, "%%%lu = ", num_instructions);
                            string_appendf(bb_insns_buffer, "call @%s", call_inst->function->name);
                            for (iterator arg_it = ptr_list_iterator_create(call_inst->arguments);
                                    arg_it.has_next; arg_it = iterator_next(arg_it)) {
                                string_appendf(bb_insns_buffer, ", %%%lu",
                                        (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, iterator_get_item(arg_it))->value);
                            }
                            string_appendf(bb_insns_buffer, "\\n");
                        }   break;

                        case lstf_ir_instruction_type_closure:
                        {
                            lstf_ir_closureinstruction *closure_inst = (lstf_ir_closureinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = closure @%s", num_instructions, closure_inst->fn->name);
                            for (iterator cap_it = ptr_list_iterator_create(closure_inst->captures);
                                    cap_it.has_next; cap_it = iterator_next(cap_it)) {
                                const lstf_ir_captured *cap = iterator_get_item(cap_it);
                                if (cap->is_local)
                                    string_appendf(bb_insns_buffer, ", %%%lu",
                                            (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, cap->local)->value);
                                else
                                    string_appendf(bb_insns_buffer, ", uv#%hhu", cap->upvalue_id);
                            }
                            string_appendf(bb_insns_buffer, "\\n");
                        }   break;

                        case lstf_ir_instruction_type_getelement:
                        {
                            lstf_ir_getelementinstruction *get_inst = (lstf_ir_getelementinstruction *)bb->instructions[i];
                            unsigned long container_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, get_inst->container)->value;
                            unsigned long index_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, get_inst->index)->value;

                            string_appendf(bb_insns_buffer, "%%%lu = get %%%lu, %%%lu\\n",
                                    num_instructions, container_i, index_i);
                        }   break;

                        case lstf_ir_instruction_type_getupvalue:
                        {
                            lstf_ir_getupvalueinstruction *guv_inst = (lstf_ir_getupvalueinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = getupvalue %hhu\\n",
                                    num_instructions, guv_inst->upvalue_id);
                        }   break;

                        case lstf_ir_instruction_type_indirectcall:
                        {
                            lstf_ir_indirectcallinstruction *icall_inst = (lstf_ir_indirectcallinstruction *)bb->instructions[i];
                            unsigned long expression_i =
                                (uintptr_t)ptr_hashmap_get(insn_result_ids, icall_inst->expression)->value;

                            if (icall_inst->has_return)
                                string_appendf(bb_insns_buffer, "%%%lu = ", num_instructions);
                            string_appendf(bb_insns_buffer, "calli %%%lu", expression_i);
                            for (iterator arg_it = ptr_list_iterator_create(icall_inst->arguments);
                                    arg_it.has_next; arg_it = iterator_next(arg_it)) {
                                string_appendf(bb_insns_buffer, ", %%%lu",
                                        (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, iterator_get_item(arg_it))->value);
                            }
                            string_appendf(bb_insns_buffer, "\\n");
                        }   break;

                        case lstf_ir_instruction_type_indirectschedule:
                        {
                            lstf_ir_indirectscheduleinstruction *isched_inst =
                                (lstf_ir_indirectscheduleinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = schedulei %%%lu",
                                    num_instructions,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, isched_inst->expression)->value);
                            for (iterator arg_it = ptr_list_iterator_create(isched_inst->arguments);
                                    arg_it.has_next; arg_it = iterator_next(arg_it)) {
                                string_appendf(bb_insns_buffer, ", %%%lu",
                                        (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, iterator_get_item(arg_it))->value);
                            }
                            string_appendf(bb_insns_buffer, "\\n");
                        }   break;

                        case lstf_ir_instruction_type_load:
                        {
                            lstf_ir_loadinstruction *load_inst = (lstf_ir_loadinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = load *%%%lu\\n",
                                    num_instructions,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, load_inst->source)->value);
                        }   break;

                        case lstf_ir_instruction_type_loadfunction:
                        {
                            lstf_ir_loadfunctioninstruction *loadfn_inst = (lstf_ir_loadfunctioninstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = load &@%s\\n",
                                    num_instructions,
                                    loadfn_inst->function->name);
                        }   break;

                        case lstf_ir_instruction_type_match:
                        {
                            lstf_ir_matchinstruction *match_inst = (lstf_ir_matchinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = match %%%lu, %%%lu\\n",
                                    num_instructions,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, match_inst->pattern)->value,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, match_inst->expression)->value);
                        }   break;

                        case lstf_ir_instruction_type_phi:
                        {
                            lstf_ir_phiinstruction *phi_inst = (lstf_ir_phiinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = phi", num_instructions);
                            for (iterator arg_it = ptr_list_iterator_create(phi_inst->arguments);
                                    arg_it.has_next; arg_it = iterator_next(arg_it)) {
                                lstf_ir_instruction *arg = iterator_get_item(arg_it);
                                if (!arg_it.is_first)
                                    string_appendf(bb_insns_buffer, ",");
                                string_appendf(bb_insns_buffer, " %%%lu",
                                        (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, arg)->value);
                            }
                        }   break;

                        case lstf_ir_instruction_type_return:
                        {
                            lstf_ir_returninstruction *ret_inst = (lstf_ir_returninstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "return");
                            if (ret_inst->value)
                                string_appendf(bb_insns_buffer, " %%%lu",
                                        (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, ret_inst->value)->value);
                            string_appendf(bb_insns_buffer, "\\n");
                        }   break;

                        case lstf_ir_instruction_type_schedule:
                        {
                            lstf_ir_scheduleinstruction *sched_inst = (lstf_ir_scheduleinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "schedule @%s", sched_inst->function->name);
                            for (iterator arg_it = ptr_list_iterator_create(sched_inst->arguments);
                                    arg_it.has_next; arg_it = iterator_next(arg_it)) {
                                lstf_ir_instruction *arg = iterator_get_item(arg_it);
                                string_appendf(bb_insns_buffer, ", %%%lu",
                                        (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, arg)->value);
                            }
                            string_appendf(bb_insns_buffer, "\\n");
                        }   break;

                        case lstf_ir_instruction_type_setelement:
                        {
                            lstf_ir_setelementinstruction *set_inst = (lstf_ir_setelementinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "set %%%lu, %%%lu, %%%lu\\n",
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, set_inst->container)->value,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, set_inst->index)->value,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, set_inst->value)->value);
                        }   break;

                        case lstf_ir_instruction_type_setupvalue:
                        {
                            lstf_ir_setupvalueinstruction *suv_inst = (lstf_ir_setupvalueinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "setupvalue %hhu, %%%lu\\n",
                                    suv_inst->upvalue_id,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, suv_inst->value)->value);
                        }   break;

                        case lstf_ir_instruction_type_store:
                        {
                            lstf_ir_storeinstruction *store_inst = (lstf_ir_storeinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "store %%%lu, *%%%lu\\n",
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, store_inst->source)->value,
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, store_inst->destination)->value);
                        }   break;

                        case lstf_ir_instruction_type_unary:
                        {
                            lstf_ir_unaryinstruction *unary_inst = (lstf_ir_unaryinstruction *)bb->instructions[i];

                            string_appendf(bb_insns_buffer, "%%%lu = %s %%%lu\\n",
                                    num_instructions,
                                    lstf_vm_opcode_to_string(unary_inst->opcode),
                                    (unsigned long)(uintptr_t)ptr_hashmap_get(insn_result_ids, unary_inst->source)->value);
                        }   break;
                    }

                    num_instructions++;
                }

                const char *bb_name = ptr_hashmap_get(bb_names, bb)->value;
                if (bb_insns_buffer->buffer[0])
                    fprintf(dotfile, "        \"%s_%s\" [label=\"%s\"];\n", fn->name, bb_name, bb_insns_buffer->buffer);
                else
                    fprintf(dotfile, "        \"%s_%s\";\n", fn->name, bb_name);

                for (unsigned i = 0; i < sizeof(bb->successors) / sizeof(bb->successors[0]); i++) {
                    if (!bb->successors[i])
                        continue;

                    const char *bb_successor_name = ptr_hashmap_get(bb_names, bb->successors[i])->value;
                    fprintf(dotfile, "        \"%s_%s\" -> \"%s_%s\";\n", fn->name, bb_name, fn->name, bb_successor_name);
                }

                fprintf(dotfile, "\n");
            }
        }
        fprintf(dotfile, "    }\n");
        ptr_hashmap_destroy(bb_names);
        ptr_hashmap_destroy(insn_result_ids);
    }
    string_unref(bb_insns_buffer);
    fprintf(dotfile, "}\n");

    fclose(dotfile);
    return true;
}
