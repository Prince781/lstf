#pragma once

#include "lstf-common.h"
#include "lstf-statement.h"
#include "lstf-variable.h"
#include "lstf-sourceref.h"
#include "lstf-file.h"
#include "data-structures/ptr-list.h"
#include "data-structures/ptr-hashset.h"
#include "lstf-datatype.h"
#include "lstf-block.h"
#include "lstf-symbol.h"
#include "lstf-scope.h"
#include "vm/lstf-vm-opcodes.h"
#include <stdbool.h>
#include <stdint.h>

struct _lstf_function {
    lstf_symbol parent_struct;
    lstf_datatype *return_type;

    /**
     * list of `(lstf_variable *)` items
     */
    ptr_list *parameters;

    /**
     * Contains the parameters of this function.
     */
    lstf_scope *scope;

    /**
     * If NULL, then this is a native/builtin function.
     */
    lstf_block *block;

    /**
     * The VM instruction that this function corresponds to, if relevant (when
     * `lstf_symbol::is_builtin` is true).
     */
    lstf_vm_opcode vm_opcode;

    /**
     * The special VM call that this function correponds to, if relevant.
     * 
     * When relevant, then `vm_opcode` will be `lstf_vm_op_vmcall`
     */
    lstf_vm_vmcallcode vm_callcode;

    /**
     * The local variables declared outside of this function that are captured
     * by this function (AKA the up-values we need to generate).
     *
     * hash set of `(lstf_symbol *)` which may be `(lstf_variable *)` or
     * `(lstf_function *)`
     */
    ptr_hashset *captured_locals;

    /**
     * Whether this is an instance method. If it is, then this function must be
     * a child of a `lstf_typesymbol`
     */
    bool is_instance;

    /**
     * If true, then calls to this function can use the `await` keyword.
     */
    bool is_async;

    /**
     * Whether this function contains a return statement.
     */
    bool has_return_statement;
};
typedef struct _lstf_function lstf_function;

static inline lstf_function *lstf_function_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_symbol &&
            ((lstf_symbol *)code_node)->symbol_type == lstf_symbol_type_function)
        return (lstf_function *)code_node;
    return NULL;
}

/**
 * Creates a new function for user-defined code.
 */
lstf_symbol *lstf_function_new(const lstf_sourceref *source_reference, 
                               const char           *name,
                               lstf_datatype        *return_type,
                               bool                  is_instance,
                               bool                  is_async,
                               bool                  is_builtin)
    __attribute__((nonnull (2, 3)));

/**
 * Creates a new function (prototype) for a VM instruction.
 *
 * This means that `lstf_symbol::is_builtin` will be true.
 *
 * @param vm_callcode either the VM call code or `(lstf_vm_vmcallcode)0` if
 *                    this is not a VM call.
 */
lstf_symbol *lstf_function_new_for_opcode(const lstf_sourceref *source_reference,
                                          const char           *name,
                                          lstf_datatype        *return_type,
                                          bool                  is_async,
                                          lstf_vm_opcode        vm_opcode,
                                          lstf_vm_vmcallcode    vm_callcode)
    __attribute__((nonnull (2, 3)));

void lstf_function_add_parameter(lstf_function *function, lstf_variable *variable);

lstf_variable *lstf_function_get_parameter(lstf_function *function, const char *parameter_name);

void lstf_function_set_return_type(lstf_function *function, lstf_datatype *data_type);

void lstf_function_add_statement(lstf_function *function, lstf_statement *statement);

/**
 * Capture either a local variable or a closure.
 *
 * `let v = ...;`
 *
 * In the below example, `f` captures `v`, and `g` references `v`
 * and `f`, therefore `g` has two captures: `v` and `f`. If `f` did not capture 
 * `v`, `g` would only have one capture `v`, because then `f` wouldn't be
 * associated with a closure created as a hidden local variable.
 *
 * ```
 * fun main(): void {
 *      let v = 1;
 *
 *      fun f(): void {
 *          v += 1;
 *      }
 *
 *      fun g(): void {
 *          v += 1;
 *          f();
 *      }
 * }
 * ```
 */
void lstf_function_add_captured_local(lstf_function *function, lstf_symbol *var_or_fn);
