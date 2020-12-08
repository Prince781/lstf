#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include <stdbool.h>

struct _lstf_semanticanalyzer {
    lstf_codevisitor parent_struct;
    lstf_file *file;

    /**
     * Stack of current scopes
     */
    ptr_list *scopes;

    /**
     * Stack of expected expression types in the current context
     */
    ptr_list *expected_expression_types;

    /**
     * Whether ellipsis is allowed in the current context
     */
    bool ellipsis_allowed;

    bool encountered_server_path_assignment;
    bool encountered_project_files_assignment;

    unsigned num_errors;
};
typedef struct _lstf_semanticanalyzer lstf_semanticanalyzer;

lstf_semanticanalyzer *lstf_semanticanalyzer_new(lstf_file *file);

void lstf_semanticanalyzer_analyze(lstf_semanticanalyzer *analyzer);

void lstf_semanticanalyzer_destroy(lstf_semanticanalyzer *analyzer);
