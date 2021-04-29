#include "lstf-integertype.h"
#include "lstf-variable.h"
#include "lstf-parser.h"
#include "lstf-function.h"
#include "lstf-stringtype.h"
#include "lstf-declaration.h"
#include "lstf-arraytype.h"
#include "lstf-anytype.h"
#include "lstf-voidtype.h"
#include "lstf-language-builtins.h"
#include "vm/lstf-vm-opcodes.h"

void lstf_parser_create_builtins(lstf_parser *parser)
{
    // create built-in variables and functions
    lstf_sourceref src = lstf_sourceref_default_from_file(parser->file);
    lstf_symbol *server_path = lstf_variable_new(&src, 
            "server_path", lstf_stringtype_new(&src), NULL, true);
    lstf_function_add_statement(parser->file->main_function,
            lstf_declaration_new_from_variable(&src, lstf_variable_cast(server_path)));

    lstf_symbol *project_files = lstf_variable_new(&src,
            "project_files", lstf_arraytype_new(&src, lstf_stringtype_new(&src)), NULL, true);
    lstf_function_add_statement(parser->file->main_function,
            lstf_declaration_new_from_variable(&src, lstf_variable_cast(project_files)));

    lstf_function *diagnostics = (lstf_function *)
        lstf_function_new_for_opcode(&src,
                "diagnostics",
                lstf_anytype_new(&src),
                false,
                lstf_vm_op_vmcall,
                lstf_vm_vmcall_diagnostics);
    lstf_variable *diagnostics_args[] = {
        (lstf_variable *)lstf_variable_new(&src, "file", lstf_stringtype_new(&src), NULL, true),
    };

    for (unsigned i = 0; i < sizeof(diagnostics_args) / sizeof(diagnostics_args[0]); i++)
        lstf_function_add_parameter(diagnostics, diagnostics_args[i]);
    lstf_function_add_statement(parser->file->main_function,
            lstf_declaration_new_from_function(&src, diagnostics));

    lstf_function *print_fn = (lstf_function *)
        lstf_function_new_for_opcode(&src, "print", lstf_voidtype_new(&src), false, lstf_vm_op_print, 0);
    lstf_variable *print_fn_args[] = {
        (lstf_variable *)lstf_variable_new(&src, "args", lstf_anytype_new(&src), NULL, true)
    };
    for (unsigned i = 0; i < sizeof(print_fn_args) / sizeof(print_fn_args[0]); i++)
        lstf_function_add_parameter(print_fn, print_fn_args[i]);
    lstf_function_add_statement(parser->file->main_function,
            lstf_declaration_new_from_function(&src, print_fn));
}
