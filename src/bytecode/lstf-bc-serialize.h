#pragma once

#include "io/outputstream.h"
#include "lstf-bc-program.h"
#include <stdbool.h>

bool lstf_bc_program_serialize_to_binary(lstf_bc_program *program, outputstream *ostream);

bool lstf_bc_program_serialize_to_text(lstf_bc_program *program, outputstream *ostream);
