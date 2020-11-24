#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include <stdbool.h>
#include <stdint.h>

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

lstf_expression *lstf_literal_new(const lstf_sourceref *source_reference,
                                  lstf_literal_type     literal_type,
                                  lstf_literal_value    literal_value);
