#include "lstf-datatype.h"
#include "data-structures/ptr-list.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include <stdbool.h>
#include <limits.h>

struct _lstf_symbolresolver {
    lstf_codevisitor parent_struct;
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;
    unsigned num_errors;
    lstf_file *file;
    ptr_list *scopes;
    lstf_datatype *expected_element_type;
};
typedef struct _lstf_symbolresolver lstf_symbolresolver;

lstf_symbolresolver *lstf_symbolresolver_new(lstf_file *file);

lstf_symbolresolver *lstf_symbolresolver_ref(lstf_symbolresolver *resolver);

void lstf_symbolresolver_unref(lstf_symbolresolver *resolver);

void lstf_symbolresolver_resolve(lstf_symbolresolver *resolver);
