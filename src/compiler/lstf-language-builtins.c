#include "lstf-constant.h"
#include "lstf-enum.h"
#include "lstf-enumtype.h"
#include "lstf-futuretype.h"
#include "lstf-interfacetype.h"
#include "lstf-literal.h"
#include "lstf-typealias.h"
#include "lstf-unresolvedtype.h"
#include "lstf-interface.h"
#include "lstf-integertype.h"
#include "lstf-variable.h"
#include "lstf-parser.h"
#include "lstf-function.h"
#include "lstf-stringtype.h"
#include "lstf-declaration.h"
#include "lstf-arraytype.h"
#include "lstf-anytype.h"
#include "lstf-voidtype.h"
#include "lstf-uniontype.h"
#include "lstf-language-builtins.h"
#include "vm/lstf-vm-opcodes.h"

void lstf_file_create_builtins(lstf_file *file)
{
    // create built-in variables, functions, and types

    // server_path: string
    lstf_sourceref src = lstf_sourceref_default_from_file(file);
    lstf_symbol *server_path = lstf_variable_new(&src, 
            "server_path", lstf_stringtype_new(&src), NULL, true);
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_variable(&src, lstf_variable_cast(server_path)));

    // fun option(key: string): string
    lstf_function *option_fn = (lstf_function *)
        lstf_function_new_for_opcode(&src, "option", lstf_stringtype_new(&src), false, lstf_vm_op_getopt, 0);
    lstf_function_add_parameter(option_fn, (lstf_variable *) lstf_variable_new(&src, "key", lstf_stringtype_new(&src), NULL, true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_function(&src, option_fn));

    // type DocumentUri = string
    lstf_typealias *docuri_alias = (lstf_typealias *) lstf_typealias_new(&src, "DocumentUri", lstf_stringtype_new(&src), true);
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_type_alias(&src, docuri_alias));

    // fun file(type: string, contents: string): DocumentUri
    lstf_function *file_fn = (lstf_function *)
        lstf_function_new_for_opcode(&src, "file", docuri_alias->aliased_type, false, 
                lstf_vm_op_vmcall, lstf_vm_vmcall_memory);
    lstf_function_add_parameter(file_fn, (lstf_variable *)
            lstf_variable_new(&src, "type", lstf_stringtype_new(&src), NULL, true));
    lstf_function_add_parameter(file_fn, (lstf_variable *)
            lstf_variable_new(&src, "contents", lstf_stringtype_new(&src), NULL, true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_function(&src, file_fn));

    // interface Position { line: int; character: int; }
    lstf_interface *position_iface = lstf_interface_new(&src, "Position", false, true);
    lstf_interface_add_member(position_iface, lstf_interfaceproperty_new(&src, "line", false, lstf_integertype_new(&src), true));
    lstf_interface_add_member(position_iface, lstf_interfaceproperty_new(&src, "character", false, lstf_integertype_new(&src), true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_interface(&src, position_iface));

    // interface Range {
    //  start: Position;
    //  end: Position;
    // }
    lstf_interface *range_iface = lstf_interface_new(&src, "Range", false, true);
    lstf_interface_add_member(range_iface, lstf_interfaceproperty_new(&src, "start", false, lstf_interfacetype_new(&src, position_iface), true));
    lstf_interface_add_member(range_iface, lstf_interfaceproperty_new(&src, "end", false, lstf_interfacetype_new(&src, position_iface), true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_interface(&src, range_iface));

    // enum DiagnosticSeverity {
    //  Error = 1,
    //  Warning = 2,
    //  Information = 3,
    //  Hint = 4
    // }
    lstf_enum *diagseverity_enum = lstf_enum_new(&src, "DiagnosticSeverity", true);
    lstf_enum_add_member(diagseverity_enum, (lstf_constant *)
            lstf_constant_new(&src, "Error", (lstf_expression *)
                    lstf_literal_new(&src, lstf_literal_type_integer, (lstf_literal_value){ .integer_value = 1 })));
    lstf_enum_add_member(diagseverity_enum, (lstf_constant *)
            lstf_constant_new(&src, "Warning", (lstf_expression *)
                    lstf_literal_new(&src, lstf_literal_type_integer, (lstf_literal_value){ .integer_value = 2 })));
    lstf_enum_add_member(diagseverity_enum, (lstf_constant *)
            lstf_constant_new(&src, "Information", (lstf_expression *)
                    lstf_literal_new(&src, lstf_literal_type_integer, (lstf_literal_value){ .integer_value = 3 })));
    lstf_enum_add_member(diagseverity_enum, (lstf_constant *)
            lstf_constant_new(&src, "Hint", (lstf_expression *)
                    lstf_literal_new(&src, lstf_literal_type_integer, (lstf_literal_value){ .integer_value = 4 })));
    lstf_function_add_statement(file->main_function, lstf_declaration_new_from_enum(&src, diagseverity_enum));

    // interface Diagnostic {
    //  range: Range;
    //  severity?: DiagnosticSeverity;
    //  message: string;
    // }
    lstf_interface *diagnostic_iface = lstf_interface_new(&src, "Diagnostic", false, true);
    lstf_interface_add_member(diagnostic_iface, lstf_interfaceproperty_new(&src, "range", false, lstf_interfacetype_new(&src, range_iface), true));
    lstf_interface_add_member(diagnostic_iface, lstf_interfaceproperty_new(&src, "severity", true, lstf_enumtype_new(&src, diagseverity_enum), true));
    lstf_interface_add_member(diagnostic_iface, lstf_interfaceproperty_new(&src, "message", false, lstf_stringtype_new(&src), true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_interface(&src, diagnostic_iface));

    // interface PublishDiagnosticsParams {
    //  uri: string;
    //  version?: int;
    //  diagnostics: Diagnostic[];
    // }
    lstf_interface *publishdiagsparams_iface = lstf_interface_new(&src, "PublishDiagnosticsParams", false, true);
    lstf_interface_add_member(publishdiagsparams_iface, lstf_interfaceproperty_new(&src, "uri", false, lstf_stringtype_new(&src), true));
    lstf_interface_add_member(publishdiagsparams_iface, lstf_interfaceproperty_new(&src, "version", true, lstf_integertype_new(&src), true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_interface(&src, publishdiagsparams_iface));

    // fun memory(contents: string): DocumentUri
    lstf_function *memory_fn = (lstf_function *)
        lstf_function_new_for_opcode(&src,
                "memory",
                lstf_unresolvedtype_new(&src, "DocumentUri"),
                false,
                lstf_vm_op_vmcall,
                lstf_vm_vmcall_memory);
    lstf_function_add_parameter(memory_fn, (lstf_variable *) lstf_variable_new(&src, "contents", lstf_stringtype_new(&src), NULL, true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_function(&src, memory_fn));

    // project_files: DocumentUri[]
    lstf_symbol *project_files = lstf_variable_new(&src,
            "project_files", lstf_arraytype_new(&src,
                    lstf_unresolvedtype_new(&src, "DocumentUri")), NULL, true);
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_variable(&src, lstf_variable_cast(project_files)));

    // async fun diagnostics(file: DocumentUri): future<PublishDiagnosticsParams>
    lstf_function *diagnostics = (lstf_function *)
        lstf_function_new_for_opcode(&src,
                "diagnostics",
                lstf_futuretype_new(&src, lstf_interfacetype_new(&src, publishdiagsparams_iface)),
                true,
                lstf_vm_op_vmcall,
                lstf_vm_vmcall_diagnostics);
    lstf_function_add_parameter(diagnostics, (lstf_variable *)
            lstf_variable_new(&src, "file",
                lstf_unresolvedtype_new(&src, "DocumentUri"),
                NULL, true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_function(&src, diagnostics));

    // print(args: any)
    lstf_function *print_fn = (lstf_function *)
        lstf_function_new_for_opcode(&src, "print", lstf_voidtype_new(&src), false, lstf_vm_op_print, 0);
    lstf_function_add_parameter(print_fn, (lstf_variable *)
            lstf_variable_new(&src, "args", lstf_anytype_new(&src), NULL, true));
    lstf_function_add_statement(file->main_function,
            lstf_declaration_new_from_function(&src, print_fn));
}
