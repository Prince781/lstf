#pragma once

#include "vm/lstf-vm-opcodes.h"
#include "json/json.h"
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _lstf_bc_function;
typedef struct _lstf_bc_function lstf_bc_function;

typedef struct _lstf_bc_instruction lstf_bc_instruction;
struct _lstf_bc_instruction {
    /**
     * This is serialized to one byte
     */
    lstf_vm_opcode opcode;

    union {
        /**
         * Used for stores and certain loads.
         */
        int64_t frame_offset;

        /**
         * Used for data loads. Do not free.
         */
        char *data_offset;

        /**
         * Used by:
         * - `lstf_vm_op_load_codeoffset`
         * - `lstf_vm_op_call`
         *
         * Used when loading a function reference or calling a function
         */
        lstf_bc_function *function_ref;

        /**
         * Used by: `lstf_vm_op_vmcall`
         */
        lstf_vm_vmcallcode vmcall_code;

        /**
         * Used by: `lstf_vm_op_load_expression`
         */
        char *json_expression;

        /**
         * Used by:
         * - `lstf_vm_op_if`
         * - `lstf_vm_op_jump`
         *
         * For jumping to the location of an instruction.
         */
        lstf_bc_instruction *instruction_ref;

        /**
         * Used by: `lstf_vm_op_exit`
         *
         * The exit code
         */
        uint8_t exit_code;
    };
};

static inline lstf_bc_instruction lstf_bc_instruction_load_frameoffset_new(int64_t frame_offset)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_load_frameoffset,
        .frame_offset = frame_offset
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_load_dataoffset_new(char *data_offset)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_load_dataoffset,
        .data_offset = data_offset
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_load_codeoffset_new(lstf_bc_function *function_ref)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_load_codeoffset,
        .function_ref = function_ref
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_load_expression_new(json_node *expression)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_load_expression,
        .json_expression = json_node_to_string(expression, false)
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_store_new(int64_t frame_offset)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_store,
        .frame_offset = frame_offset
    };
}


static inline lstf_bc_instruction lstf_bc_instruction_get_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_get,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_set_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_set,
        { 0 }
    };
}


static inline lstf_bc_instruction lstf_bc_instruction_call_new(lstf_bc_function *function_ref)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_call,
        .function_ref = function_ref
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_indirect_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_indirect,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_return_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_return,
        { 0 }
    };
}


static inline lstf_bc_instruction lstf_bc_instruction_vmcall_new(lstf_vm_vmcallcode vmcall_code)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_vmcall,
        .vmcall_code = vmcall_code
    };
}


/**
 * Creates a new conditional jump instruction.
 *
 * @param instruction_ref pass `NULL` if the jump is unresolved
 */
static inline lstf_bc_instruction lstf_bc_instruction_if_new(lstf_bc_instruction *instruction_ref)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_if,
        .instruction_ref = instruction_ref
    };
}

/**
 * Creates a new unconditional jump instruction.
 *
 * @param instruction_ref pass `NULL` if the jump is unresolved
 */
static inline lstf_bc_instruction lstf_bc_instruction_jump_new(lstf_bc_instruction *instruction_ref)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_jump,
        .instruction_ref = instruction_ref
    };
}

/**
 * Resolves a conditional or unconditional jump instruction to the address of `instruction_ref`.
 */
static inline lstf_bc_instruction *lstf_bc_instruction_resolve_jump(lstf_bc_instruction *instruction, 
                                                                    lstf_bc_instruction *instruction_ref)
{
    assert((instruction->opcode == lstf_vm_op_if || instruction->opcode == lstf_vm_op_jump) &&
            !instruction->instruction_ref);
    instruction->instruction_ref = instruction_ref;
    return instruction;
}

static inline lstf_bc_instruction lstf_bc_instruction_bool_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_bool,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_land_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_land,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_lor_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_lor,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_lnot_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_lnot,
        { 0 }
    };
}


static inline lstf_bc_instruction lstf_bc_instruction_lessthan_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_lessthan,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_lessthan_equal_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_lessthan_equal,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_equal_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_equal,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_greaterthan_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_greaterthan,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_greaterthan_equal_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_greaterthan_equal,
        { 0 }
    };
}


static inline lstf_bc_instruction lstf_bc_instruction_add_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_add,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_sub_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_sub,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_mul_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_mul,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_div_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_div,
        { 0 }
    };
}


static inline lstf_bc_instruction lstf_bc_instruction_print_new(void)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_print,
        { 0 }
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_exit_new(uint8_t exit_code)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_exit,
        .exit_code = exit_code
    };
}

static inline size_t lstf_bc_instruction_compute_size(lstf_bc_instruction *instruction)
{
    switch (instruction->opcode) {
    case lstf_vm_op_load_frameoffset:
        return sizeof(uint8_t) + sizeof(instruction->frame_offset);
    case lstf_vm_op_load_dataoffset:
        return sizeof(uint8_t) + sizeof(instruction->data_offset);
    case lstf_vm_op_load_codeoffset:
        return sizeof(uint8_t) + sizeof(uint64_t);
    case lstf_vm_op_load_expression:
        return sizeof(uint8_t) + strlen(instruction->json_expression) + 1;
    case lstf_vm_op_store:
        return sizeof(uint8_t) + sizeof(instruction->frame_offset);
    case lstf_vm_op_get:
        return sizeof(uint8_t);
    case lstf_vm_op_set:
        return sizeof(uint8_t);
    case lstf_vm_op_call:
        return sizeof(uint8_t) + sizeof(uint64_t);
    case lstf_vm_op_indirect:
        return sizeof(uint8_t);
    case lstf_vm_op_return:
        return sizeof(uint8_t);
    case lstf_vm_op_vmcall:
        return sizeof(uint8_t) + sizeof(uint8_t);
    case lstf_vm_op_if:
        return sizeof(uint8_t) + sizeof(uint64_t);
    case lstf_vm_op_jump:
        return sizeof(uint8_t) + sizeof(uint64_t);
    case lstf_vm_op_bool:
        return sizeof(uint8_t);
    case lstf_vm_op_land:
        return sizeof(uint8_t);
    case lstf_vm_op_lor:
        return sizeof(uint8_t);
    case lstf_vm_op_lnot:
        return sizeof(uint8_t);
    case lstf_vm_op_lessthan:
        return sizeof(uint8_t);
    case lstf_vm_op_lessthan_equal:
        return sizeof(uint8_t);
    case lstf_vm_op_equal:
        return sizeof(uint8_t);
    case lstf_vm_op_greaterthan:
        return sizeof(uint8_t);
    case lstf_vm_op_greaterthan_equal:
        return sizeof(uint8_t);
    case lstf_vm_op_add:
        return sizeof(uint8_t);
    case lstf_vm_op_sub:
        return sizeof(uint8_t);
    case lstf_vm_op_mul:
        return sizeof(uint8_t);
    case lstf_vm_op_div:
        return sizeof(uint8_t);
    case lstf_vm_op_print:
        return sizeof(uint8_t);
    case lstf_vm_op_exit:
        return sizeof(uint8_t) + sizeof(uint8_t);
    }

    fprintf(stderr, "%s: unreachable code: unexpected VM opcode `%d'\n", __func__, instruction->opcode);
    abort();
}

static inline void lstf_bc_instruction_clear(lstf_bc_instruction *instruction)
{
    switch (instruction->opcode) {
    case lstf_vm_op_load_frameoffset:
    case lstf_vm_op_load_dataoffset:
    case lstf_vm_op_load_codeoffset:
        break;
    case lstf_vm_op_load_expression:
        free(instruction->json_expression);
        break;
    case lstf_vm_op_store:
    case lstf_vm_op_get:
    case lstf_vm_op_set:
    case lstf_vm_op_call:
    case lstf_vm_op_indirect:
    case lstf_vm_op_return:
    case lstf_vm_op_vmcall:
    case lstf_vm_op_if:
    case lstf_vm_op_jump:
    case lstf_vm_op_bool:
    case lstf_vm_op_land:
    case lstf_vm_op_lor:
    case lstf_vm_op_lnot:
    case lstf_vm_op_lessthan:
    case lstf_vm_op_lessthan_equal:
    case lstf_vm_op_equal:
    case lstf_vm_op_greaterthan:
    case lstf_vm_op_greaterthan_equal:
    case lstf_vm_op_add:
    case lstf_vm_op_sub:
    case lstf_vm_op_mul:
    case lstf_vm_op_div:
    case lstf_vm_op_print:
    case lstf_vm_op_exit:
        break;
    }
}
