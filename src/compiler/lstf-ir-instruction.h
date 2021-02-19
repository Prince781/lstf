#pragma once

#include "lstf-codenode.h"
#include "lstf-ir-node.h"
#include "data-structures/ptr-list.h"
#include "vm/lstf-vm-opcodes.h"
#include "json/json.h"
#include <stdbool.h>
#include <stdint.h>

enum _lstf_ir_instruction_type {
    lstf_ir_instruction_type_constant,
    lstf_ir_instruction_type_getelement,
    lstf_ir_instruction_type_setelement,
    lstf_ir_instruction_type_binary,
    lstf_ir_instruction_type_unary,
    lstf_ir_instruction_type_call,
    lstf_ir_instruction_type_schedule,
    lstf_ir_instruction_type_branch,
    lstf_ir_instruction_type_return,
    lstf_ir_instruction_type_indirectcall,
    lstf_ir_instruction_type_indirectschedule,
    lstf_ir_instruction_type_alloc,
    lstf_ir_instruction_type_load,
    lstf_ir_instruction_type_loadfunction,
    lstf_ir_instruction_type_store,
    lstf_ir_instruction_type_closure,
    lstf_ir_instruction_type_setupvalue,
    lstf_ir_instruction_type_getupvalue,
    lstf_ir_instruction_type_phi,           // pseudo-instruction
    lstf_ir_instruction_type_append,
    lstf_ir_instruction_type_match,
};
typedef enum _lstf_ir_instruction_type lstf_ir_instruction_type;

/**
 * A pointer to an IR instruction can be implicitly used as a unique identifier
 * for a temporary, since (almost) every IR instruction produces a result, the
 * only exceptions being calls that do not return a value.
 */
struct _lstf_ir_instruction {
    lstf_ir_node parent_struct;
    lstf_ir_instruction_type insn_type;
    int frame_offset;
    lstf_codenode *code_node;
};
typedef struct _lstf_ir_instruction lstf_ir_instruction;

struct _lstf_ir_constantinstruction {
    lstf_ir_instruction parent_struct;

    /**
     * The source of this instruction must be a JSON expression.
     */
    json_node *json;
};
typedef struct _lstf_ir_constantinstruction lstf_ir_constantinstruction;

lstf_ir_instruction *lstf_ir_constantinstruction_new(lstf_codenode *code_node, json_node *json);

struct _lstf_ir_getelementinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_instruction *container;
    lstf_ir_instruction *index;
};
typedef struct _lstf_ir_getelementinstruction lstf_ir_getelementinstruction;

lstf_ir_instruction *lstf_ir_getelementinstruction_new(lstf_codenode       *code_node,
                                                       lstf_ir_instruction *container,
                                                       lstf_ir_instruction *index);

struct _lstf_ir_setelementinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_instruction *container;
    lstf_ir_instruction *index;

    /**
     * The value to assign to `<container>.<index>` or `<container>[<index>]`
     */
    lstf_ir_instruction *value;
};
typedef struct _lstf_ir_setelementinstruction lstf_ir_setelementinstruction;

lstf_ir_instruction *lstf_ir_setelementinstruction_new(lstf_codenode       *code_node,
                                                       lstf_ir_instruction *container,
                                                       lstf_ir_instruction *index,
                                                       lstf_ir_instruction *value);

struct _lstf_ir_binaryinstruction {
    lstf_ir_instruction parent_struct;
    lstf_vm_opcode opcode;

    /**
     * The sources of this instruction must be the results of two previous
     * instructions.
     */
    lstf_ir_instruction *sources[2];
};
typedef struct _lstf_ir_binaryinstruction lstf_ir_binaryinstruction;

lstf_ir_instruction *lstf_ir_binaryinstruction_new(lstf_codenode       *code_node,
                                                   lstf_vm_opcode       opcode,
                                                   lstf_ir_instruction *source_left,
                                                   lstf_ir_instruction *source_right);

struct _lstf_ir_unaryinstruction {
    lstf_ir_instruction parent_struct;
    lstf_vm_opcode opcode;
    lstf_ir_instruction *source;
};
typedef struct _lstf_ir_unaryinstruction lstf_ir_unaryinstruction;

lstf_ir_instruction *lstf_ir_unaryinstruction_new(lstf_codenode       *code_node,
                                                  lstf_vm_opcode       opcode,
                                                  lstf_ir_instruction *source);

typedef struct _lstf_ir_function lstf_ir_function;
struct _lstf_ir_callinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_function *function;

    /**
     * Each element of this list is a `(lstf_ir_instruction *)`
     */
    ptr_list *arguments;
};
typedef struct _lstf_ir_callinstruction lstf_ir_callinstruction;

lstf_ir_instruction *lstf_ir_callinstruction_new(lstf_codenode    *code_node,
                                                 lstf_ir_function *function,
                                                 ptr_list         *arguments);

typedef struct _lstf_ir_basicblock lstf_ir_basicblock;
struct _lstf_ir_branchinstruction {
    lstf_ir_instruction parent_struct;

    /**
     * (weak) The condition to evaluate
     *
     * - If `NULL`, then this is an unconditional jump, and `not_taken` must be
     *   `NULL` too.
     */
    lstf_ir_instruction *source;

    /**
     * (weak) The basic block to jump to if the condition is true
     */
    lstf_ir_basicblock *taken;

    /**
     * (weak) The basic block to jump to if the condition is false,
     * or `NULL` if this is an unconditional jump.
     */
    lstf_ir_basicblock *not_taken;
};
typedef struct _lstf_ir_branchinstruction lstf_ir_branchinstruction;

lstf_ir_instruction *lstf_ir_branchinstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *source,
                                                   lstf_ir_basicblock  *taken,
                                                   lstf_ir_basicblock  *not_taken);

struct _lstf_ir_returninstruction {
    lstf_ir_instruction parent_struct;

    /**
     * The value to return, or `NULL` if there is no value to return.
     */
    lstf_ir_instruction *value;
};
typedef struct _lstf_ir_returninstruction lstf_ir_returninstruction;

/**
 * Creates a return instruction.
 *
 * @param value         the value to return, or `NULL` if there is no value to return
 */
lstf_ir_instruction *lstf_ir_returninstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *value);

struct _lstf_ir_indirectcallinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_instruction *expression;
    ptr_list *arguments;
    bool has_return;
};
typedef struct _lstf_ir_indirectcallinstruction lstf_ir_indirectcallinstruction;

/**
 * Creates a new indirect call instruction.
 *
 * @param expression    can be any kind of instruction
 * @param arguments     must be a list of `(lstf_ir_instruction *)`
 * @param has_return    whether the type of function pointer has a return value
 */
lstf_ir_instruction *lstf_ir_indirectcallinstruction_new(lstf_codenode       *code_node,
                                                         lstf_ir_instruction *expression,
                                                         ptr_list            *arguments,
                                                         bool                 has_return);

struct _lstf_ir_indirectscheduleinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_instruction *expression;
    ptr_list *arguments;
    bool has_return;
};
typedef struct _lstf_ir_indirectscheduleinstruction lstf_ir_indirectscheduleinstruction;

/**
 * Creates a new indirect schedule instruction.
 *
 * @param expression    can be any kind of instruction
 * @param arguments     arguments to pass to the function. must be a list of
 *                      `(lstf_ir_instruction *)`
 * @param has_return    whether the type of function pointer has a return value
 */
lstf_ir_instruction *lstf_ir_indirectscheduleinstruction_new(lstf_codenode       *code_node,
                                                             lstf_ir_instruction *expression,
                                                             ptr_list            *arguments,
                                                             bool                 has_return);

struct _lstf_ir_allocinstruction {
    lstf_ir_instruction parent_struct;

    /**
     * Whether this storage is automatically deallocated by the VM on function
     * return. This is `true` for function parameters.
     *
     * Also determines whether this allocation is already (in the IR) initialized to a result.
     * If `false`, we expect at least one virtual IR `store` instruction to
     * "write" to this stack area before the stack pointer moves.
     */
    bool is_automatic;

    bool is_initialized;
};
typedef struct _lstf_ir_allocinstruction lstf_ir_allocinstruction;

lstf_ir_instruction *lstf_ir_allocinstruction_new(lstf_codenode *code_node,
                                                  bool           is_automatic);


struct _lstf_ir_loadinstruction {
    lstf_ir_instruction parent_struct;

    /**
     * The position this instruction value occupies on the stack is the offset
     * this load is from.
     */
    lstf_ir_instruction *source;
};
typedef struct _lstf_ir_loadinstruction lstf_ir_loadinstruction;

/**
 * Creates an instruction loading a value from the stack.
 *
 * @param source        the instruction that produces a result on the stack that
 *                      will be loaded
 */
lstf_ir_instruction *lstf_ir_loadinstruction_new(lstf_codenode       *code_node,
                                                 lstf_ir_instruction *source);

struct _lstf_ir_loadfunctioninstruction {
    lstf_ir_instruction parent_struct;

    lstf_ir_function *function;
};
typedef struct _lstf_ir_loadfunctioninstruction lstf_ir_loadfunctioninstruction;

/**
 * Creates an instruction loading the address to a function.
 *
 * @param function      the function address to load
 */
lstf_ir_instruction *lstf_ir_loadfunctioninstruction_new(lstf_codenode    *code_node,
                                                         lstf_ir_function *function);

struct _lstf_ir_storeinstruction {
    lstf_ir_instruction parent_struct;

    /**
     * The instruction that produces a result on the stack.
     */
    lstf_ir_instruction *source;

    /**
     * The destination on the stack to write to.
     */
    lstf_ir_instruction *destination;
};
typedef struct _lstf_ir_storeinstruction lstf_ir_storeinstruction;

/**
 * Creates an instruction storing to the location of the value on the stack.
 *
 * @param source        the instruction that produces a result on the stack
 * @param destination   the destination on the stack to write to
 */
lstf_ir_instruction *lstf_ir_storeinstruction_new(lstf_codenode       *code_node,
                                                  lstf_ir_instruction *source,
                                                  lstf_ir_instruction *destination);

struct _lstf_ir_scheduleinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_function *function;
    ptr_list *arguments;
};
typedef struct _lstf_ir_scheduleinstruction lstf_ir_scheduleinstruction;

/**
 * Creates a new schedule instruction.
 *
 * @param function      the function to invoke as a coroutine
 * @param arguments     arguments to pass to the function. must be a list of
 *                      `(lstf_ir_instruction *)`
 */
lstf_ir_instruction *lstf_ir_scheduleinstruction_new(lstf_codenode    *code_node,
                                                     lstf_ir_function *function,
                                                     ptr_list         *arguments);

/**
 * Represents a captured value
 */
typedef struct {
    bool is_local;
    union {
        lstf_ir_instruction *local;
        uint8_t upvalue_id;
    };
} lstf_ir_captured;

struct _lstf_ir_closureinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_function *fn;

    /**
     * A list of `(lstf_ir_captured *)`
     */
    ptr_list *captures;
};
typedef struct _lstf_ir_closureinstruction lstf_ir_closureinstruction;

/**
 * Creates a new instruction for a closure.
 *
 * @param fn        a function to call
 * @param captures  a list of `(lstf_ir_captured *)`, i.e. the captured locals/up-values
 */
lstf_ir_instruction *lstf_ir_closureinstruction_new(lstf_codenode         *code_node,
                                                    lstf_ir_function      *fn,
                                                    ptr_list              *captures);

struct _lstf_ir_setupvalueinstruction {
    lstf_ir_instruction parent_struct;
    uint8_t upvalue_id;
    lstf_ir_instruction *value;
};
typedef struct _lstf_ir_setupvalueinstruction lstf_ir_setupvalueinstruction;

lstf_ir_instruction *lstf_ir_setupvalueinstruction_new(lstf_codenode       *code_node,
                                                       uint8_t              upvalue_id,
                                                       lstf_ir_instruction *value);

struct _lstf_ir_getupvalueinstruction {
    lstf_ir_instruction parent_struct;
    uint8_t upvalue_id;
};
typedef struct _lstf_ir_getupvalueinstruction lstf_ir_getupvalueinstruction;

lstf_ir_instruction *lstf_ir_getupvalueinstruction_new(lstf_codenode *code_node, uint8_t upvalue_id);

struct _lstf_ir_phiinstruction {
    lstf_ir_instruction parent_struct;

    /**
     * list of `(lstf_ir_instruction *)`
     */
    ptr_list *arguments;
};
typedef struct _lstf_ir_phiinstruction lstf_ir_phiinstruction;

/**
 * Creating a phi instruction takes ownership of the `arguments` list.
 */
lstf_ir_instruction *lstf_ir_phiinstruction_new(lstf_codenode *code_node, ptr_list *arguments);

struct _lstf_ir_appendinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_instruction *container;
    lstf_ir_instruction *value;
};
typedef struct _lstf_ir_appendinstruction lstf_ir_appendinstruction;

lstf_ir_instruction *lstf_ir_appendinstruction_new(lstf_codenode       *code_node,
                                                   lstf_ir_instruction *container,
                                                   lstf_ir_instruction *value);

struct _lstf_ir_matchinstruction {
    lstf_ir_instruction parent_struct;
    lstf_ir_instruction *pattern;
    lstf_ir_instruction *expression;
};
typedef struct _lstf_ir_matchinstruction lstf_ir_matchinstruction;

lstf_ir_instruction *lstf_ir_matchinstruction_new(lstf_codenode       *code_node,
                                                  lstf_ir_instruction *pattern,
                                                  lstf_ir_instruction *expression);

/**
 * Whether this is an instruction that produces a result.
 */
bool lstf_ir_instruction_has_result(const lstf_ir_instruction *insn);
