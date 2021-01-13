#pragma once

#include "lstf-vm-program.h"
#include <stddef.h>

#define LSTFC_MAGIC_HEADER_BYTE0 '\x89'
#define LSTFC_MAGIC_HEADER_BYTE1 'L'
#define LSTFC_MAGIC_HEADER_BYTE2 'S'
#define LSTFC_MAGIC_HEADER_BYTE3 'T'
#define LSTFC_MAGIC_HEADER_BYTE4 'F'
#define LSTFC_MAGIC_HEADER_BYTE5 '\x01'
#define LSTFC_MAGIC_HEADER_BYTE6 '\x0A'
#define LSTFC_MAGIC_HEADER_BYTE7 '\x00'

#define LSTFC_MAGIC_HEADER (char[]){\
    LSTFC_MAGIC_HEADER_BYTE0, LSTFC_MAGIC_HEADER_BYTE1, LSTFC_MAGIC_HEADER_BYTE2, LSTFC_MAGIC_HEADER_BYTE3,\
    LSTFC_MAGIC_HEADER_BYTE4, LSTFC_MAGIC_HEADER_BYTE5, LSTFC_MAGIC_HEADER_BYTE6, LSTFC_MAGIC_HEADER_BYTE7,\
}

enum _lstf_vm_loader_error {
    lstf_vm_loader_error_none,

    /**
     * There was a system error reading the file.
     */
    lstf_vm_loader_error_read,

    /**
     * One or more of the section sizes is inconsistent with the size of the
     * program binary and sections.
     */
    lstf_vm_loader_error_invalid_section_size,

    lstf_vm_loader_error_invalid_magic_value,

    /**
     * A section name was encountered that's longer than the maximum length of
     * 128 bytes (including trailing NUL byte).
     */
    lstf_vm_loader_error_too_long_section_name,

    /**
     * A section name is not valid.
     */
    lstf_vm_loader_error_invalid_section_name,

    /**
     * The size for the `debug_info` section was too large.
     */
    lstf_vm_loader_error_invalid_debug_size,

    /**
     * Source filename was too long (or didn't have a NUL terminator).
     */
    lstf_vm_loader_error_source_filename_too_long,

    /**
     * A section defined in the program header had length 0.
     */
    lstf_vm_loader_error_zero_section_size,

    /**
     * The `code` section does not exist.
     */
    lstf_vm_loader_error_no_code_section,

    /**
     * Reached memory limits while allocating space for the program.
     */
    lstf_vm_loader_error_out_of_memory,
    
    /**
     * Debugging information has been corrupted. Reasons:
     *
     * - a debug symbol name is not NUL-terminated
     */
    lstf_vm_loader_error_invalid_debug_info,

    /**
     * The entry point is at an invalid location (offset within `code` section)
     */
    lstf_vm_loader_error_invalid_entry_point
};
typedef enum _lstf_vm_loader_error lstf_vm_loader_error;

lstf_vm_program *lstf_vm_loader_load_from_path(const char           *path,
                                               lstf_vm_loader_error *error);

lstf_vm_program *lstf_vm_loader_load_from_buffer(char                 *buffer,
                                                 size_t                buffer_size,
                                                 lstf_vm_loader_error *error);
