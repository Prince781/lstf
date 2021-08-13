#include "bytecode/lstf-bc-function.h"
#include "bytecode/lstf-bc-instruction.h"
#include "bytecode/lstf-bc-program.h"
#include "bytecode/lstf-bc-serialize.h"
#include "data-structures/string-builder.h"
#include "io/inputstream.h"
#include "io/outputstream.h"
#include "vm/lstf-virtualmachine.h"
#include "vm/lstf-vm-loader.h"
#include "vm/lstf-vm-program.h"
#include "vm/lstf-vm-status.h"
#include "json/json.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    int retval = 0;

    /**
     * --- code ---
     * factorial:                   # offset: 0x00, locals (after): 0
     *          params 1            # offset: 0x00, locals (after): 1
     *          load frame(0)       # offset: 0x02, locals (after): 2
     *          load 1              # offset: 0x07, locals (after): 3
     *          is <=               # offset: 0x0A, locals (after): 2
     *          else <false>        # offset: 0x0B, locals (after): 1
     *          load 1              # offset: 0x10, locals (after): 2
     *          return              # offset: 0x13
     * <false>: load frame(0)       # offset: 0x14, locals (after): 2
     *          load frame(0)       # offset: 0x19, locals (after): 3
     *          load 1              # offset: 0x1E, locals (after): 4
     *          sub                 # offset: 0x21, locals (after): 3
     *          call factorial      # offset: 0x22, locals (after): 3
     *          mul                 # offset: 0x27, locals (after): 2
     *          return              # offset: 0x28
     * 
     * main:                        # offset: 0x29
     *          load 10
     *          call factorial
     *          print               # 10! = 3628800
     *          exit 0
     */
    lstf_bc_program *program = lstf_bc_program_new(NULL);

    lstf_bc_function *fact_fun = lstf_bc_function_new("factorial");

    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_params_new(1));
    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_load_frameoffset_new(0));
    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_load_expression_new(json_integer_new(1)));
    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_lessthan_equal_new());
    lstf_bc_instruction *else1 = lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_else_new(NULL));
    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_load_expression_new(json_integer_new(1)));
    lstf_bc_function_add_instruction(fact_fun, lstf_bc_instruction_return_new());
    lstf_bc_instruction_resolve_jump(else1,
            lstf_bc_function_add_instruction(fact_fun,
                lstf_bc_instruction_load_frameoffset_new(0)));
    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_load_frameoffset_new(0));
    lstf_bc_function_add_instruction(fact_fun,
            lstf_bc_instruction_load_expression_new(json_integer_new(1)));
    lstf_bc_function_add_instruction(fact_fun, lstf_bc_instruction_sub_new());
    lstf_bc_function_add_instruction(fact_fun, lstf_bc_instruction_call_new(fact_fun));
    lstf_bc_function_add_instruction(fact_fun, lstf_bc_instruction_mul_new());
    lstf_bc_function_add_instruction(fact_fun, lstf_bc_instruction_return_new());

    lstf_bc_program_add_function(program, fact_fun);

    lstf_bc_function *main_fun = lstf_bc_function_new("main");

    lstf_bc_function_add_instruction(main_fun,
            lstf_bc_instruction_load_expression_new(json_integer_new(10)));
    lstf_bc_function_add_instruction(main_fun, lstf_bc_instruction_call_new(fact_fun));
    lstf_bc_function_add_instruction(main_fun, lstf_bc_instruction_print_new());
    lstf_bc_function_add_instruction(main_fun, lstf_bc_instruction_exit_new(0));

    lstf_bc_program_add_function(program, main_fun);

    // output stream for program code
    outputstream *p_ostream = outputstream_new_from_buffer(NULL, 0, true);

    if (lstf_bc_program_serialize_to_binary(program, p_ostream)) {
        lstf_vm_loader_error error;
        lstf_vm_program *vm_program = lstf_vm_loader_load_from_buffer(p_ostream->buffer, 
                p_ostream->buffer_offset, &error);

        if (vm_program) {
            outputstream *vm_ostream = outputstream_new_from_buffer(NULL, 0, true);
            lstf_virtualmachine *vm = lstf_virtualmachine_new(vm_program, vm_ostream, false);

            if (!lstf_virtualmachine_run(vm)) {
                if (vm->last_status == lstf_vm_status_exited) {
                    retval = vm->return_code;
                    inputstream *istream = inputstream_new_from_static_buffer(vm->ostream->buffer, vm->ostream->buffer_offset);
                    // check output
                    const char expected_output[] = "3628800\n";
                    char buffer[128] = { 0 };
                    if (inputstream_read(istream, buffer, sizeof buffer - 1) > 0) {
                        if (strncmp(buffer, expected_output, sizeof buffer) != 0) {
                            retval = 1;
                            fprintf(stderr, "---expected output:\n%s---actual output:\n%s",
                                    expected_output, buffer);
                        } else {
                            printf("factorial program exited with expected output:\n%s", expected_output);
                        }
                    } else {
                        retval = 99;
                        fprintf(stderr, "could not check output of VM: %s\n", strerror(errno));
                    }
                    inputstream_unref(istream);
                } else {
                    retval = 1;     // VM encountered some fatal error
                    fprintf(stderr, "VM encountered a fatal error: %s.\n",
                            lstf_vm_status_to_string(vm->last_status));
                }
            } else {
                retval = 1;         // VM should terminate
                fprintf(stderr, "VM was interrupted.\n");
            }

            lstf_virtualmachine_destroy(vm);
        } else {
            retval = 99;
            fprintf(stderr, "failed to load program\n");
            // TODO: make use of [[error]]
        }
    } else {
        retval = 99;
        fprintf(stderr, "failed to assemble code: %s\n", strerror(errno));
    }

    // dump assembly to a file for inspection
    // if (retval != 0) {
    //     outputstream *fostream = outputstream_new_from_path("factorial.lstfc", "w");
    //     if (fostream) {
    //         fprintf(stderr, "outputting VM code...");
    //         fflush(stderr);
    //         if (lstf_bc_program_serialize_to_binary(program, fostream)) {
    //             char *filename = outputstream_get_name(fostream);
    //             fprintf(stderr, "done. check %s\n", filename);
    //             free(filename);
    //         } else {
    //             fprintf(stderr, "failed: %s\n", strerror(errno));
    //         }
    //         outputstream_unref(fostream);
    //     }
    // }

    lstf_bc_program_destroy(program);
    outputstream_unref(p_ostream);
    return retval;
}
