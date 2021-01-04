#include "lstf-vm-loader.h"
#include "lstf-vm-program.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "io/inputstream.h"
#include "lstf-vm-debug.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

// for ntoh*()
#if (WIN32 || WIN64)
#include <winsock.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>

// ntohll not defined on non-Windows systems

uint64_t ntohll(uint64_t netinteger) {
    if (((unsigned char *)&netinteger)[0] != (netinteger >> 56)) {
        // host is little-endian
        uint64_t hostinteger = 0;

        for (unsigned i = 0; i < sizeof netinteger; i++)
            hostinteger |= ((netinteger >> (sizeof netinteger - 1 - i)*CHAR_BIT) & 0xFF) << i*CHAR_BIT;

        return hostinteger;
    }
    return netinteger;
}
#endif

static lstf_vm_program *lstf_vm_program_create(void)
{
    lstf_vm_program *program = calloc(1, sizeof *program);

    if (!program)
        return NULL;

    program->floating = true;
    program->debug_entries = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);
    if (!program->debug_entries) {
        free(program);
        return NULL;
    }
    program->debug_symbols = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);
    if (!program->debug_symbols) {
        ptr_hashmap_destroy(program->debug_entries);
        free(program);
        return NULL;
    }

    return program;
}

static lstf_vm_program *lstf_vm_loader_load_from_stream(inputstream *istream, lstf_vm_loader_error *error)
{
    const char magic[] = {'\x89', 'L', 'S', 'T', 'F', '\x01', '\x0A', '\x00'};
    lstf_vm_program *program = NULL;
    uint64_t entry_point_offset = 0;
    char byte = 0;
    ptr_list *debug_entries = NULL;
    ptr_list *debug_symbols = NULL;

    if (error)
        *error = lstf_vm_loader_error_none;

    // load magic header
    for (unsigned i = 0; i < sizeof magic; i++) {
        if (!inputstream_has_data(istream)) {
            if (error)
                *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        } else if ((byte = inputstream_read_char(istream)) != magic[i]) {
            if (error)
                *error = lstf_vm_loader_error_invalid_magic_value;
            goto error_cleanup;
        }
    }

    // load entry point offset
    if (!inputstream_read_uint64(istream, &entry_point_offset)) {
        *error = lstf_vm_loader_error_read;
        goto error_cleanup;
    }

    // read the rest of the program header
    if (!inputstream_has_data(istream)) {
        // a list of sections was not found
        *error = lstf_vm_loader_error_read;
        goto error_cleanup;
    }
    program = lstf_vm_program_create();

    while (inputstream_has_data(istream) &&
            (byte = inputstream_read_char(istream)) != '\0') {
        char section_name[128];
        unsigned sn_length = 0;
        uint64_t section_size = 0;

        section_name[sn_length++] = byte;

        // read section name until we encounter a NUL terminator
        while (inputstream_has_data(istream)) {
            byte = inputstream_read_char(istream);
            if (sn_length >= sizeof section_name) {
                *error = lstf_vm_loader_error_too_long_section_name;
                goto error_cleanup;
            }

            section_name[sn_length++] = byte;
            if (byte == '\0')
                break;
        }
        if (byte != '\0') {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }

        // read section length
        if (!inputstream_read_uint64(istream, &section_size)) {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }

        if (section_size == 0) {
            *error = lstf_vm_loader_error_zero_section_size;
            goto error_cleanup;
        }

        if (strcmp(section_name, "debug_info") == 0) {
            program->debug_size = section_size;
        } else if (strcmp(section_name, "data") == 0) {
            program->data_size = section_size;
        } else if (strcmp(section_name, "code") == 0) {
            program->code_size = section_size;
            if (entry_point_offset >= program->code_size) {
                *error = lstf_vm_loader_error_invalid_entry_point;
                goto error_cleanup;
            }
        } else {
            *error = lstf_vm_loader_error_invalid_section_name;
            goto error_cleanup;
        }
    }
    if (byte != '\0') {
        *error = lstf_vm_loader_error_read;
        goto error_cleanup;
    }

    // parse optional debug section
    if (program->debug_size > 0) {
        program->debug = calloc(program->debug_size, sizeof *program->debug);

        if (!program->debug) {
            *error = lstf_vm_loader_error_out_of_memory;
            goto error_cleanup;
        }

        if (!inputstream_read(istream, program->debug, program->debug_size)) {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }

        // read source filename
        program->source_filename = (char *) program->debug;
        char *nb_ptr = memchr(program->debug, '\0', program->debug_size);
        if (!nb_ptr || nb_ptr - (char *)program->debug > FILENAME_MAX) {
            *error = lstf_vm_loader_error_source_filename_too_long;
            goto error_cleanup;
        }

        uint64_t n_debug_entries = 0;
        if (!inputstream_read_uint64(istream, &n_debug_entries)) {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }

        debug_entries = ptr_list_new(NULL, NULL);

        uint8_t *offset = program->debug;
        while (n_debug_entries > 0 && (uint64_t)(offset - program->debug) < program->debug_size) {
            lstf_vm_debugentry *entry = (lstf_vm_debugentry *)offset;

            ptr_list_append(debug_entries, entry);

            offset += sizeof *entry;
            n_debug_entries--;
        }
        
        if (n_debug_entries > 0) {
            // debug entries remain
            *error = lstf_vm_loader_error_invalid_debug_size;
            goto error_cleanup;
        }

        // parse debug symbols
        uint64_t n_debug_symbols = 0;
        if (!inputstream_read_uint64(istream, &n_debug_symbols)) {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }

        debug_symbols = ptr_list_new(NULL, NULL);

        offset += sizeof n_debug_symbols;
        while (n_debug_symbols > 0 && (uint64_t)(offset - program->debug) < program->debug_size) {
            lstf_vm_debugsym *symbol = (lstf_vm_debugsym *)offset;
            nb_ptr = memchr(offset, '\0', program->debug_size - (offset - program->debug));

            if (!nb_ptr) {
                *error = lstf_vm_loader_error_invalid_debug_info;
                goto error_cleanup;
            }

            ptr_list_append(debug_symbols, symbol);

            offset += sizeof *symbol;
            n_debug_symbols--;
        }

        if (n_debug_symbols > 0) {
            // debug symbols remain
            *error = lstf_vm_loader_error_invalid_debug_size;
            goto error_cleanup;
        }
    }

    // now load the data section (if it exists)
    if (program->data_size > 0) {
        program->data = calloc(program->data_size, sizeof *program->data);

        if (!program->data) {
            *error = lstf_vm_loader_error_out_of_memory;
            goto error_cleanup;
        }

        if (!inputstream_read(istream, program->data, program->data_size)) {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }

        program->entry_point = program->data + entry_point_offset;
    }

    // now load the code section, which is mandatory
    if (program->code_size > 0) {
        program->code = calloc(program->code_size, sizeof *program->code);

        if (!program->code) {
            *error = lstf_vm_loader_error_out_of_memory;
            goto error_cleanup;
        }

        if (!inputstream_read(istream, program->code, program->code_size)) {
            *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }
    } else {
        *error = lstf_vm_loader_error_no_code_section;
        goto error_cleanup;
    }

    // now write the debug entries
    for (iterator it = ptr_list_iterator_create(debug_entries);
            it.has_next; it = iterator_next(it)) {
        lstf_vm_debugentry *entry = iterator_get_item(it);

        entry->instruction_offset = ntohll(entry->instruction_offset);
        entry->source_column = ntohl(entry->source_column);
        entry->source_line = ntohl(entry->source_line);

        ptr_hashmap_insert(program->debug_entries,
                program->code + entry->instruction_offset, entry);
    }

    // now write the debug symbols
    for (iterator it = ptr_list_iterator_create(debug_symbols);
            it.has_next; it = iterator_next(it)) {
        lstf_vm_debugsym *symbol = iterator_get_item(it);

        symbol->instruction_offset = ntohll(symbol->instruction_offset);

        ptr_hashmap_insert(program->debug_symbols,
                program->code + symbol->instruction_offset, symbol);
    }

    ptr_list_destroy(debug_entries);
    ptr_list_destroy(debug_symbols);
    inputstream_unref(istream);
    return program;

//  -------------------------------------------
error_cleanup:
    ptr_list_destroy(debug_entries);
    ptr_list_destroy(debug_symbols);
    inputstream_unref(istream);
    if (program) {
        lstf_vm_program_unref(program);
        program = NULL;
    }
    return NULL;
}

lstf_vm_program *lstf_vm_loader_load_from_path(const char *path, lstf_vm_loader_error *error)
{
    return lstf_vm_loader_load_from_stream(inputstream_new_from_path(path, "rb"), error);
}

lstf_vm_program *lstf_vm_loader_load_from_buffer(char                 *buffer,
                                                 size_t                buffer_size,
                                                 lstf_vm_loader_error *error)
{
    return lstf_vm_loader_load_from_stream(inputstream_new_from_buffer(buffer, buffer_size, true), error);
}
