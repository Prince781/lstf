#include "lstf-codegenerator.h"
#include "lstf-assertstatement.h"
#include "lstf-block.h"
#include "lstf-typesymbol.h"
#include "lstf-variable.h"
#include "lstf-returnstatement.h"
#include "lstf-unaryexpression.h"
#include "lstf-constant.h"
#include "lstf-function.h"
#include "lstf-lambdaexpression.h"
#include "lstf-expression.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include "lstf-functiontype.h"
#include "lstf-report.h"
#include "lstf-codevisitor.h"
#include "io/outputstream.h"
#include "data-structures/ptr-hashset.h"
#include "data-structures/string-builder.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "bytecode/lstf-bc-program.h"
#include "bytecode/lstf-bc-serialize.h"
#include "lstf-ir-function.h"
#include "lstf-ir-basicblock.h"
#include "lstf-ir-instruction.h"
#include "lstf-ir-program.h"
#include "util.h"
#include "vm/lstf-vm-opcodes.h"
#include "json/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static lstf_scope *
lstf_codenode_get_containing_scope(lstf_codenode *node)
{
    for (node = node->parent_node; node; node = node->parent_node) {
        if (node->codenode_type == lstf_codenode_type_scope)
            return (lstf_scope *)node;

        lstf_block *owner_parent_block = lstf_block_cast(node);
        if (owner_parent_block)
            return owner_parent_block->scope;

        lstf_function *owner_parent_function = lstf_function_cast(node);
        if (owner_parent_function)
            return owner_parent_function->scope;

        lstf_typesymbol *owner_parent_typesymbol = lstf_typesymbol_cast(node);
        if (owner_parent_typesymbol)
            return owner_parent_typesymbol->scope;

        lstf_lambdaexpression *owner_parent_lambda = lstf_lambdaexpression_cast(node);
        if (owner_parent_lambda)
            return owner_parent_lambda->scope;
    }

    return NULL;
}

/**
 * Gets the containing function or lambda
 *
 * Cast to `lstf_function *` or `lstf_lambdaexpression *`
 */
static lstf_codenode *
lstf_codenode_get_containing_function(lstf_codenode *node)
{
    for (node = node->parent_node; node; node = node->parent_node)
        if (lstf_function_cast(node) || lstf_lambdaexpression_cast(node))
            return node;

    return NULL;
}

static lstf_ir_function *
lstf_codegenerator_get_current_function(lstf_codegenerator *generator)
{
    assert(!ptr_list_is_empty(generator->ir_functions) && "we should always have a current IR function");
    return ptr_list_node_get_data(generator->ir_functions->tail, lstf_ir_function *);
}

/**
 * The result of this is independent of the current basic block we're in.
 */
static lstf_ir_instruction *
lstf_codegenerator_get_temp_for_expression(lstf_codegenerator *generator, lstf_expression *expression)
{
    assert(expression);
    const ptr_hashmap_entry *entry = ptr_hashmap_get(generator->exprs_to_temps, expression);

    return entry->value;
}

static void
lstf_codegenerator_set_temp_for_expression(lstf_codegenerator  *generator,
                                           lstf_expression     *expression,
                                           lstf_ir_instruction *temp)
{
    assert(expression && temp);
    ptr_hashmap_insert(generator->exprs_to_temps, expression, temp);
}

static lstf_ir_basicblock *
lstf_codegenerator_get_current_basicblock_for_scope(lstf_codegenerator *generator,
                                                    lstf_scope         *scope)
{
    const ptr_hashmap_entry *entry = ptr_hashmap_get(generator->scopes_to_basicblocks, scope);

    if (entry)
        return entry->value;

    return NULL;
}

static void
lstf_codegenerator_set_current_basicblock_for_scope(lstf_codegenerator *generator,
                                                    lstf_scope         *scope,
                                                    lstf_ir_basicblock *block)
{
    ptr_hashmap_insert(generator->scopes_to_basicblocks, scope, block);
}

/**
 * The result of this function will depend on the scope we are currently in.
 */
static lstf_ir_instruction *
lstf_codegenerator_get_temp_for_symbol(lstf_codegenerator *generator,
                                       lstf_scope         *current_scope,
                                       lstf_symbol        *symbol)
{
    lstf_ir_instruction *temp = NULL;

    while (!temp && current_scope) {
        const ptr_hashmap_entry *scope_map_entry =
            ptr_hashmap_get(generator->scopes_to_symbols_to_temps, current_scope);

        if (scope_map_entry) {
            const ptr_hashmap *symbols_to_temps = scope_map_entry->value;
            const ptr_hashmap_entry *temp_entry = ptr_hashmap_get(symbols_to_temps, symbol);

            if (temp_entry)
                temp = temp_entry->value;
        }

        if (!temp) {
            // if we haven't found anything, try looking in the parent scope
            lstf_codenode *owner = lstf_codenode_cast(current_scope)->parent_node;
            lstf_codenode *parent = owner->parent_node;
            current_scope = NULL;

            while (parent && !current_scope) {
                lstf_block *parent_block = lstf_block_cast(parent);
                if (parent_block) {
                    current_scope = parent_block->scope;
                    break;
                }

                lstf_function *parent_function = lstf_function_cast(parent);
                if (parent_function) {
                    current_scope = parent_function->scope;
                    break;
                }

                lstf_typesymbol *parent_typesymbol = lstf_typesymbol_cast(parent);
                if (parent_typesymbol) {
                    current_scope = parent_typesymbol->scope;
                    break;
                }

                lstf_lambdaexpression *parent_lambda = lstf_lambdaexpression_cast(parent);
                if (parent_lambda) {
                    current_scope = parent_lambda->scope;
                    break;
                }

                parent = parent->parent_node;
            }
        }
    }

    return temp;
}

/**
 * Associates @symbol with @temp in @current_scope
 */
static void
lstf_codegenerator_set_temp_for_symbol(lstf_codegenerator  *generator,
                                       lstf_scope          *current_scope,
                                       lstf_symbol         *symbol,
                                       lstf_ir_instruction *temp)
{
    const ptr_hashmap_entry *scope_map_entry =
        ptr_hashmap_get(generator->scopes_to_symbols_to_temps, current_scope);
    ptr_hashmap *symbols_to_temps = NULL;

    if (!scope_map_entry) {
        // create a new scope map
        symbols_to_temps = ptr_hashmap_new(ptrhash,
                (collection_item_ref_func) lstf_codenode_ref,
                (collection_item_unref_func) lstf_codenode_unref,
                NULL,
                (collection_item_ref_func) lstf_ir_node_ref,
                (collection_item_unref_func) lstf_ir_node_unref);
        ptr_hashmap_insert(generator->scopes_to_symbols_to_temps, current_scope, symbols_to_temps);
    } else {
        symbols_to_temps = scope_map_entry->value;
    }

    ptr_hashmap_insert(symbols_to_temps, symbol, temp);
}

/**
 * Gets the IR associated with a function.
 *
 * @param function  could be a `lstf_function *` or `lstf_lambdaexpression *`
 */
static lstf_ir_function *
lstf_codegenerator_get_ir_function_from_codenode(lstf_codegenerator *generator, lstf_codenode *function)
{
    const ptr_hashmap_entry *entry = ptr_hashmap_get(generator->codenodes_to_ir_functions, function);

    if (!entry)
        return NULL;

    return entry->value;
}

static void
lstf_codegenerator_set_ir_function_from_codenode(lstf_codegenerator *generator, lstf_codenode *function, lstf_ir_function *fn)
{
    ptr_hashmap_insert(generator->codenodes_to_ir_functions, function, fn);
}

static lstf_ir_instruction *
lstf_codegenerator_get_alloc_for_local(lstf_codegenerator *generator,
                                       lstf_symbol        *local)
{
    const ptr_hashmap_entry *entry = ptr_hashmap_get(generator->local_allocs, local);

    if (!entry)
        return NULL;

    return entry->value;
}

static void
lstf_codegenerator_set_alloc_for_local(lstf_codegenerator  *generator,
                                       lstf_symbol         *local,
                                       lstf_ir_instruction *alloc_inst,
                                       lstf_ir_function    *fn)
{
    assert(alloc_inst->insn_type == lstf_ir_instruction_type_alloc);
    if (!((lstf_ir_allocinstruction *)alloc_inst)->is_automatic)
        fn->num_locals++;
    ptr_hashmap_insert(generator->local_allocs, local, alloc_inst);
}

/**
 * Gets the up-value ID of the symbol referred to by `access` or return -1 if
 * it is not captured.
 */
static int
lstf_memberaccess_get_capture_id(lstf_memberaccess *access)
{
    lstf_symbol *symbol = lstf_expression_cast(access)->symbol_reference;
    ptr_hashset *captured_locals = NULL;
    assert(symbol && "access must have a symbol reference");

    for (lstf_codenode *node = lstf_codenode_cast(access)->parent_node;
            node;
            node = node->parent_node) {
        lstf_function *parent_function = lstf_function_cast(node);
        if (parent_function && ptr_hashset_contains(parent_function->captured_locals, symbol)) {
            captured_locals = parent_function->captured_locals;
            break;
        }

        lstf_lambdaexpression *parent_lambda = lstf_lambdaexpression_cast(node);
        if (parent_lambda && ptr_hashset_contains(parent_lambda->captured_locals, symbol)) {
            captured_locals = parent_lambda->captured_locals;
            break;
        }
    }

    if (captured_locals) {
        for (iterator it = ptr_hashset_iterator_create(captured_locals); it.has_next; it = iterator_next(it)) {
            if (iterator_get_item(it) == symbol)
                return it.counter;
        }
    }

    return -1;
}

/**
 * Generates a closure instruction for the function (either an explicit
 * function or a lambda)
 * and the environment it closes over. If the function does not capture
 * anything, this generates an address load instruction instead.
 */
static void
lstf_codegenerator_generate_closure_for_function(lstf_codegenerator *generator,
                                                 lstf_codenode      *code_node,
                                                 lstf_ir_function   *ir_function)
{
    lstf_lambdaexpression *lambda = lstf_lambdaexpression_cast(code_node);
    lstf_function *function = lstf_function_cast(code_node);
    assert(lambda || function);

    if (function == generator->file->main_function)
        // the main function is at the root of the scope hierarchy
        return;

    ptr_hashset *captured_locals = lambda ? lambda->captured_locals : function->captured_locals;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(code_node);
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    // if this function does not close over any variables, just generate a load
    // instruction and quit in the current function
    if (ptr_hashset_is_empty(captured_locals)) {
        if (lambda) {
            lstf_ir_instruction *load_inst = lstf_ir_loadfunctioninstruction_new(code_node, ir_function);
            lstf_ir_basicblock_add_instruction(block, load_inst);
            lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(lambda), load_inst);
        }
        return;
    }

    // - get containing function/lambda of this lambda
    // - for each symbol captured by this lambda, check whether that
    //   function/lambda also captures this symbol
    // - if it does, then this is an up-value. otherwise, this must be a local 
    //   variable/parameter declared in the containing function/lambda

    lstf_codenode *container = code_node->parent_node;
    while (container && !(lstf_function_cast(container) || lstf_lambdaexpression_cast(container)))
        container = lstf_codenode_cast(container)->parent_node;

    assert(container && "must have a containing function or lambda!");
    lstf_function *containing_function = lstf_function_cast(container);
    lstf_lambdaexpression *containing_lambda = lstf_lambdaexpression_cast(container);
    ptr_list *captures = ptr_list_new(NULL, free);

    for (iterator it = ptr_hashset_iterator_create(captured_locals); it.has_next; it = iterator_next(it)) {
        lstf_variable *variable = iterator_get_item(it);
        lstf_ir_captured *captured = calloc(1, sizeof *captured);

        // whether we're capturing a local variable from the current function
        // (AKA the parent of the function/lambda we're at)
        captured->is_local = !ptr_hashset_contains(containing_function ?
            containing_function->captured_locals :
            containing_lambda->captured_locals,
            variable);
        if (captured->is_local) {
            captured->local = lstf_codegenerator_get_alloc_for_local(generator, lstf_symbol_cast(variable));
            assert(captured->local && "capture of local variable must correspond to an instruction!");
        } else {
            captured->upvalue_id = it.counter;
        }

        ptr_list_append(captures, captured);
    }

    // generate a closure instruction in the current function that references
    // the function we just created
    lstf_ir_instruction *closure_inst = lstf_ir_closureinstruction_new(code_node, ir_function, captures);
    
    if (lambda) {
        lstf_ir_basicblock_add_instruction(block, closure_inst);
        lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(lambda), closure_inst);
    } else if (function) { 
        lstf_ir_instruction *alloc_inst = lstf_ir_allocinstruction_new(code_node, false);
        lstf_ir_instruction *store_inst = lstf_ir_storeinstruction_new(code_node, closure_inst, alloc_inst);
        // the containing function
        lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);

        lstf_ir_basicblock_add_instruction(block, alloc_inst);
        lstf_ir_basicblock_add_instruction(block, closure_inst);
        lstf_ir_basicblock_add_instruction(block, store_inst);

        lstf_codegenerator_set_temp_for_symbol(generator, current_scope, lstf_symbol_cast(function), alloc_inst);
        lstf_codegenerator_set_alloc_for_local(generator, lstf_symbol_cast(function), alloc_inst, fn);
    } else {
        lstf_ir_basicblock_add_instruction(block, closure_inst);
    }
}

static void
lstf_codegenerator_visit_array(lstf_codevisitor *visitor, lstf_array *array)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(array));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    json_node *array_json = NULL;
    lstf_ir_instruction *array_temp = NULL;

    if ((array_json = lstf_expression_to_json(lstf_expression_cast(array)))) {
        array_temp = lstf_ir_constantinstruction_new(lstf_codenode_cast(array), array_json);
        lstf_ir_basicblock_add_instruction(block, array_temp);
    } else {
        array_temp =
            lstf_ir_constantinstruction_new(lstf_codenode_cast(array),
                    array->is_pattern ? json_array_pattern_new() : json_array_new());
        lstf_ir_basicblock_add_instruction(block, array_temp);

        for (iterator it = ptr_list_iterator_create(array->expression_list); it.has_next; it = iterator_next(it)) {
            lstf_expression *element = iterator_get_item(it);
            lstf_ir_instruction *element_temp = NULL;
            json_node *element_json = NULL;

            if ((element_json = lstf_expression_to_json(element))) {
                element_temp = lstf_ir_constantinstruction_new(lstf_codenode_cast(element), element_json);
                lstf_ir_basicblock_add_instruction(block, element_temp);
            } else {
                lstf_codenode_accept(element, visitor);
                element_temp = lstf_codegenerator_get_temp_for_expression(generator, element);
            }

            lstf_ir_basicblock_add_instruction(block,
                    lstf_ir_appendinstruction_new(lstf_codenode_cast(element), array_temp, element_temp));
        }
    }

    lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(array), array_temp);
}

static void
lstf_codegenerator_visit_assert_statement(lstf_codevisitor *visitor, lstf_assertstatement *stmt)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(stmt));

    lstf_codenode_accept_children(stmt, visitor);

    // t_expression = <... expression ...>
    // assert t_expression

    lstf_ir_instruction *t_expression = lstf_codegenerator_get_temp_for_expression(generator, stmt->expression);
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    lstf_ir_basicblock_add_instruction(block, lstf_ir_assertinstruction_new(lstf_codenode_cast(stmt), t_expression));
}

static void
lstf_codegenerator_visit_assignment(lstf_codevisitor *visitor, lstf_assignment *assign)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(assign));

    if (assign->lhs->expr_type == lstf_expression_type_memberaccess) {
        if (assign->lhs->symbol_reference->is_builtin) {
            // TODO: handle assignment to built-in variable
            lstf_report_warning(&lstf_codenode_cast(assign)->source_reference, "codegen: TODO");
            return;
        }

        // we are writing to an object or a variable
        lstf_memberaccess *maccess = (lstf_memberaccess *)assign->lhs;

        if (!maccess->inner) {
            // we are writing to a variable

            lstf_codenode_accept_children(assign, visitor);
            lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
            lstf_ir_instruction *rhs_temp = lstf_codegenerator_get_temp_for_expression(generator, assign->rhs);

            // associate our new temporary (rhs_temp) with the symbol on the LHS
            lstf_codegenerator_set_temp_for_symbol(generator,
                    current_scope,
                    assign->lhs->symbol_reference,
                    rhs_temp);
            int capture_id;
            if ((capture_id = lstf_memberaccess_get_capture_id(maccess)) != -1) {
                // generate a store to the up-value
                lstf_ir_basicblock_add_instruction(block,
                        lstf_ir_setupvalueinstruction_new(lstf_codenode_cast(assign), capture_id, rhs_temp));
            } else {
                // generate a store instruction
                lstf_ir_basicblock_add_instruction(block,
                        lstf_ir_storeinstruction_new(lstf_codenode_cast(assign),
                            rhs_temp,
                            lstf_codegenerator_get_alloc_for_local(generator, assign->lhs->symbol_reference)));
            }
        } else {
            // we are writing to an object property

            // [lhs_inner].[member_name] = [rhs]
            // should serialize to:
            // [lhs_inner]
            // [member_name]
            // [rhs]
            lstf_codenode_accept(assign->lhs, visitor);

            lstf_ir_instruction *lhs_temp = lstf_codegenerator_get_temp_for_expression(generator, maccess->inner);

            lstf_ir_instruction *index_inst =
                lstf_ir_constantinstruction_new(lstf_codenode_cast(maccess), json_string_new(maccess->member_name));
            lstf_ir_basicblock_add_instruction(
                    lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope), index_inst);

            lstf_codenode_accept(assign->rhs, visitor);         // current basic block may change
            lstf_ir_instruction *rhs_temp = lstf_codegenerator_get_temp_for_expression(generator, assign->rhs);

            lstf_ir_instruction *set_inst =
                lstf_ir_setelementinstruction_new(lstf_codenode_cast(maccess), lhs_temp, index_inst, rhs_temp);

            lstf_ir_basicblock_add_instruction(
                    lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope), set_inst);
        }
    } else if (assign->lhs->expr_type == lstf_expression_type_elementaccess) {
        // we are writing to an object or array
        lstf_elementaccess *eaccess = (lstf_elementaccess *)assign->lhs;

        lstf_ir_basicblock *block =
            lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

        // if the number of arguments is 1, then create:
        // 1. instructions to load the expression [access->container]
        // 2. instructions to load the expression [access->arguments(0)]

        // if the number of arguments is 2, then this is an array slice we are assigning to, and so
        // we will need to generate an assignment for each element of the slice.
        // again, we generate 

        if (eaccess->arguments->length == 1) {
            lstf_codenode_accept_children(assign, visitor);
            lstf_ir_instruction *lhs_temp = lstf_codegenerator_get_temp_for_expression(generator, eaccess->container);
            lstf_ir_instruction *rhs_temp = lstf_codegenerator_get_temp_for_expression(generator, assign->rhs);
            lstf_ir_instruction *index_inst =
                lstf_codegenerator_get_temp_for_expression(generator,
                        iterator_get_item(ptr_list_iterator_create(eaccess->arguments)));
            lstf_ir_instruction *set_inst =
                lstf_ir_setelementinstruction_new(lstf_codenode_cast(eaccess), lhs_temp, index_inst, rhs_temp);

            lstf_ir_basicblock_add_instruction(block, set_inst);
        } else if (eaccess->arguments->length == 2) {
            // generate an assignment to a sliced array index
            // the RHS is evaluated only once:
            // ----------------------------------------------
            //
            //          t_container = [eaccess->container]
            //          t_index0 = [eaccess->arguments(0)]
            //          t_end = [eaccess->arguments(1)]
            //          t_test0 = t_index0 < t_end
            //          if (t_test0) goto init else goto end
            //
            // init:
            //          t_rhs = [assign->rhs]
            //
            // test:
            //          t_index1 = phi(t_index0, t_index2)
            //          t_test1 = t_index1 < t_end
            //          if (t_test1) goto loop else goto end
            //
            // loop:
            //          set(t_container, t_index1, t_rhs)
            //          t_index2 = t_index1 + 1
            //          goto test
            // 
            // end:
            //          <...>

            lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);

            lstf_codenode_accept(eaccess, visitor);

            lstf_ir_instruction *t_container =
                lstf_codegenerator_get_temp_for_expression(generator, eaccess->container);
            iterator args_it = ptr_list_iterator_create(eaccess->arguments);
            lstf_ir_instruction *t_index0 =
                lstf_codegenerator_get_temp_for_expression(generator, iterator_get_item(args_it));
            args_it = iterator_next(args_it);
            lstf_ir_instruction *t_end =
                lstf_codegenerator_get_temp_for_expression(generator, iterator_get_item(args_it));
            lstf_ir_instruction *t_test0 =
                lstf_ir_binaryinstruction_new(lstf_codenode_cast(assign), lstf_vm_op_lessthan, t_index0, t_end);

            lstf_ir_basicblock *bb_init = lstf_ir_basicblock_new();
            lstf_ir_basicblock *bb_test = lstf_ir_basicblock_new();
            lstf_ir_basicblock *bb_loop = lstf_ir_basicblock_new();
            lstf_ir_basicblock *bb_end  = lstf_ir_basicblock_new();

            lstf_ir_basicblock_add_instruction(block, t_test0);
            lstf_ir_basicblock_add_instruction(block,
                    lstf_ir_branchinstruction_new(lstf_codenode_cast(assign), t_test0, bb_init, bb_end));

            lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_init);
            lstf_codenode_accept(assign->rhs, visitor);
            lstf_ir_instruction *t_rhs = lstf_codegenerator_get_temp_for_expression(generator, assign->rhs);

            lstf_ir_instruction *t_index1 =
                lstf_ir_phiinstruction_new(
                    lstf_codenode_cast(assign),
                    ptr_list_new_with_data((collection_item_ref_func) lstf_ir_node_ref,
                        (collection_item_unref_func) lstf_ir_node_unref,
                        t_index0,
                        NULL));
            lstf_ir_instruction *t_test1 =
                lstf_ir_binaryinstruction_new(lstf_codenode_cast(assign), lstf_vm_op_lessthan, t_index1, t_end);
            lstf_ir_basicblock_add_instruction(bb_test, t_index1);
            lstf_ir_basicblock_add_instruction(bb_test, t_test1);
            lstf_ir_basicblock_add_instruction(bb_test,
                    lstf_ir_branchinstruction_new(NULL, t_test1, bb_loop, bb_end));

            lstf_ir_basicblock_add_instruction(bb_loop,
                    lstf_ir_setelementinstruction_new(lstf_codenode_cast(assign), t_container, t_index1, t_rhs));
            lstf_ir_instruction *t_increment =
                lstf_ir_constantinstruction_new(lstf_codenode_cast(assign), json_integer_new(1));
            lstf_ir_basicblock_add_instruction(bb_loop, t_increment);
            lstf_ir_instruction *t_index2 =
                lstf_ir_binaryinstruction_new(
                        lstf_codenode_cast(assign),
                        lstf_vm_op_add,
                        t_index1,
                        t_increment);
            ptr_list_append(((lstf_ir_phiinstruction *)t_index1)->arguments, t_index2);
            lstf_ir_basicblock_add_instruction(bb_loop, t_index2);
            lstf_ir_basicblock_add_instruction(bb_loop,
                    lstf_ir_branchinstruction_new(lstf_codenode_cast(assign), NULL, bb_test, NULL));

            lstf_ir_function_add_basic_block(fn, bb_init);
            lstf_ir_function_add_basic_block(fn, bb_test);
            lstf_ir_function_add_basic_block(fn, bb_loop);
            lstf_ir_function_add_basic_block(fn, bb_end);

            lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_end);
        }
    } else {
        fprintf(stderr, "%s: unexpected LHS expression type %s\n",
                __func__, lstf_expression_type_to_string(assign->lhs->expr_type));
        abort();
    }
}

static void
lstf_codegenerator_visit_binary_expression(lstf_codevisitor *visitor, lstf_binaryexpression *expr)
{
    lstf_codenode_accept(expr->left, visitor);

    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;

    lstf_ir_instruction *lhs_temp = lstf_codegenerator_get_temp_for_expression(generator, expr->left);

    lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(expr));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    if (expr->op == lstf_binaryoperator_coalescer) {
        // handle special case for coalescing operator:
        // t0 = LHS (result of expr->left)
        // t1 = bool t0
        // if (t1) goto L2 else goto L1
        // L1:      t2 = RHS (result of expr->right)
        // L2:      t3 = phi(t0, t2)
        lstf_ir_basicblock *bb_L1 = lstf_ir_basicblock_new();
        lstf_ir_basicblock *bb_L2 = lstf_ir_basicblock_new();

        lstf_ir_instruction *t0 = lhs_temp;
        lstf_ir_instruction *t1 = lstf_ir_unaryinstruction_new(lstf_codenode_cast(expr->left), lstf_vm_op_bool, t0);
        lstf_ir_basicblock_add_instruction(block, t1);
        lstf_ir_basicblock_add_instruction(block,
                lstf_ir_branchinstruction_new(lstf_codenode_cast(expr), t1, bb_L2, bb_L1));

        lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_L1);
        // this will write the IR instructions of [expr->right] to L1
        lstf_codenode_accept(expr->right, visitor);
        lstf_ir_instruction *t2 = lstf_codegenerator_get_temp_for_expression(generator, expr->right);
        bb_L1->successors[0] = bb_L2;

        lstf_ir_instruction *t3 = 
            lstf_ir_phiinstruction_new(
                lstf_codenode_cast(expr),
                ptr_list_new_with_data((collection_item_ref_func) lstf_ir_node_ref,
                    (collection_item_unref_func) lstf_ir_node_unref,
                    t0,
                    t2,
                    NULL));
        lstf_ir_basicblock_add_instruction(bb_L2, t3);

        lstf_ir_function_add_basic_block(fn, bb_L1);
        lstf_ir_function_add_basic_block(fn, bb_L2);

        lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(expr), t3);
        lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_L2);
        return;
    } else if (expr->op == lstf_binaryoperator_notequal) {
        // handle special case for not-equals operator
        lstf_codenode_accept(expr->right, visitor);
        lstf_ir_instruction *rhs_temp = lstf_codegenerator_get_temp_for_expression(generator, expr->right);
        lstf_ir_instruction *eq_test =
            lstf_ir_binaryinstruction_new(lstf_codenode_cast(expr), lstf_vm_op_equal, lhs_temp, rhs_temp);
        lstf_ir_instruction *notequal_temp =
            lstf_ir_unaryinstruction_new(lstf_codenode_cast(expr), lstf_vm_op_lnot, eq_test);

        lstf_ir_basicblock_add_instruction(block, eq_test);
        lstf_ir_basicblock_add_instruction(block, notequal_temp);

        lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(expr), notequal_temp);
        return;
    }
    
    lstf_codenode_accept(expr->right, visitor);
    lstf_ir_instruction *rhs_temp = lstf_codegenerator_get_temp_for_expression(generator, expr->right);

    lstf_vm_opcode opcode;

    switch (expr->op) {
        case lstf_binaryoperator_bitwise_and:
            opcode = lstf_vm_op_and;
            break;
        case lstf_binaryoperator_bitwise_or:
            opcode = lstf_vm_op_or;
            break;
        case lstf_binaryoperator_bitwise_xor:
            opcode = lstf_vm_op_xor;
            break;
        case lstf_binaryoperator_coalescer:
            fprintf(stderr, "%s: unreachable code: special case handling needed for coalescing operator\n",
                    __func__);
            abort();
        case lstf_binaryoperator_divide:
            opcode = lstf_vm_op_div;
            break;
        case lstf_binaryoperator_equal:
            opcode = lstf_vm_op_equal;
            break;
        case lstf_binaryoperator_exponent:
            opcode = lstf_vm_op_pow;
            break;
        case lstf_binaryoperator_greaterthan:
            opcode = lstf_vm_op_greaterthan;
            break;
        case lstf_binaryoperator_greaterthan_equal:
            opcode = lstf_vm_op_greaterthan_equal;
            break;
        case lstf_binaryoperator_in:
            opcode = lstf_vm_op_in;
            break;
        case lstf_binaryoperator_leftshift:
            opcode = lstf_vm_op_lshift;
            break;
        case lstf_binaryoperator_lessthan:
            opcode = lstf_vm_op_lessthan;
            break;
        case lstf_binaryoperator_lessthan_equal:
            opcode = lstf_vm_op_lessthan_equal;
            break;
        case lstf_binaryoperator_logical_and:
            opcode = lstf_vm_op_land;
            break;
        case lstf_binaryoperator_logical_or:
            opcode = lstf_vm_op_lor;
            break;
        case lstf_binaryoperator_minus:
            opcode = lstf_vm_op_sub;
            break;
        case lstf_binaryoperator_modulo:
            opcode = lstf_vm_op_mod;
            break;
        case lstf_binaryoperator_multiply:
            opcode = lstf_vm_op_mul;
            break;
        case lstf_binaryoperator_notequal:
            fprintf(stderr, "%s: unreachable code: special case handling needed for not-equals operator\n",
                    __func__);
            abort();
        case lstf_binaryoperator_plus:
            opcode = lstf_vm_op_add;
            break;
        case lstf_binaryoperator_rightshift:
            opcode = lstf_vm_op_rshift;
            break;
        case lstf_binaryoperator_equivalent:
            opcode = lstf_vm_op_equal;
            break;
        default:
            fprintf(stderr, "%s: unreachable code: invalid expression op `%u'\n",
                    __func__, expr->op);
            abort();
    }

    lstf_ir_instruction *expr_temp =
        lstf_ir_binaryinstruction_new(lstf_codenode_cast(expr), opcode, lhs_temp, rhs_temp);
    lstf_ir_basicblock_add_instruction(block, expr_temp);

    lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(expr), expr_temp);
}

static void
lstf_codegenerator_visit_block(lstf_codevisitor *visitor, lstf_block *block)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(block));
    lstf_ir_basicblock *bb_current =
        lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    if (!lstf_codegenerator_get_current_basicblock_for_scope(generator, block->scope))
        lstf_codegenerator_set_current_basicblock_for_scope(generator, block->scope, bb_current);

    lstf_codenode_accept_children(block, visitor);
}

static void
lstf_codegenerator_visit_conditional_expression(lstf_codevisitor *visitor, lstf_conditionalexpression *expr)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(expr));

    // t0 = [expr->condition]
    // if (t0) goto L1 else goto L2
    // L1:  t1 = [expr->true_expression]
    //      goto L3
    // L2:  t2 = [expr->false_expression]
    // L3:  t3 = phi(t1, t2)

    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    lstf_codenode_accept(expr->condition, visitor);

    lstf_ir_basicblock *bb_L1 = lstf_ir_basicblock_new();
    lstf_ir_basicblock *bb_L2 = lstf_ir_basicblock_new();
    lstf_ir_basicblock *bb_L3 = lstf_ir_basicblock_new();

    lstf_ir_instruction *t0 = lstf_codegenerator_get_temp_for_expression(generator, expr->condition);
    lstf_ir_basicblock_add_instruction(block,
            lstf_ir_branchinstruction_new(lstf_codenode_cast(expr), t0, bb_L1, bb_L2));

    lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_L1);
    lstf_codenode_accept(expr->true_expression, visitor);
    lstf_ir_instruction *t1 = lstf_codegenerator_get_temp_for_expression(generator, expr->true_expression);
    lstf_ir_basicblock_add_instruction(bb_L1,
            lstf_ir_branchinstruction_new(lstf_codenode_cast(expr), NULL, bb_L3, NULL));

    lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_L2);
    lstf_codenode_accept(expr->false_expression, visitor);
    lstf_ir_instruction *t2 = lstf_codegenerator_get_temp_for_expression(generator, expr->false_expression);
    bb_L2->successors[0] = bb_L3;

    lstf_ir_basicblock_add_instruction(bb_L3,
            lstf_ir_phiinstruction_new(
                lstf_codenode_cast(expr),
                ptr_list_new_with_data((collection_item_ref_func) lstf_ir_node_ref,
                    (collection_item_unref_func) lstf_ir_node_unref,
                    t1,
                    t2,
                    NULL)));

    lstf_ir_function_add_basic_block(fn, bb_L1);
    lstf_ir_function_add_basic_block(fn, bb_L2);
    lstf_ir_function_add_basic_block(fn, bb_L3);

    lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_L3);
}

static void
lstf_codegenerator_visit_constant(lstf_codevisitor *visitor, lstf_constant *constant)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(constant));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    json_node *constant_json = lstf_expression_to_json(constant->expression);

    assert(constant_json && "lstf_constant should be convertible to a JSON expression");

    lstf_ir_instruction *const_inst =
        lstf_ir_constantinstruction_new(lstf_codenode_cast(constant), constant_json);
    lstf_ir_basicblock_add_instruction(block, const_inst);

    lstf_codegenerator_set_temp_for_symbol(generator, current_scope, lstf_symbol_cast(constant), const_inst);
}

static void
lstf_codegenerator_visit_declaration(lstf_codevisitor *visitor, lstf_declaration *decl)
{
    lstf_codenode_accept_children(decl, visitor);
}

static void
lstf_codegenerator_visit_element_access(lstf_codevisitor *visitor, lstf_elementaccess *access)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_expression *expr = lstf_expression_cast(access);
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(access));

    lstf_codenode_accept_children(access, visitor);

    if (!lstf_expression_is_lvalue(expr)) {
        // if the number of arguments is 1, then simply generate a `get` instruction
        // if the number of arguments is 2, then generate a new array from the slice
        lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);
        lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

        if (access->arguments->length == 1) {
            lstf_ir_instruction *container_inst = lstf_codegenerator_get_temp_for_expression(generator, access->container);
            lstf_ir_instruction *index_inst =
                lstf_codegenerator_get_temp_for_expression(generator,
                        iterator_get_item(ptr_list_iterator_create(access->arguments)));

            lstf_ir_instruction *get_inst =
                lstf_ir_getelementinstruction_new(lstf_codenode_cast(access), container_inst, index_inst);
            lstf_ir_basicblock_add_instruction(block, get_inst);

            lstf_codegenerator_set_temp_for_expression(generator, expr, get_inst);
        } else if (access->arguments->length == 2) {
            // generate array slice operation:
            // ------------------------------------
            //
            //          t_container = [access->container]
            //          t_index0 = [access->arguments(0)]
            //          t_end = [access->arguments(1)]
            //          t_slice = new []
            //
            // test:
            //          t_index1 = phi(t_index0, t_index2)
            //          t_test0 = t_index1 < t_end
            //          if (t_test0) goto fill else goto end
            //
            // fill:
            //          t_element = get(t_container, t_index1)
            //          append(t_slice, t_element)
            //          t_index2 = t_index1 + 1
            //          goto test
            //
            // end:
            //          <...>
 
            lstf_ir_instruction *t_container = lstf_codegenerator_get_temp_for_expression(generator, access->container);
            iterator args_it = ptr_list_iterator_create(access->arguments);
            lstf_ir_instruction *t_index0 = lstf_codegenerator_get_temp_for_expression(generator, iterator_get_item(args_it));
            args_it = iterator_next(args_it);
            lstf_ir_instruction *t_end = lstf_codegenerator_get_temp_for_expression(generator, iterator_get_item(args_it));
            lstf_ir_instruction *t_slice =
                lstf_ir_constantinstruction_new(lstf_codenode_cast(access), json_array_new());

            lstf_ir_basicblock_add_instruction(block, t_slice);

            lstf_ir_basicblock *bb_test = lstf_ir_basicblock_new();
            lstf_ir_basicblock *bb_fill = lstf_ir_basicblock_new();
            lstf_ir_basicblock *bb_end  = lstf_ir_basicblock_new();

            block->successors[0] = bb_test;

            lstf_ir_instruction *t_index1 =
                lstf_ir_phiinstruction_new(
                        lstf_codenode_cast(access),
                        ptr_list_new_with_data((collection_item_ref_func) lstf_ir_node_ref,
                            (collection_item_unref_func) lstf_ir_node_unref,
                            t_index0,
                            NULL));
            lstf_ir_instruction *t_test0 =
                lstf_ir_binaryinstruction_new(lstf_codenode_cast(access), lstf_vm_op_lessthan, t_index1, t_end);
            lstf_ir_basicblock_add_instruction(bb_test,
                    lstf_ir_branchinstruction_new(lstf_codenode_cast(access), t_test0, bb_fill, bb_end));

            lstf_ir_instruction *t_element =
                lstf_ir_getelementinstruction_new(lstf_codenode_cast(access), t_container, t_index1);
            lstf_ir_basicblock_add_instruction(bb_fill, t_element);
            lstf_ir_basicblock_add_instruction(bb_fill,
                    lstf_ir_appendinstruction_new(lstf_codenode_cast(access), t_slice, t_element));
            lstf_ir_instruction *t_index2 =
                lstf_ir_binaryinstruction_new(
                        lstf_codenode_cast(access),
                        lstf_vm_op_add,
                        t_index1,
                        lstf_ir_constantinstruction_new(lstf_codenode_cast(access), json_integer_new(1)));
            ptr_list_append(((lstf_ir_phiinstruction *)t_index1)->arguments, t_index2);
            lstf_ir_basicblock_add_instruction(bb_fill, t_index2);
            lstf_ir_basicblock_add_instruction(bb_fill,
                    lstf_ir_branchinstruction_new(lstf_codenode_cast(access), NULL, bb_test, NULL));

            lstf_ir_function_add_basic_block(fn, bb_test);
            lstf_ir_function_add_basic_block(fn, bb_fill);
            lstf_ir_function_add_basic_block(fn, bb_end);

            lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_end);
            lstf_codegenerator_set_temp_for_expression(generator, expr, t_slice);
        }
    }
}

static void
lstf_codegenerator_visit_expression_statement(lstf_codevisitor *visitor, lstf_expressionstatement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static void
lstf_codegenerator_visit_file(lstf_codevisitor *visitor, lstf_file *file)
{
    lstf_codenode_accept(file->main_function, visitor);
}

static void
lstf_codegenerator_visit_function(lstf_codevisitor *visitor, lstf_function *function)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_ir_function *fn = NULL;

    if (lstf_symbol_cast(function)->is_builtin) {
        fn = lstf_ir_function_new_for_instruction(lstf_symbol_cast(function)->name,
                function->parameters->length,
                function->return_type->datatype_type != lstf_datatype_type_voidtype,
                !(function->vm_opcode == lstf_vm_op_exit),
                function->vm_opcode,
                function->vm_callcode);
    } else {
        fn = lstf_ir_function_new_for_userfn(lstf_symbol_cast(function)->name,
                function->parameters->length,
                ptr_hashset_num_elements(function->captured_locals),
                function->return_type->datatype_type != lstf_datatype_type_voidtype);
    }

    lstf_codegenerator_set_ir_function_from_codenode(generator, lstf_codenode_cast(function), fn);
    lstf_ir_program_add_function(generator->ir, fn);
    ptr_list_append(generator->ir_functions, fn);
    if (function->block) {
        // associate a new basic block with the function block in the AST
        lstf_ir_basicblock *bb_start = lstf_ir_basicblock_new();
        fn->entry_block->successors[0] = bb_start;

        lstf_codegenerator_set_current_basicblock_for_scope(generator, function->scope, bb_start);
        lstf_ir_function_add_basic_block(fn, bb_start);
    }

    lstf_codenode_accept_children(function, visitor);
    if (!function->has_return_statement && function->block) {
        // every user-defined function needs to end with a return instruction
        lstf_ir_basicblock *bb_end =
            lstf_codegenerator_get_current_basicblock_for_scope(generator, function->block->scope);
        lstf_ir_basicblock_add_instruction(bb_end,
                lstf_ir_returninstruction_new(lstf_codenode_cast(function->block), NULL));
        // connect to the exit block
        bb_end->successors[0] = fn->exit_block;
    }
    ptr_list_remove_last_link(generator->ir_functions);

    // generate code for a closure if the function has up-values
    lstf_codegenerator_generate_closure_for_function(generator, lstf_codenode_cast(function), fn);
}

static void
lstf_codegenerator_visit_if_statement(lstf_codevisitor *visitor, lstf_ifstatement *stmt)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(stmt));
    
    // generate IR for a branch:
    // 
    // [ top block    ]
    // |              |
    // [ (condition)  ] -----------> [  if-statements  ]
    //  |                                     |
    //  |                                     |
    //  V                                     |
    // [ else block(?)]                       |
    //  |                                     |
    //  |                                     |
    //  V                                     |
    // [ next block   ] <--------------------- 

    lstf_codenode_accept(stmt->condition, visitor);
    lstf_ir_instruction *t_ifcondition = lstf_codegenerator_get_temp_for_expression(generator, stmt->condition);

    lstf_ir_basicblock *top_block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    lstf_ir_basicblock *bb_ifbegin = lstf_ir_basicblock_new();
    lstf_ir_basicblock *bb_elsebegin = stmt->false_statements ? lstf_ir_basicblock_new() : NULL;
    lstf_ir_basicblock *bb_next = lstf_ir_basicblock_new();

    // this will connect top_block to the next basic blocks
    lstf_ir_basicblock_add_instruction(top_block,
            lstf_ir_branchinstruction_new(lstf_codenode_cast(stmt),
                t_ifcondition, bb_ifbegin, bb_elsebegin ? bb_elsebegin : bb_next));

    lstf_ir_function_add_basic_block(fn, bb_ifbegin);
    lstf_codegenerator_set_current_basicblock_for_scope(generator, stmt->true_statements->scope, bb_ifbegin);
    lstf_codenode_accept(stmt->true_statements, visitor);

    lstf_ir_basicblock *bb_ifend =
        lstf_codegenerator_get_current_basicblock_for_scope(generator, stmt->true_statements->scope);
    bb_ifend->successors[0] = bb_next;

    if (stmt->false_statements) {
        lstf_ir_function_add_basic_block(fn, bb_elsebegin);
        lstf_codegenerator_set_current_basicblock_for_scope(generator, stmt->false_statements->scope, bb_elsebegin);
        lstf_codenode_accept(stmt->false_statements, visitor);

        lstf_ir_basicblock *bb_elseend =
            lstf_codegenerator_get_current_basicblock_for_scope(generator, stmt->false_statements->scope);
        bb_elseend->successors[0] = bb_next;
    }

    lstf_ir_function_add_basic_block(fn, bb_next);
    lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_next);
}

static void
lstf_codegenerator_visit_lambda_expression(lstf_codevisitor *visitor, lstf_lambdaexpression *expr)
{
    // we must do two things:
    // 1. generate function code
    // 2. generate a closure
    
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    string *lambda_name = string_appendf(string_new(), "__lambda.%u", expr->id);

    // (1) generate function code
    lstf_ir_function *lambda_fn =
        lstf_ir_function_new_for_userfn(lambda_name->const_buffer,
            expr->parameters->length,
            ptr_hashset_num_elements(expr->captured_locals),
            lstf_functiontype_cast(
                lstf_expression_cast(expr)->value_type)->return_type->datatype_type != lstf_datatype_type_voidtype);
    lstf_ir_program_add_function(generator->ir, lambda_fn);

    ptr_list_append(generator->ir_functions, lambda_fn);

    lstf_ir_basicblock *bb_start = lstf_ir_basicblock_new();
    lambda_fn->entry_block->successors[0] = bb_start;

    lstf_codegenerator_set_current_basicblock_for_scope(generator, expr->scope, bb_start);
    lstf_ir_function_add_basic_block(lambda_fn, bb_start);

    lstf_codenode_accept_children(expr, visitor);
    // add return statement and connect to end
    if (expr->expression_body) {
        lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, expr->scope);
        lstf_ir_instruction *t_expression =
            lstf_codegenerator_get_temp_for_expression(generator, expr->expression_body);

        lstf_ir_basicblock_add_instruction(block,
                lstf_ir_returninstruction_new(lstf_codenode_cast(expr->expression_body), t_expression));
        block->successors[0] = lambda_fn->exit_block;
    }
    ptr_list_remove_last_link(generator->ir_functions);
    string_unref(lambda_name);

    // (2) generate code for a closure in the current basic block
    lstf_codegenerator_generate_closure_for_function(generator, lstf_codenode_cast(expr), lambda_fn);
}

static void
lstf_codegenerator_visit_literal(lstf_codevisitor *visitor, lstf_literal *lit)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    json_node *lit_json = lstf_expression_to_json(lstf_expression_cast(lit));
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(lit));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    assert(lit_json && "lstf_literal must be convertible to JSON");
    lstf_ir_instruction *lit_temp =
        lstf_ir_constantinstruction_new(lstf_codenode_cast(lit), lit_json);

    lstf_ir_basicblock_add_instruction(block, lit_temp);
    lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(lit), lit_temp);
}

static void
lstf_codegenerator_visit_member_access(lstf_codevisitor *visitor, lstf_memberaccess *access)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_expression *expr = lstf_expression_cast(access);
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(access));

    lstf_codenode_accept_children(access, visitor);

    if (!lstf_expression_is_lvalue(expr)) {
        lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
        lstf_codenode *current_function = lstf_codenode_get_containing_function(lstf_codenode_cast(access));

        // access a variable
        if (!access->inner) {
            int capture_id;
            if ((capture_id = lstf_memberaccess_get_capture_id(access)) != -1) {
                // access the up-value for this symbol
                // constants are never captured as up-values
                lstf_ir_instruction *guv_inst = lstf_ir_getupvalueinstruction_new(lstf_codenode_cast(expr), capture_id);
                lstf_ir_basicblock_add_instruction(block, guv_inst);
                lstf_codegenerator_set_temp_for_expression(generator, expr, guv_inst);
            } else if (expr->symbol_reference->symbol_type == lstf_symbol_type_variable ||
                    // only map this reference to constants that are already generated in the current function
                    (expr->symbol_reference->symbol_type == lstf_symbol_type_constant &&
                     lstf_codenode_get_containing_function(lstf_codenode_cast(expr->symbol_reference)) == current_function) ||
                    // map this reference to a local variable that was created to load the closure
                    (expr->symbol_reference->symbol_type == lstf_symbol_type_function &&
                     !ptr_hashset_is_empty(lstf_function_cast(expr->symbol_reference)->captured_locals))) {
                // perform loads on local variables, local constants, and locally-created function closures
                lstf_ir_instruction *load_inst = NULL;
                if (expr->symbol_reference->symbol_type == lstf_symbol_type_variable) {
                    // a local variable
                    load_inst = lstf_ir_loadinstruction_new(lstf_codenode_cast(access),
                            lstf_codegenerator_get_alloc_for_local(generator, expr->symbol_reference));
                } else {
                    // a local constant or a locally-created closure
                    load_inst = lstf_ir_loadinstruction_new(lstf_codenode_cast(access),
                            lstf_codegenerator_get_temp_for_symbol(generator,
                                current_scope,
                                expr->symbol_reference));
                }
                lstf_ir_basicblock_add_instruction(block, load_inst);
                lstf_codegenerator_set_temp_for_expression(generator, expr, load_inst);
            } else if (expr->symbol_reference->symbol_type == lstf_symbol_type_constant &&
                    lstf_codenode_get_containing_function(lstf_codenode_cast(expr->symbol_reference)) != current_function) {
                // constants from parent scopes are copied by value
                lstf_constant *constant = lstf_constant_cast(expr->symbol_reference);

                json_node *constant_json = lstf_expression_to_json(constant->expression);

                assert(constant_json && "lstf_constant should be convertible to a JSON expression");

                lstf_ir_instruction *const_inst =
                    lstf_ir_constantinstruction_new(lstf_codenode_cast(expr), constant_json);
                lstf_ir_basicblock_add_instruction(block, const_inst);

                lstf_codegenerator_set_temp_for_symbol(generator, current_scope, lstf_symbol_cast(constant), const_inst);
            } else if (expr->symbol_reference->symbol_type == lstf_symbol_type_function) {
                // we are referencing a function that is not associated with a
                // closure, so all we have to do is generate a new instruction
                // to load the function address
                lstf_ir_function *fn =
                    lstf_codegenerator_get_ir_function_from_codenode(generator,
                            lstf_codenode_cast(expr->symbol_reference));
                lstf_ir_instruction *loadfn_inst =
                    lstf_ir_loadfunctioninstruction_new(lstf_codenode_cast(expr), fn);

                lstf_ir_basicblock_add_instruction(block, loadfn_inst);
                lstf_codegenerator_set_temp_for_expression(generator, expr, loadfn_inst);
            }
        } else {
            lstf_ir_instruction *t_container = lstf_codegenerator_get_temp_for_expression(generator, access->inner);
            lstf_ir_instruction *t_member =
                lstf_ir_constantinstruction_new(lstf_codenode_cast(access), json_string_new(access->member_name));
            lstf_ir_instruction *gei =
                lstf_ir_getelementinstruction_new(lstf_codenode_cast(access), t_container, t_member);

            lstf_ir_basicblock_add_instruction(block, t_member);
            lstf_ir_basicblock_add_instruction(block, gei);

            lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(access), gei);
        }
    }
}

static void
lstf_codegenerator_visit_method_call(lstf_codevisitor *visitor, lstf_methodcall *mcall)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(mcall));
    lstf_function *called_function = lstf_function_cast(mcall->call->symbol_reference);
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    if (called_function && ptr_hashset_is_empty(called_function->captured_locals)) {
        // this is a direct (explicit) call
        ptr_list *callinst_arguments = ptr_list_new(lstf_ir_node_ref, lstf_ir_node_unref);
        // generate instructions to load arguments
        for (iterator it = ptr_list_iterator_create(mcall->arguments); it.has_next; it = iterator_next(it)) {
            lstf_expression *argument = iterator_get_item(it);
            lstf_codenode_accept(argument, visitor);

            ptr_list_append(callinst_arguments,
                    lstf_codegenerator_get_temp_for_expression(generator, argument));
        }

        lstf_ir_function *fn =
            lstf_codegenerator_get_ir_function_from_codenode(generator, lstf_codenode_cast(called_function));
        // generate the instruction to call the function
        if (!called_function->is_async || mcall->is_awaited) {
            // either:
            //  - we are calling a synchronous function
            //  - we are awaiting an asynchronous function, which means we call it
            //    normally and it will suspend us
            lstf_ir_instruction *call_inst =
                lstf_ir_callinstruction_new(lstf_codenode_cast(mcall), fn, callinst_arguments);
            lstf_ir_basicblock_add_instruction(block, call_inst);
            lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(mcall), call_inst);
        } else {
            // calling an asynchronous function but not awaiting it means we
            // have to schedule it in a new background coroutine
            lstf_ir_basicblock_add_instruction(block,
                    lstf_ir_scheduleinstruction_new(lstf_codenode_cast(mcall), fn, callinst_arguments));
        }
    } else {
        // this is an indirect call
        // generate instructions to load the function address (or closure) and arguments
        for (iterator it = ptr_list_iterator_create(mcall->arguments); it.has_next; it = iterator_next(it))
            lstf_codenode_accept((lstf_expression *)iterator_get_item(it), visitor);
        lstf_codenode_accept(mcall->call, visitor);
        lstf_ir_instruction *t_call = lstf_codegenerator_get_temp_for_expression(generator, mcall->call);
        ptr_list *icallinst_arguments = ptr_list_new((collection_item_ref_func) lstf_ir_node_ref,
                (collection_item_unref_func) lstf_ir_node_unref);
        for (iterator it = ptr_list_iterator_create(mcall->arguments); it.has_next; it = iterator_next(it)) {
            lstf_expression *argument = iterator_get_item(it);

            ptr_list_append(icallinst_arguments,
                    lstf_codegenerator_get_temp_for_expression(generator, argument));
        }
        lstf_functiontype *called_type = lstf_functiontype_cast(mcall->call->value_type);
        // just like with direct calls, the same logic applies (see above)
        if (!called_type->is_async || mcall->is_awaited) {
            lstf_ir_instruction *icall_inst =
                lstf_ir_indirectcallinstruction_new(lstf_codenode_cast(mcall),
                        t_call,
                        icallinst_arguments,
                        lstf_functiontype_cast(mcall->call->value_type)->return_type->datatype_type
                            != lstf_datatype_type_voidtype);
            lstf_ir_basicblock_add_instruction(block, icall_inst);
            lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(mcall), icall_inst);
        } else {
            lstf_ir_basicblock_add_instruction(block,
                    lstf_ir_indirectscheduleinstruction_new(lstf_codenode_cast(mcall),
                        t_call,
                        icallinst_arguments,
                        lstf_functiontype_cast(mcall->call->value_type)->return_type->datatype_type
                            != lstf_datatype_type_voidtype));
        }
    }
}

static void
lstf_codegenerator_visit_object(lstf_codevisitor *visitor, lstf_object *object)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(object));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    json_node *object_json = NULL;
    lstf_ir_instruction *object_temp = NULL;

    if ((object_json = lstf_expression_to_json(lstf_expression_cast(object)))) {
        object_temp = lstf_ir_constantinstruction_new(lstf_codenode_cast(object), object_json);
        lstf_ir_basicblock_add_instruction(block, object_temp);
    } else {
        object_temp =
            lstf_ir_constantinstruction_new(lstf_codenode_cast(object),
                    object->is_pattern ? json_object_pattern_new() : json_object_new());
        lstf_ir_basicblock_add_instruction(block, object_temp);

        for (iterator it = ptr_list_iterator_create(object->members_list); it.has_next; it = iterator_next(it)) {
            lstf_objectproperty *property = iterator_get_item(it);
            lstf_ir_instruction *property_value_temp = NULL;
            json_node *property_value_json = NULL;

            if ((property_value_json = lstf_expression_to_json(property->value))) {
                property_value_temp =
                    lstf_ir_constantinstruction_new(lstf_codenode_cast(property), property_value_json);
                lstf_ir_basicblock_add_instruction(block, property_value_temp);
            } else {
                lstf_codenode_accept(property->value, visitor);
                property_value_temp = lstf_codegenerator_get_temp_for_expression(generator, property->value);
            }

            lstf_ir_instruction *index_temp =
                lstf_ir_constantinstruction_new(lstf_codenode_cast(property),
                        json_string_new(lstf_symbol_cast(property)->name));
            lstf_ir_basicblock_add_instruction(block, index_temp);
            lstf_ir_basicblock_add_instruction(block,
                    lstf_ir_setelementinstruction_new(lstf_codenode_cast(property),
                        object_temp, index_temp, property_value_temp));
        }
    }

    lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(object), object_temp);
}

static void
lstf_codegenerator_visit_return_statement(lstf_codevisitor *visitor, lstf_returnstatement *stmt)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(stmt));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    lstf_codenode_accept_children(stmt, visitor);

    lstf_ir_instruction *t_expression =
        stmt->expression ? lstf_codegenerator_get_temp_for_expression(generator, stmt->expression) : NULL;
    lstf_ir_basicblock_add_instruction(block,
            lstf_ir_returninstruction_new(lstf_codenode_cast(stmt), t_expression));

    // return statement ends this basic block
    lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);
    block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);
    block->successors[0] = fn->exit_block;

    // create a new "dead" basic block (the entry block never leads to it)
    lstf_ir_basicblock *bb_dead = lstf_ir_basicblock_new();
    lstf_ir_function_add_basic_block(fn, bb_dead);
    lstf_codegenerator_set_current_basicblock_for_scope(generator, current_scope, bb_dead);
}

static void
lstf_codegenerator_visit_unary_expression(lstf_codevisitor *visitor, lstf_unaryexpression *expr)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(expr));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    // t_inner = <...>
    // t_expr = <unary op> t_inner

    lstf_codenode_accept_children(expr, visitor);

    lstf_vm_opcode opcode;

    switch (expr->op) {
        case lstf_unaryoperator_bitwise_not:
            opcode = lstf_vm_op_not;
            break;
        case lstf_unaryoperator_logical_not:
            opcode = lstf_vm_op_lnot;
            break;
        case lstf_unaryoperator_negate:
            opcode = lstf_vm_op_neg;
            break;
        default:
            fprintf(stderr, "%s: unreachable code: invalid expression op `%u'\n",
                    __func__, expr->op);
            abort();
    }

    lstf_ir_instruction *t_inner = lstf_codegenerator_get_temp_for_expression(generator, expr->inner);
    lstf_ir_instruction *expr_temp =
        lstf_ir_unaryinstruction_new(lstf_codenode_cast(expr), opcode, t_inner);
    lstf_ir_basicblock_add_instruction(block, expr_temp);

    lstf_codegenerator_set_temp_for_expression(generator, lstf_expression_cast(expr), expr_temp);
}

static void
lstf_codegenerator_visit_variable(lstf_codevisitor *visitor, lstf_variable *variable)
{
    lstf_codegenerator *generator = (lstf_codegenerator *)visitor;
    lstf_scope *current_scope = lstf_codenode_get_containing_scope(lstf_codenode_cast(variable));
    lstf_ir_basicblock *block = lstf_codegenerator_get_current_basicblock_for_scope(generator, current_scope);

    if (!lstf_symbol_cast(variable)->is_builtin) {
        lstf_ir_function *fn = lstf_codegenerator_get_current_function(generator);
        lstf_ir_instruction *alloc_inst =
            lstf_ir_allocinstruction_new(lstf_codenode_cast(variable), !variable->initializer);
        lstf_ir_basicblock_add_instruction(block, alloc_inst);
        lstf_codegenerator_set_alloc_for_local(generator, lstf_symbol_cast(variable), alloc_inst, fn);

        if (variable->initializer) {
            // generate temporary for the initializer expression
            lstf_codenode_accept_children(variable, visitor);

            // associate variable with the temporary
            lstf_ir_instruction *t_initializer = lstf_codegenerator_get_temp_for_expression(generator, variable->initializer);
            lstf_codegenerator_set_temp_for_symbol(generator, current_scope, lstf_symbol_cast(variable), t_initializer);

            // generate virtual store instruction
            lstf_ir_basicblock_add_instruction(block,
                    lstf_ir_storeinstruction_new(lstf_codenode_cast(variable), t_initializer, alloc_inst));
        }
    } else {
        // TODO: handle built-ins like `server_path` and `project_files` here
    }
}

static const lstf_codevisitor_vtable codegenerator_vtable = {
    lstf_codegenerator_visit_array,
    lstf_codegenerator_visit_assert_statement,
    lstf_codegenerator_visit_assignment,
    lstf_codegenerator_visit_binary_expression,
    lstf_codegenerator_visit_block,
    lstf_codegenerator_visit_conditional_expression,
    lstf_codegenerator_visit_constant,
    NULL /* visit_data_type */,
    lstf_codegenerator_visit_declaration,
    lstf_codegenerator_visit_element_access,
    NULL /* visit_ellipsis */,
    NULL /* visit_enum */,
    NULL /* visit_expression */,
    lstf_codegenerator_visit_expression_statement,
    lstf_codegenerator_visit_file,
    lstf_codegenerator_visit_function,
    lstf_codegenerator_visit_if_statement,
    NULL /* visit_interface */,
    NULL /* visit_interface_property */,
    lstf_codegenerator_visit_lambda_expression,
    lstf_codegenerator_visit_literal,
    lstf_codegenerator_visit_member_access,
    lstf_codegenerator_visit_method_call,
    lstf_codegenerator_visit_object,
    NULL /* visit_object_property */,
    lstf_codegenerator_visit_return_statement,
    NULL /* visit_type_alias */,
    lstf_codegenerator_visit_unary_expression,
    lstf_codegenerator_visit_variable
};

lstf_codegenerator *lstf_codegenerator_new(lstf_file *file)
{
    lstf_codegenerator *generator = calloc(1, sizeof *generator);

    if (!generator) {
        perror("could not lstf_codegenerator");
        abort();
    }

    lstf_codevisitor_construct((lstf_codevisitor *)generator, &codegenerator_vtable);

    generator->floating = true;
    generator->file = lstf_file_ref(file);
    generator->ir = lstf_ir_program_ref(lstf_ir_program_new());
    generator->scopes_to_basicblocks = ptr_hashmap_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref, (collection_item_unref_func) lstf_codenode_unref,
            NULL,
            (collection_item_ref_func) lstf_ir_node_ref, (collection_item_unref_func) lstf_ir_node_unref);
    generator->exprs_to_temps = ptr_hashmap_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref, (collection_item_unref_func) lstf_codenode_unref,
            NULL,
            (collection_item_ref_func) lstf_ir_node_ref, (collection_item_unref_func) lstf_ir_node_unref);
    generator->scopes_to_symbols_to_temps = ptr_hashmap_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref, (collection_item_unref_func) lstf_codenode_unref,
            NULL,
            NULL, (collection_item_unref_func) ptr_hashmap_destroy);
    generator->ir_functions = ptr_list_new((collection_item_ref_func) lstf_ir_node_ref,
            (collection_item_unref_func) lstf_ir_node_unref);
    generator->codenodes_to_ir_functions = ptr_hashmap_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref, (collection_item_unref_func) lstf_codenode_unref,
            NULL,
            (collection_item_ref_func) lstf_ir_node_ref, (collection_item_unref_func) lstf_ir_node_unref);
    generator->local_allocs = ptr_hashmap_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref, (collection_item_unref_func) lstf_codenode_unref,
            NULL,
            (collection_item_ref_func) lstf_ir_node_ref, (collection_item_unref_func) lstf_ir_node_unref);
    if (!(generator->output = outputstream_new_from_buffer(NULL, 1024, true))) {
        perror("could not create output stream for code generator");
        abort();
    }

    return generator;
}

lstf_codegenerator *lstf_codegenerator_ref(lstf_codegenerator *generator)
{
    if (!generator)
        return NULL;

    assert(generator->floating || generator->refcount > 0);

    if (generator->floating) {
        generator->floating = false;
        generator->refcount = 1;
    } else {
        generator->refcount++;
    }

    return generator;
}

static void lstf_codegenerator_destroy(lstf_codegenerator *generator)
{
    lstf_file_unref(generator->file);
    lstf_ir_program_unref(generator->ir);
    ptr_hashmap_destroy(generator->scopes_to_basicblocks);
    ptr_hashmap_destroy(generator->scopes_to_symbols_to_temps);
    ptr_hashmap_destroy(generator->exprs_to_temps);
    ptr_list_destroy(generator->ir_functions);
    ptr_hashmap_destroy(generator->codenodes_to_ir_functions);
    ptr_hashmap_destroy(generator->local_allocs);
    outputstream_unref(generator->output);
    free(generator);
}

void lstf_codegenerator_unref(lstf_codegenerator *generator)
{
    if (!generator)
        return;

    assert(generator->floating || generator->refcount > 0);

    if (generator->floating || --generator->refcount == 0)
        lstf_codegenerator_destroy(generator);
}

void lstf_codegenerator_compile(lstf_codegenerator *generator)
{
    lstf_codevisitor_visit_file((lstf_codevisitor *)generator, generator->file);

    if (generator->num_errors == 0)
        generator->num_errors = lstf_ir_program_analyze(generator->ir);

    if (generator->num_errors == 0) {
        lstf_bc_program *bc = lstf_ir_program_assemble(generator->ir);
        bool status = lstf_bc_program_serialize_to_binary(bc, generator->output);
        lstf_bc_program_destroy(bc);

        if (!status) {
            lstf_report_error(NULL, "failed to serialize bytecode");
            generator->num_errors++;
        }
    }
}

const uint8_t *lstf_codegenerator_get_compiled_bytecode(const lstf_codegenerator *generator,
                                                        size_t                   *buffer_size)
{
    if (generator->num_errors > 0)
        return NULL;
    if (buffer_size)
        *buffer_size = generator->output->buffer_offset;
    return generator->output->buffer;
}
