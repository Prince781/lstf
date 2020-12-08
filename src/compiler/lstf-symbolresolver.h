#include "lstf-datatype.h"
#include "data-structures/ptr-list.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"

struct _lstf_symbolresolver {
    lstf_codevisitor parent_struct;
    lstf_file *file;
    ptr_list *scopes;
    lstf_datatype *expected_element_type;
    unsigned num_errors;
};
typedef struct _lstf_symbolresolver lstf_symbolresolver;

lstf_symbolresolver *lstf_symbolresolver_new(lstf_file *file);

void lstf_symbolresolver_resolve(lstf_symbolresolver *resolver);

void lstf_symbolresolver_destroy(lstf_symbolresolver *resolver);
