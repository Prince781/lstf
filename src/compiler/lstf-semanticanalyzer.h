#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include <limits.h>
#include <stdbool.h>

struct _lstf_semanticanalyzer {
    lstf_codevisitor parent_struct;
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;

    unsigned num_errors;

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
     * Stack of expected return types
     */
    ptr_list *expected_return_types;

    /**
     * Stack of current functions
     */
    ptr_list *current_functions;

    /**
     * Stack of current pattern tests `(lstf_patterntest *)`
     */
    ptr_list *current_pattern_tests;

    /**
     * Whether ellipsis is allowed in the current context
     */
    bool ellipsis_allowed;

    bool encountered_server_path_assignment;
    bool encountered_project_files_assignment;
};
typedef struct _lstf_semanticanalyzer lstf_semanticanalyzer;

lstf_semanticanalyzer *lstf_semanticanalyzer_new(lstf_file *file);

lstf_semanticanalyzer *lstf_semanticanalyzer_ref(lstf_semanticanalyzer *analyzer);

void lstf_semanticanalyzer_unref(lstf_semanticanalyzer *analyzer);

void lstf_semanticanalyzer_analyze(lstf_semanticanalyzer *analyzer);
