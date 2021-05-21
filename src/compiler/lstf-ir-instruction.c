#include "lstf-ir-instruction.h"
#include "data-structures/ptr-list.h"
#include "lstf-ir-function.h"
#include "vm/lstf-vm-opcodes.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void
lstf_ir_instruction_destruct(lstf_ir_node *node)
{
    lstf_ir_instruction *inst = (lstf_ir_instruction *)node;

    lstf_codenode_unref(inst->code_node);

    switch (inst->insn_type) {
        case lstf_ir_instruction_type_constant:
            json_node_unref(((lstf_ir_constantinstruction *)inst)->json);
            break;
        case lstf_ir_instruction_type_call:
            ptr_list_destroy(((lstf_ir_callinstruction *)inst)->arguments);
            break;
        case lstf_ir_instruction_type_indirectschedule:
            ptr_list_destroy(((lstf_ir_indirectscheduleinstruction *)inst)->arguments);
            break;
        case lstf_ir_instruction_type_schedule:
            ptr_list_destroy(((lstf_ir_scheduleinstruction *)inst)->arguments);
            break;
        case lstf_ir_instruction_type_indirectcall:
            ptr_list_destroy(((lstf_ir_indirectcallinstruction *)inst)->arguments);
            break;
        case lstf_ir_instruction_type_closure:
            ptr_list_destroy(((lstf_ir_closureinstruction *)inst)->captures);
            break;
        case lstf_ir_instruction_type_phi:
            ptr_list_destroy(((lstf_ir_phiinstruction *)inst)->arguments);
            break;
        case lstf_ir_instruction_type_setelement:
        case lstf_ir_instruction_type_getelement:
        case lstf_ir_instruction_type_append:
        case lstf_ir_instruction_type_binary:
        case lstf_ir_instruction_type_branch:
        case lstf_ir_instruction_type_match:
        case lstf_ir_instruction_type_return:
        case lstf_ir_instruction_type_getupvalue:
        case lstf_ir_instruction_type_setupvalue:
        case lstf_ir_instruction_type_unary:
        case lstf_ir_instruction_type_alloc:
        case lstf_ir_instruction_type_load:
        case lstf_ir_instruction_type_loadfunction:
        case lstf_ir_instruction_type_store:
        case lstf_ir_instruction_type_assert:
            break;
    }
}

/**
 * Initializes a new IR instruction
 *
 * @param code_node         the associated code node, used for diagnostics
 * @param instruction       the instruction to initialize
 * @param insn_type         the type of instruction
 */
static void
lstf_ir_instruction_construct(lstf_codenode           *code_node,
                              lstf_ir_instruction     *instruction,
                              lstf_ir_instruction_type insn_type)
{
    lstf_ir_node_construct((lstf_ir_node *)instruction, lstf_ir_instruction_destruct, lstf_ir_node_type_instruction);
    instruction->code_node = lstf_codenode_ref(code_node);
    instruction->insn_type = insn_type;
    instruction->frame_offset = INT_MIN;
}

lstf_ir_instruction *lstf_ir_constantinstruction_new(lstf_codenode *code_node,
                                                     json_node     *json)
{
    lstf_ir_constantinstruction *insn = calloc(1, sizeof *insn);

    if (!insn) {
        perror("failed to allocate constant load instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)insn,
            lstf_ir_instruction_type_constant);

    insn->json = json_node_ref(json);

    return (lstf_ir_instruction *)insn;
}

lstf_ir_instruction *lstf_ir_getelementinstruction_new(lstf_codenode       *code_node,
                                                       lstf_ir_instruction *container,
                                                       lstf_ir_instruction *index)
{
    lstf_ir_getelementinstruction *gei = calloc(1, sizeof *gei);

    if (!gei) {
        perror("failed to create getelement IR instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)gei,
            lstf_ir_instruction_type_getelement);

    gei->container = container;
    gei->index = index;

    return (lstf_ir_instruction *)gei;
}

lstf_ir_instruction *lstf_ir_setelementinstruction_new(lstf_codenode       *code_node,
                                                       lstf_ir_instruction *container,
                                                       lstf_ir_instruction *index,
                                                       lstf_ir_instruction *value)
{
    assert(container && index && value);
    lstf_ir_setelementinstruction *sei = calloc(1, sizeof *sei);

    if (!sei) {
        perror("failed to create setelement IR instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)sei,
            lstf_ir_instruction_type_setelement);

    sei->container = container;
    sei->index = index;
    sei->value = value;

    return (lstf_ir_instruction *)sei;
}

lstf_ir_instruction *lstf_ir_binaryinstruction_new(lstf_codenode       *code_node,
                                                   lstf_vm_opcode       opcode,
                                                   lstf_ir_instruction *source_left,
                                                   lstf_ir_instruction *source_right)
{
    lstf_ir_binaryinstruction *bin_inst = calloc(1, sizeof *bin_inst);

    if (!bin_inst) {
        perror("failed to allocate binary IR instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)bin_inst,
            lstf_ir_instruction_type_binary);

    bin_inst->opcode = opcode;
    bin_inst->sources[0] = source_left;
    bin_inst->sources[1] = source_right;

    return (lstf_ir_instruction *)bin_inst;
}

lstf_ir_instruction *lstf_ir_unaryinstruction_new(lstf_codenode       *code_node,
                                                  lstf_vm_opcode       opcode,
                                                  lstf_ir_instruction *source)
{
    lstf_ir_unaryinstruction *unary_inst = calloc(1, sizeof *unary_inst);

    if (!unary_inst) {
        perror("failed to create IR unary instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)unary_inst,
            lstf_ir_instruction_type_unary);

    unary_inst->opcode = opcode;
    unary_inst->source = source;

    return (lstf_ir_instruction *)unary_inst;
}

lstf_ir_instruction *lstf_ir_callinstruction_new(lstf_codenode    *code_node,
                                                 lstf_ir_function *function,
                                                 ptr_list         *arguments)
{
    lstf_ir_callinstruction *call_inst = calloc(1, sizeof *call_inst);

    if (!call_inst) {
        perror("failed to allocate IR call instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)call_inst,
            lstf_ir_instruction_type_call);

    call_inst->function = function;
    call_inst->arguments = arguments;

    return (lstf_ir_instruction *)call_inst;
}

lstf_ir_instruction *lstf_ir_allocinstruction_new(lstf_codenode       *code_node,
                                                  lstf_ir_instruction *initializer)
{
    lstf_ir_allocinstruction *alloc_inst = calloc(1, sizeof *alloc_inst);

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)alloc_inst,
            lstf_ir_instruction_type_alloc);

    alloc_inst->initializer = initializer;
    return (lstf_ir_instruction *)alloc_inst;
}

lstf_ir_instruction *lstf_ir_loadinstruction_new(lstf_codenode       *code_node,
                                                 lstf_ir_instruction *source)
{
    lstf_ir_loadinstruction *load_inst = calloc(1, sizeof *load_inst);

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)load_inst, lstf_ir_instruction_type_load);

    load_inst->source = source;

    return (lstf_ir_instruction *)load_inst;
}

lstf_ir_instruction *lstf_ir_loadfunctioninstruction_new(lstf_codenode    *code_node,
                                                   lstf_ir_function *function)
{
    lstf_ir_loadfunctioninstruction *loadfn_inst = calloc(1, sizeof *loadfn_inst);

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)loadfn_inst,
            lstf_ir_instruction_type_loadfunction);

    loadfn_inst->function = function;

    return (lstf_ir_instruction *)loadfn_inst;
}

lstf_ir_instruction *lstf_ir_storeinstruction_new(lstf_codenode       *code_node,
                                                  lstf_ir_instruction *source,
                                                  lstf_ir_instruction *destination)
{
    lstf_ir_storeinstruction *store_inst = calloc(1, sizeof *store_inst);

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)store_inst,
            lstf_ir_instruction_type_store);

    store_inst->source = source;
    store_inst->destination = destination;

    return (lstf_ir_instruction *)store_inst;
}

lstf_ir_instruction *lstf_ir_scheduleinstruction_new(lstf_codenode    *code_node,
                                                     lstf_ir_function *function,
                                                     ptr_list         *arguments)
{
    lstf_ir_scheduleinstruction *schedule_inst = calloc(1, sizeof *schedule_inst);

    if (!schedule_inst) {
        perror("could not create IR schedule instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)schedule_inst,
            lstf_ir_instruction_type_schedule);

    schedule_inst->function = function;
    schedule_inst->arguments = arguments;

    return (lstf_ir_instruction *)schedule_inst;
}

lstf_ir_instruction *lstf_ir_branchinstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *source,
                                                   lstf_ir_basicblock  *taken,
                                                   lstf_ir_basicblock  *not_taken)
{
    assert(taken && ((!source && !not_taken) || (source && not_taken)));

    lstf_ir_branchinstruction *branch_inst = calloc(1, sizeof *branch_inst);

    if (!branch_inst) {
        perror("failed to create IR branch instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)branch_inst,
            lstf_ir_instruction_type_branch);

    branch_inst->source = source;
    branch_inst->taken = taken;
    branch_inst->not_taken = not_taken;

    return (lstf_ir_instruction *)branch_inst;
}

lstf_ir_instruction *lstf_ir_returninstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *value)
{
    lstf_ir_returninstruction *return_inst = calloc(1, sizeof *return_inst);

    if (!return_inst) {
        perror("failed to create IR return instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)return_inst,
            lstf_ir_instruction_type_return);

    return_inst->value = value;

    return (lstf_ir_instruction *)return_inst;
}

lstf_ir_instruction *lstf_ir_indirectcallinstruction_new(lstf_codenode       *code_node,
                                                         lstf_ir_instruction *expression,
                                                         ptr_list            *arguments,
                                                         bool                 has_return)
{
    lstf_ir_indirectcallinstruction *ic_inst = calloc(1, sizeof *ic_inst);

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)ic_inst,
            lstf_ir_instruction_type_indirectcall);

    ic_inst->expression = expression;
    ic_inst->arguments = arguments;
    ic_inst->has_return = has_return;

    return (lstf_ir_instruction *)ic_inst;
}

lstf_ir_instruction *lstf_ir_indirectscheduleinstruction_new(lstf_codenode       *code_node,
                                                             lstf_ir_instruction *expression,
                                                             ptr_list            *arguments,
                                                             bool                 has_return)
{
    lstf_ir_indirectscheduleinstruction *is_inst = calloc(1, sizeof *is_inst);

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)is_inst,
            lstf_ir_instruction_type_indirectschedule);

    is_inst->expression = expression;
    is_inst->arguments = arguments;
    is_inst->has_return = has_return;

    return (lstf_ir_instruction *)is_inst;
}

lstf_ir_instruction *lstf_ir_closureinstruction_new(lstf_codenode         *code_node,
                                                    lstf_ir_function      *fn,
                                                    ptr_list              *captures)
{
    lstf_ir_closureinstruction *closure_inst = calloc(1, sizeof *closure_inst);

    if (!closure_inst) {
        perror("could not create IR closure instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)closure_inst,
            lstf_ir_instruction_type_closure);

    closure_inst->fn = fn;
    closure_inst->captures = captures;

    return (lstf_ir_instruction *)closure_inst;
}

lstf_ir_instruction *lstf_ir_setupvalueinstruction_new(lstf_codenode       *code_node,
                                                       uint8_t              upvalue_id,
                                                       lstf_ir_instruction *value)
{
    lstf_ir_setupvalueinstruction *suv_inst = calloc(1, sizeof *suv_inst);

    if (!suv_inst) {
        perror("failed to create setupvalue IR instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)suv_inst,
            lstf_ir_instruction_type_setupvalue);

    suv_inst->upvalue_id = upvalue_id;
    suv_inst->value = value;

    return (lstf_ir_instruction *)suv_inst;
}

lstf_ir_instruction *lstf_ir_getupvalueinstruction_new(lstf_codenode *code_node, uint8_t upvalue_id)
{
    lstf_ir_getupvalueinstruction *guv_inst = calloc(1, sizeof *guv_inst);

    if (!guv_inst) {
        perror("failed to create IR getupvalue instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)guv_inst,
            lstf_ir_instruction_type_getupvalue);

    guv_inst->upvalue_id = upvalue_id;

    return (lstf_ir_instruction *)guv_inst;
}

lstf_ir_instruction *lstf_ir_phiinstruction_new(lstf_codenode *code_node, ptr_list *arguments)
{
    lstf_ir_phiinstruction *phi_inst = calloc(1, sizeof *phi_inst);

    if (!phi_inst) {
        perror("failed to create IR phi instruction");
        abort();
    }
    
    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)phi_inst,
            lstf_ir_instruction_type_phi);

    phi_inst->arguments = arguments;

    return (lstf_ir_instruction *)phi_inst;
}

lstf_ir_instruction *lstf_ir_appendinstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *container,
                                                   lstf_ir_instruction *value)
{
    lstf_ir_appendinstruction *append_inst = calloc(1, sizeof *append_inst);

    if (!append_inst) {
        perror("failed to create IR append instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)append_inst,
            lstf_ir_instruction_type_append);

    append_inst->container = container;
    append_inst->value = value;

    return (lstf_ir_instruction *)append_inst;
}

lstf_ir_instruction *lstf_ir_matchinstruction_new(lstf_codenode       *code_node,
                                                  lstf_ir_instruction *pattern,
                                                  lstf_ir_instruction *expression)
{
    lstf_ir_matchinstruction *match_inst = calloc(1, sizeof *match_inst);

    if (!match_inst) {
        perror("failed to create IR match instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)match_inst,
            lstf_ir_instruction_type_match);

    match_inst->pattern = pattern;
    match_inst->expression = expression;

    return (lstf_ir_instruction *)match_inst;
}

lstf_ir_instruction *lstf_ir_assertinstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *source)
{
    lstf_ir_assertinstruction *assert_inst = calloc(1, sizeof *assert_inst);

    if (!assert_inst) {
        perror("failed to create IR assert instruction");
        abort();
    }

    lstf_ir_instruction_construct(code_node,
            (lstf_ir_instruction *)assert_inst,
            lstf_ir_instruction_type_assert);

    assert_inst->source = source;

    return (lstf_ir_instruction *)assert_inst;
}

bool lstf_ir_instruction_has_result(const lstf_ir_instruction *insn)
{
    switch (insn->insn_type) {
        case lstf_ir_instruction_type_branch:
        case lstf_ir_instruction_type_return:
        case lstf_ir_instruction_type_append:
        case lstf_ir_instruction_type_setelement:
        case lstf_ir_instruction_type_setupvalue:
        case lstf_ir_instruction_type_store:
        case lstf_ir_instruction_type_assert:
            return false;
        case lstf_ir_instruction_type_alloc:
        case lstf_ir_instruction_type_binary:
        case lstf_ir_instruction_type_closure:
        case lstf_ir_instruction_type_constant:
        case lstf_ir_instruction_type_getelement:
        case lstf_ir_instruction_type_getupvalue:
        case lstf_ir_instruction_type_schedule:
        case lstf_ir_instruction_type_indirectschedule:
        case lstf_ir_instruction_type_load:
        case lstf_ir_instruction_type_loadfunction:
        case lstf_ir_instruction_type_match:
        case lstf_ir_instruction_type_phi:
        case lstf_ir_instruction_type_unary:
            return true;
        case lstf_ir_instruction_type_call:
            return ((const lstf_ir_callinstruction *)insn)->function->has_result;
        case lstf_ir_instruction_type_indirectcall:
            return ((const lstf_ir_indirectcallinstruction *)insn)->has_return;
    }

    fprintf(stderr, "%s: unreachable code (unexpected instruction type %u)\n",
            __func__, insn->insn_type);
    abort();
}
