#pragma once

#include "io/outputstream.h"
#include "lstf-bc-program.h"
#include <stdbool.h>

/**
 * Writes LSTF bytecode to the output stream.
 *
 * `program` must contain a `main` function
 *
 * @return `true` on success, `false` on write failure
 */
bool lstf_bc_program_serialize_to_binary(lstf_bc_program *program, outputstream *ostream);

// TODO
// bool lstf_bc_program_serialize_to_text(lstf_bc_program *program, outputstream *ostream);
