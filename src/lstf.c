#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "compiler/lstf-parser.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-symbolresolver.h"
#include "compiler/lstf-semanticanalyzer.h"
#include "vm/lstf-vm-loader.h"
#include "vm/lstf-vm-program.h"

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

static void report_load_error(const char          *progname,
                              const char          *filename,
                              lstf_vm_loader_error error)
{
    (void) progname;
    switch (error) {
    case lstf_vm_loader_error_read:
        lstf_report_error(NULL, "%s: VM loader: read error (%s)\n", filename, strerror(errno));
        break;
    case lstf_vm_loader_error_invalid_debug_info:
        lstf_report_error(NULL, "%s: VM loader: invalid debug info\n", filename);
        break;
    case lstf_vm_loader_error_invalid_debug_size:
        lstf_report_error(NULL, "%s: VM loader: invalid debug size\n", filename);
        break;
    case lstf_vm_loader_error_invalid_magic_value:
        lstf_report_error(NULL, "%s: VM loader: invalid magic value\n", filename);
        break;
    case lstf_vm_loader_error_invalid_section_name:
        lstf_report_error(NULL, "%s: VM loader: invalid section name\n", filename);
        break;
    case lstf_vm_loader_error_no_code_section:
        lstf_report_error(NULL, "%s: VM loader: no code section\n", filename);
        break;
    case lstf_vm_loader_error_out_of_memory:
        lstf_report_error(NULL, "%s: VM loader: out of memory\n", filename);
        break;
    case lstf_vm_loader_error_source_filename_too_long:
        lstf_report_error(NULL, "%s: VM loader: source filename too long\n", filename);
        break;
    case lstf_vm_loader_error_too_long_section_name:
        lstf_report_error(NULL, "%s: VM loader: section name too long\n", filename);
        break;
    case lstf_vm_loader_error_zero_section_size:
        lstf_report_error(NULL, "%s: VM loader: section size was zero\n", filename);
        break;
    case lstf_vm_loader_error_invalid_entry_point:
        lstf_report_error(NULL, "%s: VM loader: invalid entry point\n", filename);
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

    lstf_vm_program_unref(program);
    return 0;
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
    } else if (strcmp(argv[1], "-d") == 0) {
        // TODO: disassemble
    } else {
        char *suffix_ptr = strchr(argv[1], '.');

        if (suffix_ptr)
            suffix_ptr++;

        if (!suffix_ptr || !*suffix_ptr) {
            lstf_report_error(NULL, "%s: filename must have a suffix\n", argv[0]);
            return 1;
        }

        if (strcmp(suffix_ptr, "lstf") == 0) {
            if (argc > 2) {
                lstf_report_error(NULL, "%s: extra arguments\n", argv[0]);
                return 1;
            }
            return compile_lstf_script(argv[0], argv[1]);
        } else if (strcmp(suffix_ptr, "lstfa") == 0) {
            // TODO: assemble
        } else if (strcmp(suffix_ptr, "lstfc") == 0) {
            return load_and_run_file(argv[0], argv[1]);
        } else {
            lstf_report_error(NULL, "%s: filename must end with one of '.lstf', '.lstfa', '.lstfc'\n", argv[1]);
            return 1;
        }
    }
}
