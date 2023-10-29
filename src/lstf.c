#include "compiler/lstf-codegenerator.h"
#include "compiler/lstf-ir-program.h"
#include "compiler/lstf-parser.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-symbolresolver.h"
#include "compiler/lstf-semanticanalyzer.h"
#include "data-structures/string-builder.h"
#include "data-structures/array.h"
#include "io/inputstream.h"
#include "io/outputstream.h"
#include "io/io-common.h"
#include "vm/lstf-virtualmachine.h"
#include "vm/lstf-vm-loader.h"
#include "vm/lstf-vm-program.h"
#include "vm/lstf-vm-status.h"
#include "util.h"
#include "version.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Compiler options.
 */
struct lstf_options {
    bool disable_resolver;
    bool disable_analyzer;
    bool disable_codegen;
    bool disable_interpreter;
    bool no_lsp;
    bool emit_ir;
    bool output_codegen;                // flag: -c
    bool disassemble;                   // flag: -d
    array(ptrdiff_t) *breakpoints;
    ptr_hashmap *variables;             // flag: -e
    const char *input_filename;
    const char *output_filename;
    const char *expected_output;
};

static inline char *suffix(const char *str)
{
    char *ext = strrchr(str, '.');

    return ext != NULL ? (*(ext + 1) ? ext + 1 : NULL) : NULL;
}

static char *substitute_file_extension(const char *filename, const char *new_ext)
{
    static char bn_buffer[FILENAME_MAX];
    const char *suffix_ptr = suffix(filename);
    if (!suffix_ptr) {
        snprintf(bn_buffer, sizeof bn_buffer - 1, "%s.%s", filename, new_ext);
    } else {
        size_t basename_len = (size_t)(suffix_ptr - filename);
        size_t ext_len = strlen(new_ext);
        if (basename_len > sizeof bn_buffer - ext_len - 1)
            basename_len = sizeof bn_buffer - ext_len - 1;
        snprintf(bn_buffer, basename_len, "%s", filename);
        snprintf(&bn_buffer[basename_len - 1], sizeof bn_buffer - basename_len, ".%s", new_ext);
    }
    return bn_buffer;
}

static const char usage_message[] =
"usage: %s [options] file...\n"
"\n"
"usage: %s [-e NAME=VALUE[, ...]] script.lstf\n"
"        runs a LSTF script\n"
"\n"
"usage: %s script.lstfc\n"
"        runs a compiled LSTF script\n"
"\n"
"usage: %s -c script.lstf [-o script.lstfc]\n"
"        compiles a LSTF script to bytecode\n"
"\n"
"usage: %s -a script.lstfa [-o script.lstfc]\n"
"        assembles LSTF assembly code to bytecode\n"
"\n"
"usage: %s -d script.lstfc [-o script.lstfa]\n"
"        disassembles LSTF bytecode to assembly code\n"
"\n"
"usage: %s -d script.lstf [-o script.lstfa]\n"
"        compiles a LSTF script to assembly code\n"
"\n"
"note: you can use \"-\" with -o to output to stdout.\n"
"\n"
"Other Options:\n"
"  --disable=<stage>        Disable a stage. Also disables dependent stages.\n"
"                           The stages are `resolver`, `analyzer`, `codegen`,\n"
"                           and `interpreter`. Used for debugging and testing.\n"
"  -no-lsp                  Don't error out when language server protocol\n"
"                           requirements aren't met. (Use this when you just want\n"
"                           to test the VM without any LSP features.)\n"
"  -emit-ir                 Output IR to a Graphviz file in the current directory.\n"
"  -expect <string>         Test the program output against <string>.\n"
"  -break <offset>          Enable debug mode and break at the offset (in hexadecimal).\n"
"\n"
"Flags:\n"
"  -e NAME=VALUE            Set variable NAME to VALUE.\n"
"  -h                       Show this help message.\n"
"  -v                       Print version.\n";

static void
print_usage(const char *progname)
{
    fprintf(stderr, usage_message, progname, progname, progname, progname, progname, progname, progname);
    fprintf(stderr, "\n");
}

static void report_load_error(const char          *progname,
                              const char          *filename,
                              lstf_vm_loader_error error)
{
    (void) progname;
    switch (error) {
    case lstf_vm_loader_error_read:
        lstf_report_error(NULL, "%s: VM loader: read error - %s", filename, strerror(errno));
        break;
    case lstf_vm_loader_error_invalid_section_size:
        lstf_report_error(NULL, "%s: VM loader: invalid section size(s)", filename);
        break;
    case lstf_vm_loader_error_invalid_debug_info:
        lstf_report_error(NULL, "%s: VM loader: invalid debug info", filename);
        break;
    case lstf_vm_loader_error_invalid_debug_size:
        lstf_report_error(NULL, "%s: VM loader: invalid debug size", filename);
        break;
    case lstf_vm_loader_error_invalid_magic_value:
        lstf_report_error(NULL, "%s: VM loader: invalid magic value", filename);
        break;
    case lstf_vm_loader_error_invalid_section_name:
        lstf_report_error(NULL, "%s: VM loader: invalid section name", filename);
        break;
    case lstf_vm_loader_error_no_code_section:
        lstf_report_error(NULL, "%s: VM loader: no code section", filename);
        break;
    case lstf_vm_loader_error_out_of_memory:
        lstf_report_error(NULL, "%s: VM loader: out of memory", filename);
        break;
    case lstf_vm_loader_error_source_filename_too_long:
        lstf_report_error(NULL, "%s: VM loader: source filename too long", filename);
        break;
    case lstf_vm_loader_error_too_long_section_name:
        lstf_report_error(NULL, "%s: VM loader: section name too long", filename);
        break;
    case lstf_vm_loader_error_zero_section_size:
        lstf_report_error(NULL, "%s: VM loader: section size was zero", filename);
        break;
    case lstf_vm_loader_error_invalid_entry_point:
        lstf_report_error(NULL, "%s: VM loader: invalid entry point", filename);
        break;
    case lstf_vm_loader_error_none:
        break;
    }
}

static int run_program(lstf_vm_program *program, struct lstf_options options)
{
    int retval = 0;
    lstf_virtualmachine *vm =
        lstf_virtualmachine_new(program,
            options.expected_output ? outputstream_new_from_buffer(NULL, 0, false) : NULL,
            false);
    outputstream *os = outputstream_new_from_file(stdout, false);
    if (options.breakpoints) {
        for (size_t i = 0; i < options.breakpoints->length; i++) {
            if (!lstf_virtualmachine_add_breakpoint(vm, options.breakpoints->elements[i])) {
                lstf_report_error(NULL, "failed to add breakpoint %td - out of range", options.breakpoints->elements[i]);
                lstf_virtualmachine_destroy(vm);
                return 1;
            }
        }
        vm->debug = true;
    }
    if (options.variables) {
        for (iterator it = ptr_hashmap_iterator_create(options.variables); it.has_next; it = iterator_next(it)) {
            ptr_hashmap_entry *entry = iterator_get_item(it);
            lstf_virtualmachine_set_variable(vm, entry->key, entry->value);
        }
    }

    bool first_hit_debugger = false;
    bool print_prompt = true;   // don't print prompt until after a newline
    // ptr_list<uint8_t *>
    ptr_list *pc_offsets = ptr_list_new(NULL, NULL);
    iterator pc_it = {0};

    // Run the VM as long as it's in a non-error state. Returns when paused (true) or exiting (false).
    while (lstf_virtualmachine_run(vm)) {
        if (!first_hit_debugger && vm->last_status == lstf_vm_status_hit_breakpoint) {
            ptr_list_clear(pc_offsets);
            lstf_vm_program_disassemble(vm->program, os, vm->last_pc, pc_offsets);
            pc_it = ptr_list_iterator_create(pc_offsets);
            vm->next_stop = vm->last_pc; // keep VM stopped at current PC
        }

        if (print_prompt)
            outputstream_printf(os, "(lstf debug) ");
        first_hit_debugger = true;

        char command = getchar();
        switch (command) {
        case 'b': {
            // print current coroutine and suspended coroutines
            outputstream_printf(os, "run list: %lu items\n", vm->run_queue->length);
            for (iterator cr_it = ptr_list_iterator_create(vm->run_queue);
                 cr_it.has_next; cr_it = iterator_next(cr_it)) {
                const lstf_vm_coroutine *cr = iterator_get_item(cr_it);
                outputstream_printf(
                    os, " - coroutine @ %#lx (%u frames)\n",
                    (unsigned long)(cr->pc - vm->program->code), cr->stack->n_frames);
            }

            outputstream_printf(os, "suspended list: %lu items\n", vm->suspended_list->length);
            for (iterator cr_it = ptr_list_iterator_create(vm->suspended_list);
                 cr_it.has_next; cr_it = iterator_next(cr_it)) {
                const lstf_vm_coroutine *cr = iterator_get_item(cr_it);
                outputstream_printf(os,
                                    " - coroutine @ %#lx (%u frames) - waiting "
                                    "for %u I/O events\n",
                                    (unsigned long)(cr->pc - vm->program->code),
                                    cr->stack->n_frames, cr->outstanding_io);
            }
            print_prompt = false;
        } break;
        case 'c':       // continue VM
        case EOF:
            vm->next_stop = NULL;
            first_hit_debugger = false;
            break;
        case 'd': {
            if (vm->last_status == lstf_vm_status_hit_breakpoint)
                lstf_vm_program_disassemble(vm->program, os, vm->last_pc, NULL);
            print_prompt = false;
        } break;
        case 'f': {
            // iterate over items in current stack frame
            if (ptr_list_is_empty(vm->run_queue)) {
                // TODO: selecting a coroutine by ID
                outputstream_printf(os, "all coroutines suspended for I/O\n");
            } else {
                // don't need to grab reference here
                lstf_vm_coroutine const *cr = ptr_list_node_get_data(vm->run_queue->head, lstf_vm_coroutine *);
                lstf_vm_stackframe const *cf = &cr->stack->frames[cr->stack->n_frames-1];
                if (!cr->stack->n_values)
                    outputstream_printf(os, "stack frame is empty.\n");
                else {
                    outputstream_printf(os, "stack:\n");
                    for (uint64_t offset = cf->offset;
                         offset < cr->stack->n_values; ++offset) {
                      outputstream_printf(os, "|%0#5" PRIx64 "| ", offset);
                      lstf_vm_value *value = &cr->stack->values[offset];
                      switch (value->value_type) {
                      case lstf_vm_value_type_null:
                          outputstream_printf(os, "<null> ");
                          break;
                      case lstf_vm_value_type_integer:
                          outputstream_printf(os, "<int> ");
                          break;
                      case lstf_vm_value_type_double:
                          outputstream_printf(os, "<double> ");
                          break;
                      case lstf_vm_value_type_boolean:
                          outputstream_printf(os, "<bool> ");
                          break;
                      case lstf_vm_value_type_string:
                          outputstream_printf(os, "<string> ");
                          break;
                      case lstf_vm_value_type_object_ref:
                      case lstf_vm_value_type_array_ref:
                          outputstream_printf(os, "<JSON> ");
                          break;
                      case lstf_vm_value_type_pattern_ref:
                          outputstream_printf(os, "<JSON pattern> ");
                          break;
                      case lstf_vm_value_type_code_address:
                          outputstream_printf(os, "<code address> ");
                          break;
                      case lstf_vm_value_type_closure:
                          outputstream_printf(os, "<closure> ");
                          break;
                      }
                      lstf_vm_value_print(value, vm->program, os);
                    }
                }
            }
            print_prompt = false;
        } break;
        case 'h': {
            outputstream_printf(os, "debug commands:\n"
                    "b - print (b)acktrace of all running and suspended coroutines\n"
                    "c - (c)ontinue\n"
                    "d - show (d)isassembly\n"
                    "f - examine current stack (f)rame\n"
                    "h - print help\n"
                    "n - continue to (n)ext instruction\n");
            // TODO:
            // s <coroutine ID> - (s)witch to another coroutine
            print_prompt = false;
        } break;
        case 'n': {
            if (pc_it.has_next) {
                uint8_t *next_pc = iterator_get_item(pc_it);
                pc_it = iterator_next(pc_it);
                if (*next_pc == lstf_vm_op_params)
                    outputstream_printf(os, "at end of function\n");
                else {
                    // temporarily breakpoint next instruction
                    vm->next_stop = next_pc;
                    outputstream_printf(os, "breaking at %0#lx\n", (uint64_t)(vm->next_stop - vm->program->code));
                }
            } else {
                outputstream_printf(os, "at end of instruction stream\n");
            }
            first_hit_debugger = false;
            print_prompt = false;
        } break;
        case '\n':
            print_prompt = true;
            break;
        default:
            outputstream_printf(os, "unsupported command `%c'. type `h' for help.\n", command);
            print_prompt = false;
            break;
        }
    }
    if (vm->last_status != lstf_vm_status_exited) {
        lstf_report_error(NULL, "VM: %s", lstf_vm_status_to_string(vm->last_status));
        lstf_vm_program_disassemble(vm->program, os, vm->last_pc, NULL);
        retval = 1;
    } else {
        retval = vm->return_code;
        if (options.expected_output) {
            // test the output buffer
            inputstream *istream = inputstream_new_from_static_buffer(vm->ostream->buffer, vm->ostream->buffer_size);
            char *buffer = calloc(1, vm->ostream->buffer_offset + 1);

            if (inputstream_read(istream, buffer, vm->ostream->buffer_offset) > 0 &&
                    strcmp(buffer, options.expected_output) == 0) {
                lstf_report_note(NULL, "VM output matches");
            } else {
                lstf_report_error(NULL, "VM output differs");
                lstf_report_note(NULL, "\n---expected:---\n%s------\n---got:---\n%s------",
                        options.expected_output, buffer);
                retval = 1;
            }

            free(buffer);
            inputstream_unref(istream);
        }
    }

    lstf_virtualmachine_destroy(vm);
    outputstream_unref(os);
    ptr_list_destroy(pc_offsets);
    return retval;
}

static int disassemble_program(lstf_vm_program *program, struct lstf_options options)
{
    int retval = 0;
    const char *disas_filename = options.output_filename;
    outputstream *os;
    if (!disas_filename)
        disas_filename = substitute_file_extension(options.input_filename, "lstfa");
    if (strcmp(disas_filename, "-") == 0)
        os = outputstream_new_from_file(stdout, false);
    else
        os = outputstream_new_from_path(disas_filename, "w");
    if (!os) {
        lstf_report_error(NULL, "failed to open %s: %s", disas_filename, strerror(errno));
        retval = 99;
    } else if (!lstf_vm_program_disassemble(program, os, NULL, NULL)) {
        lstf_report_error(NULL, "failed to write disassembly to %s: %s", disas_filename, strerror(errno));
        retval = 99;
    }
    outputstream_unref(os);
    return retval;
}

static int
compile_lstf_script(const char *progname, struct lstf_options options)
{
    (void) progname;
    lstf_file *script = lstf_file_load(options.input_filename);
    if (!script) {
        lstf_report_error(NULL, "%s: %s", options.input_filename, strerror(errno));
        fprintf(stderr, "compilation terminated.\n");
        return 1;
    }

    lstf_parser *parser = NULL;
    lstf_symbolresolver *resolver = NULL;
    lstf_semanticanalyzer *analyzer = NULL;
    lstf_codegenerator *generator = NULL;
    int retval = 0;
    unsigned num_errors = 0;
    size_t bytecode_length = 0;
    const uint8_t *bytecode = NULL;
    lstf_vm_loader_error loader_error = 0;
    lstf_vm_program *program = NULL;

    parser = lstf_parser_new(script);
    lstf_parser_parse(parser);
    if ((num_errors = parser->scanner->num_errors + parser->num_errors) > 0)
        goto cleanup;

    if (options.disable_resolver)
        goto cleanup;
    resolver = lstf_symbolresolver_new(script);
    lstf_symbolresolver_resolve(resolver);
    if ((num_errors = resolver->num_errors) > 0)
        goto cleanup;

    if (options.disable_analyzer)
        goto cleanup;
    analyzer = lstf_semanticanalyzer_new(script);
    if (options.no_lsp) {
        analyzer->encountered_server_path_assignment = true;
        analyzer->encountered_project_files_assignment = true;
    }
    lstf_semanticanalyzer_analyze(analyzer);
    if ((num_errors = analyzer->num_errors) > 0)
        goto cleanup;

    if (options.disable_codegen)
        goto cleanup;
    generator = lstf_codegenerator_new(script);
    lstf_codegenerator_compile(generator);
    if ((num_errors = generator->num_errors) > 0)
        goto cleanup;

    if (options.emit_ir) {
        const char *ir_filename = substitute_file_extension(options.input_filename, "dot");
        if (!lstf_ir_program_visualize(generator->ir, ir_filename))
            lstf_report_error(NULL, "failed to emit IR to %s: %s", ir_filename, strerror(errno));
    }
    if (!(bytecode = lstf_codegenerator_get_compiled_bytecode(generator, &bytecode_length)))
        goto cleanup;

    if (options.output_codegen) {
        const char *bc_filename = options.output_filename;
        outputstream *os;
        if (!bc_filename)
            bc_filename = substitute_file_extension(options.input_filename, "lstfc");
        if (strcmp(bc_filename, "-") == 0)
            os = outputstream_new_from_file(stdout, false);
        else
            os = outputstream_new_from_path(bc_filename, "wb");
        if (!os) {
            lstf_report_error(NULL, "failed to write to %s: %s", bc_filename, strerror(errno));
            retval = 99;
        } else if (!outputstream_write(os, bytecode, bytecode_length)) {
            lstf_report_error(NULL, "failed to write bytecode to %s: %s", bc_filename, strerror(errno));
            retval = 99;
        }
        outputstream_unref(os);
    } else {
        if (!(program = lstf_vm_loader_load_from_buffer(bytecode, bytecode_length, &loader_error))) {
            report_load_error(progname, options.input_filename, loader_error);
            retval = 99;
            goto cleanup;
        }
    }

cleanup:
    lstf_parser_unref(parser);
    lstf_symbolresolver_unref(resolver);
    lstf_semanticanalyzer_unref(analyzer);
    lstf_codegenerator_unref(generator);

    if (num_errors > 0) {
        fprintf(stderr, "%u error(s) generated.\n", num_errors);
        retval = 1;
    } else if (program) {
        // all clear. we can use the program
        if (options.disassemble) {
            retval = disassemble_program(program, options);
            lstf_vm_program_unref(program);
        } else if (!options.disable_interpreter) {
            retval = run_program(program, options);
        }
    }
    return retval;
}

static lstf_vm_program *load_file(const char *progname, const char *filename)
{
    lstf_vm_loader_error error = lstf_vm_loader_error_none;
    lstf_vm_program *program = lstf_vm_loader_load_from_path(filename, &error);

    if (!program) {
        report_load_error(progname, filename, error);
        return NULL;
    }

    return program;
}

static int load_and_run_file(const char *progname, struct lstf_options options)
{
    lstf_vm_program *program;

    if (!(program = load_file(progname, options.input_filename)))
        return 99;
    return run_program(program, options);
}

static int load_and_disassemble_file(const char *progname, struct lstf_options options)
{
    lstf_vm_program *program;
    int retval = 0;

    if (!(program = load_file(progname, options.input_filename)))
        return 99;

    retval = disassemble_program(program, options);
    lstf_vm_program_unref(program);
    return retval;
}

int main(int argc, char *argv[])
{
    (void) argc;

    struct lstf_options options = { 0 };
    bool is_assembling = false;     // flag: -a
    bool is_compiling = false;
    bool is_interpreting = false;
    bool is_input_arg = false;
    bool is_output_arg = false;
    int retval = 0;

    for (char **argp = &argv[1]; *argp; argp++) {
        char *option = *argp;

        if (strncmp(option, "--disable", sizeof "--disable" - 1) == 0) {
            char *eqc = strchr(option, '=');
            char *stage = NULL;

            if (eqc) {
                *eqc = '\0';
                stage = eqc + 1;
            } else if (*++argp) {
                stage = *argp;
            }

            if (stage && strcmp(stage, "resolver") == 0)
                options.disable_resolver = true;
            else if (stage && strcmp(stage, "analyzer") == 0)
                options.disable_analyzer = true;
            else if (stage && strcmp(stage, "codegen") == 0)
                options.disable_codegen = true;
            else if (stage && strcmp(stage, "interpreter") == 0)
                options.disable_interpreter = true;
            else {
                if (!stage)
                    lstf_report_error(NULL, "missing argument to `%s`", option);
                else
                    lstf_report_error(NULL, "option `%s` must be set to one of"
                            " `resolver`, `analyzer`, `codegen` or `interpreter`", option);
                return 1;
            }
        } else if (strcmp(option, "-no-lsp") == 0) {
            options.no_lsp = true;
        } else if (strcmp(option, "-emit-ir") == 0) {
            options.emit_ir = true;
        } else if (strncmp(option, "-a", sizeof "-a" - 1) == 0) {
            // TODO: assembly
            is_assembling = true;
            lstf_report_error(NULL, "assembler not implemented");
            return 1;
        } else if (strcmp(option, "-c") == 0) {
            options.output_codegen = true;
            is_compiling = true;
        } else if (strcmp(option, "-d") == 0) {
            options.disassemble = true;
        } else if (strcmp(option, "-break") == 0) {
            if (!*(argp + 1)) {
                lstf_report_error(NULL, "argument required for `%s'", option);
                break;
            }
            if (!options.breakpoints) {
                options.breakpoints = array_new();
            }
            array_add(options.breakpoints, (ptrdiff_t) strtoll(*(argp + 1), NULL, 16));
            argp++;
        } else if (strncmp(option, "-o", sizeof "-o" - 1) == 0) {
            if (option[2] || *(argp + 1)) {
                is_output_arg = true;
            } else {
                lstf_report_error(NULL, "missing argument to `%s`", option);
                return 1;
            }
        } else if (strncmp(option, "-expect", sizeof "-expect" - 1) == 0) {
            if (options.expected_output) {
                lstf_report_error(NULL, "`%s` specified multiple times", option);
                return 1;
            }

            char *eqc = strchr(option, '=');
            char *expected_output = NULL;

            if (eqc) {
                *eqc = '\0';
                expected_output = eqc + 1;
            } else if (*++argp) {
                expected_output = *argp;
            }

            if (!expected_output) {
                lstf_report_error(NULL, "missing argument to `%s'", option);
                return 1;
            }

            options.expected_output = expected_output;
        } else if (strcmp(option, "-e") == 0) {
            if (!*(argp + 1)) {
                lstf_report_error(NULL, "argument required for `%s'", option);
                return 1;
            }
            char *varname = *++argp;
            char *eqc = strchr(varname, '=');
            char *value = NULL;

            if (eqc) {
                *eqc = '\0';
                value = eqc + 1;
            } else if (*++argp) {
                value = *argp;
            }

            if (!value) {
                lstf_report_error(NULL, "%s: a value is required for `%s'", option, varname);
                return 1;
            }

            if (!options.variables) {
                // NOTE: this means we have to be careful about modifying the
                // strings in argv[]
                options.variables = ptr_hashmap_new(
                        (collection_item_hash_func)strhash, NULL, NULL,
                        (collection_item_equality_func)strequal, NULL, NULL);
            }

            ptr_hashmap_insert(options.variables, varname, value);
        } else if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0) {
            break;
        } else if (strcmp(option, "-v") == 0 || strcmp(option, "--version") == 0) {
            printf("LSTF version %s\n", LSTF_VERSION);
            return 1;
        } else if (option[0] == '-') {
            lstf_report_error(NULL, "unrecognized command line option `%s`", option);
            break;
        } else if (*argp) {
            is_input_arg = true;
        }

        if (is_assembling + options.output_codegen + options.disassemble > 1) {
            lstf_report_error(NULL, "only one of -a, -c, -d may be used");
            return 1;
        }

        if (is_input_arg || is_output_arg) {
            char *filename = NULL;

            if (is_output_arg) {
                if (option[2] == '=')
                    filename = &option[3];
                else if (option[2])
                    filename = &option[2];
                else if (*(argp + 1))
                    filename = *++argp;
            } else {
                filename = *argp;
            }

            if (!filename) {
                lstf_report_error(NULL, "filename required");
                return 1;
            } else if (is_input_arg) {
                // check inputs
                const char *suffix_ptr = suffix(filename);

                if (!suffix_ptr) {
                    lstf_report_error(NULL, "%s: filename must have a suffix", filename);
                    return 1;
                } else if (is_assembling) {
                    if (strcmp(suffix_ptr, "lstfa") != 0) {
                        lstf_report_error(NULL, "%s: filename must be LSTF assembly (.lstfa)", filename);
                        return 1;
                    }
                } else if (options.output_codegen) {
                    if (strcmp(suffix_ptr, "lstf") != 0) {
                        lstf_report_error(NULL, "%s: filename must be LSTF (.lstf)", filename);
                        return 1;
                    }
                } else if (options.disassemble) {
                    if (strcmp(suffix_ptr, "lstf") == 0) {
                        is_compiling = true;
                    } else if (strcmp(suffix_ptr, "lstfc") != 0) {
                        lstf_report_error(NULL, "%s: filename must be LSTF or LSTF bytecode (.lstf/.lstfc)", filename);
                        return 1;
                    }
                } else if (strcmp(suffix_ptr, "lstfc") == 0) {
                    is_interpreting = true;
                } else if (strcmp(suffix_ptr, "lstf") == 0) {
                    is_compiling = true;
                }
            } else if (is_output_arg && options.output_codegen) {
                // check inputs
                const char *suffix_ptr = suffix(filename);

                if (strcmp(filename, "-") == 0) {
                    if (is_ascii_terminal(stdout)) {
                        lstf_report_error(NULL, "refusing to write bytecode to terminal");
                        return 1;
                    }
                } else if (!suffix_ptr) {
                    lstf_report_error(NULL, "%s: filename must have a suffix", filename);
                    return 1;
                } else if (strcmp(suffix_ptr, "lstfc") != 0) {
                    lstf_report_error(NULL, "%s: filename must be LSTF bytecode (.lstfc)", filename);
                    return 1;
                }
            }

            if (is_input_arg) {
                options.input_filename = filename;
                is_input_arg = false;
            } else if (is_output_arg) {
                options.output_filename = filename;
                is_output_arg = false;
            }
        }
    }

    if (is_compiling) {
        retval = compile_lstf_script(argv[0], options);
    } else if (is_interpreting) {
        retval = load_and_run_file(argv[0], options);
    } else if (options.disassemble) {
        retval = load_and_disassemble_file(argv[0], options);
    } else {
        print_usage(argv[0]);
        retval = 1;
    }

    if (options.breakpoints)
        array_destroy(options.breakpoints);
    if (options.variables)
        ptr_hashmap_destroy(options.variables);

    return retval;
}
