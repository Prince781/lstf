#pragma once

#include "vm/lstf-vm-opcodes.h"
#include "json/json.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
         * Used for data loads
         */
        uint64_t data_offset;

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

static inline lstf_bc_instruction lstf_bc_instruction_load_dataoffset_new(uint64_t data_offset)
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


static inline lstf_bc_instruction lstf_bc_instruction_if_new(lstf_bc_instruction *instruction_ref)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_if,
        .instruction_ref = instruction_ref
    };
}

static inline lstf_bc_instruction lstf_bc_instruction_jump_new(lstf_bc_instruction *instruction_ref)
{
    return (lstf_bc_instruction) {
        .opcode = lstf_vm_op_jump,
        .instruction_ref = instruction_ref
    };
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
    case lstf_vm_op_print:
    case lstf_vm_op_exit:
        break;
    }
}
