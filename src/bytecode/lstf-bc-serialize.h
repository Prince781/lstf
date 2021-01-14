#pragma once

#include "io/inputstream.h"
#include "io/outputstream.h"
#include "lstf-bc-program.h"
#include <stdbool.h>

/**
 * Writes LSTF bytecode to the output stream.
 *
 * `program` must contain a `main` function
 *
 * @return `true` on success, `false` on write failure
 *
 * @see lstf_vm_loader_load_from_buffer
 * @see lstf_vm_loader_load_from_path
 */
bool lstf_bc_program_serialize_to_binary(lstf_bc_program *program, outputstream *ostream);
