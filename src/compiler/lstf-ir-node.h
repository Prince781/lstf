#pragma once

#include <limits.h>
#include <stdbool.h>

enum _lstf_ir_node_type {
    lstf_ir_node_type_instruction,
    lstf_ir_node_type_basicblock,
    lstf_ir_node_type_function,
    lstf_ir_node_type_program
};
typedef enum _lstf_ir_node_type lstf_ir_node_type;

typedef struct _lstf_ir_node lstf_ir_node;
struct _lstf_ir_node {
    lstf_ir_node_type node_type;
    unsigned refcount : sizeof(unsigned)*CHAR_BIT - 1;
    bool floating : 1;
    void (*destructor)(lstf_ir_node *);
};

void *lstf_ir_node_ref(void *node);

void lstf_ir_node_unref(void *node);

void lstf_ir_node_construct(lstf_ir_node     *node,
                            void            (*destructor)(lstf_ir_node *),
                            lstf_ir_node_type node_type);
