#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum _lstf_literal_type {
    lstf_literal_type_null,
    lstf_literal_type_integer,
    lstf_literal_type_double,
    lstf_literal_type_boolean,
    lstf_literal_type_string
};
typedef enum _lstf_literal_type lstf_literal_type;

union _lstf_literal_value {
    int64_t integer_value;
    double  double_value;
    bool    boolean_value;
    char   *string_value;
};
typedef union _lstf_literal_value lstf_literal_value;

struct _lstf_literal {
    lstf_expression parent_struct;
    lstf_literal_type literal_type;
    lstf_literal_value value;
};
typedef struct _lstf_literal lstf_literal;

static inline lstf_literal *lstf_literal_cast(void *node)
{
    lstf_expression *expr = lstf_expression_cast(node);

    if (expr && expr->expr_type == lstf_expression_type_literal)
        return node;
    return NULL;
}

lstf_expression *lstf_literal_new(const lstf_sourceref *source_reference,
                                  lstf_literal_type     literal_type,
                                  lstf_literal_value    literal_value);

/**
 * returns true if this literal is the `null` constant/literal
 */
bool lstf_literal_is_null(const lstf_literal *lit);

int64_t lstf_literal_get_integer(const lstf_literal *lit);

double lstf_literal_get_double(const lstf_literal *lit);

bool lstf_literal_get_boolean(const lstf_literal *lit);

const char *lstf_literal_get_string(const lstf_literal *lit);
