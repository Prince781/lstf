#include "compiler/lstf-codegenerator.h"
#include "compiler/lstf-ir-program.h"
#include "compiler/lstf-parser.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-symbolresolver.h"
#include "compiler/lstf-semanticanalyzer.h"
#include "data-structures/string-builder.h"
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
"usage: %s script.lstf\n"
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
"\n"
"Flags:\n"
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
    while (lstf_virtualmachine_run(vm)) {
        fprintf(stderr, "VM paused. press any key to continue...\n");
        getchar();
    }
    if (vm->last_status != lstf_vm_status_exited) {
        outputstream *os = outputstream_new_from_file(stderr, false);
        lstf_report_error(NULL, "VM: %s", lstf_vm_status_to_string(vm->last_status));
        lstf_vm_program_disassemble(vm->program, os, vm->last_pc);
        outputstream_unref(os);
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
    } else if (!lstf_vm_program_disassemble(program, os, NULL)) {
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

    return retval;
}
