#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "compiler/lstf-parser.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-symbolresolver.h"
#include "compiler/lstf-semanticanalyzer.h"
#include "vm/lstf-virtualmachine.h"
#include "vm/lstf-vm-loader.h"
#include "vm/lstf-vm-program.h"
#include "vm/lstf-vm-status.h"

static inline char *suffix(const char *str)
{
    char *ext = strchr(str, '.');

    return ext != NULL ? (*(ext + 1) ? ext + 1 : NULL) : NULL;
}

const char usage_message[] =
"usage: %s script.lstf\n"
"        runs a LSTF script\n"
"\n"
"usage: %s -C script.lstf [-o script.lstfa]\n"
"        compiles a LSTF script to assembly code\n"
"\n"
"usage: %s -a script.lstfa [-o script.lstfc]\n"
"        assembles LSTF assembly code to bytecode\n"
"\n"
"usage: %s -d script.lstfc [-o script.lstfa]\n"
"        disassembles LSTF bytecode to assembly code";

static void
print_usage(const char *progname)
{
    fprintf(stderr, usage_message, progname, progname, progname, progname);
    fprintf(stderr, "\n");
}

static int
compile_lstf_script(const char *progname, const char *filename)
{
    (void) progname;
    lstf_file *script = lstf_file_load(filename);
    if (!script) {
        lstf_report_error(NULL, "%s: %s", filename, strerror(errno));
        fprintf(stderr, "compilation terminated.\n");
        return 1;
    }

    lstf_parser *parser = lstf_parser_create(script);
    lstf_symbolresolver *resolver = lstf_symbolresolver_new(script);
    lstf_semanticanalyzer *analyzer = lstf_semanticanalyzer_new(script);
    int retval = 0;

    lstf_parser_parse(parser);
    if (parser->scanner->num_errors + parser->num_errors == 0) {
        lstf_symbolresolver_resolve(resolver);
        if (resolver->num_errors == 0) {
            lstf_semanticanalyzer_analyze(analyzer);
            if (analyzer->num_errors == 0) {
            } else {
                fprintf(stderr, "%u error(s) generated.\n", analyzer->num_errors);
                retval = 1;
            }
        } else {
            fprintf(stderr, "%u error(s) generated.\n", resolver->num_errors);
            retval = 1;
        }
    } else {
        fprintf(stderr, "%u error(s) generated.\n", parser->scanner->num_errors + parser->num_errors);
        retval = 1;
    }

    // TODO: code generation
    // TODO: run compiled program in VM

    lstf_parser_destroy(parser);
    lstf_symbolresolver_destroy(resolver);
    lstf_semanticanalyzer_destroy(analyzer);
    lstf_file_unload(script);
    return retval;
}

static int run_program(lstf_vm_program *program)
{
    lstf_virtualmachine *vm = lstf_virtualmachine_new(program, false);
    int return_code = 0;

    if (!lstf_virtualmachine_run(vm) &&
            vm->last_status && vm->last_status != lstf_vm_status_exited) {
        lstf_report_error(NULL, "VM hit an exception: %s", lstf_vm_status_to_string(vm->last_status));
        return_code = 1;
    } else {
        return_code = vm->return_code;
    }

    lstf_virtualmachine_destroy(vm);
    return return_code;
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

static int load_and_run_file(const char *progname, const char *filename)
{
    lstf_vm_loader_error error = lstf_vm_loader_error_none;
    lstf_vm_program *program = lstf_vm_loader_load_from_path(filename, &error);

    if (!program) {
        report_load_error(progname, filename, error);
        return 1;
    }

    return run_program(program);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-C") == 0) {
        // compile
        return compile_lstf_script(argv[0], argv[2]);
    } else if (strcmp(argv[1], "-a") == 0) {
        // TODO: assemble
        lstf_report_error(NULL, "assembler not implemented");
        return 1;
    } else if (strcmp(argv[1], "-d") == 0) {
        // TODO: disassemble
        lstf_report_error(NULL, "disassembly not implemented");
        return 1;
    } else {
        char *suffix_ptr = suffix(argv[1]);

        if (!suffix_ptr) {
            lstf_report_error(NULL, "%s: filename must have a suffix", argv[0]);
            return 1;
        }

        if (strcmp(suffix_ptr, "lstf") == 0) {
            if (argc > 2) {
                lstf_report_error(NULL, "%s: extra arguments", argv[0]);
                return 1;
            }
            return compile_lstf_script(argv[0], argv[1]);
        } else if (strcmp(suffix_ptr, "lstfa") == 0) {
            // TODO: assemble
            lstf_report_error(NULL, "assembler not implemented");
            return 1;
        } else if (strcmp(suffix_ptr, "lstfc") == 0) {
            return load_and_run_file(argv[0], argv[1]);
        } else {
            lstf_report_error(NULL, "%s: filename must end with one of '.lstf', '.lstfa', '.lstfc'", argv[1]);
            return 1;
        }
    }
}
