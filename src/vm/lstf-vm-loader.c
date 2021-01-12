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
#include <errno.h>
#include "util.h"

#if (_WIN32 || _WIN64)
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>

uint64_t ntohll(uint64_t netint) {
    if (((unsigned char *)&netint)[0] != (netint >> 56)) {
        uint64_t hostint = 0;

        // host byte order is little-endian, so we have to swap bytes in `netint`
        hostint = (hostint << CHAR_BIT) | ((netint >> (0*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (1*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (2*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (3*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (4*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (5*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (6*CHAR_BIT)) & 0xFF);
        hostint = (hostint << CHAR_BIT) | ((netint >> (7*CHAR_BIT)) & 0xFF);

        return hostint;
    }
    return netint;
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
    uint64_t comments_size = 0;

    if (error)
        *error = lstf_vm_loader_error_none;

    // load magic header
    for (unsigned i = 0; i < sizeof magic; i++) {
        if (!inputstream_has_data(istream)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        } else if ((byte = inputstream_read_char(istream)) != magic[i]) {
            if (error)
                *error = lstf_vm_loader_error_invalid_magic_value;
            goto error_cleanup;
        }
    }

    // load entry point offset
    if (!inputstream_read_uint64(istream, &entry_point_offset)) {
        if (error)
            *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
        goto error_cleanup;
    }

    // read the rest of the program header
    if (!inputstream_has_data(istream)) {
        // a list of sections was not found
        if (error)
            *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
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
                if (error)
                    *error = lstf_vm_loader_error_too_long_section_name;
                goto error_cleanup;
            }

            section_name[sn_length++] = byte;
            if (byte == '\0')
                break;
        }
        if (byte != '\0') {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }

        // read section length
        if (!inputstream_read_uint64(istream, &section_size)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }

        if (section_size == 0) {
            if (error)
                *error = lstf_vm_loader_error_zero_section_size;
            goto error_cleanup;
        }

        if (strcmp(section_name, "debug_info") == 0) {
            program->debug_size = section_size;
        } else if (strcmp(section_name, "comments") == 0) {
            comments_size = section_size;
        } else if (strcmp(section_name, "data") == 0) {
            program->data_size = section_size;
        } else if (strcmp(section_name, "code") == 0) {
            program->code_size = section_size;
            if (entry_point_offset >= program->code_size) {
                if (error)
                    *error = lstf_vm_loader_error_invalid_entry_point;
                goto error_cleanup;
            }
        } else {
            if (error)
                *error = lstf_vm_loader_error_invalid_section_name;
            goto error_cleanup;
        }
    }
    if (byte != '\0') {
        if (error)
            *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
        goto error_cleanup;
    }

    // parse optional debug section
    if (program->debug_size > 0) {
        program->debug = calloc(program->debug_size, sizeof *program->debug);

        if (!program->debug) {
            if (error)
                *error = lstf_vm_loader_error_out_of_memory;
            goto error_cleanup;
        }

        if (!inputstream_read(istream, program->debug, program->debug_size)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }

        // read source filename
        program->source_filename = (char *) program->debug;
        char *nb_ptr = memchr(program->debug, '\0', program->debug_size);
        if (!nb_ptr || nb_ptr - (char *)program->debug > FILENAME_MAX) {
            if (error)
                *error = lstf_vm_loader_error_source_filename_too_long;
            goto error_cleanup;
        }

        uint64_t n_debug_entries = 0;
        if (!inputstream_read_uint64(istream, &n_debug_entries)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }

        debug_entries = ptr_list_new(NULL, NULL);

        uint8_t *offset = program->debug;
        while (n_debug_entries > 0 && (uint64_t)(offset - program->debug) < program->debug_size) {
            lstf_vm_debugentry *entry = (lstf_vm_debugentry *)offset;

            // aliasing memory means we have to convert the byte order to host byte order
            entry->instruction_offset = ntohll(entry->instruction_offset);
            entry->source_column = ntohl(entry->source_column);
            entry->source_line = ntohl(entry->source_line);

            ptr_list_append(debug_entries, entry);

            offset += sizeof *entry;
            n_debug_entries--;
        }
        
        if (n_debug_entries > 0) {
            // debug entries remain
            if (error)
                *error = lstf_vm_loader_error_invalid_debug_size;
            goto error_cleanup;
        }

        // parse debug symbols
        uint64_t n_debug_symbols = 0;
        if (!inputstream_read_uint64(istream, &n_debug_symbols)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }

        debug_symbols = ptr_list_new(NULL, NULL);

        offset += sizeof n_debug_symbols;
        while (n_debug_symbols > 0 && (uint64_t)(offset - program->debug) < program->debug_size) {
            lstf_vm_debugsym *symbol = (lstf_vm_debugsym *)offset;
            nb_ptr = memchr(offset, '\0', program->debug_size - (offset - program->debug));

            if (!nb_ptr) {
                if (error)
                    *error = lstf_vm_loader_error_invalid_debug_info;
                goto error_cleanup;
            }

            // aliasing memory means we have to convert the byte order to host byte order
            symbol->instruction_offset = ntohll(symbol->instruction_offset);
            ptr_list_append(debug_symbols, symbol);

            offset += sizeof *symbol;
            n_debug_symbols--;
        }

        if (n_debug_symbols > 0) {
            // debug symbols remain
            if (error)
                *error = lstf_vm_loader_error_invalid_debug_size;
            goto error_cleanup;
        }
    }

    // skip over comments section
    if (comments_size > 0) {
        if (!inputstream_skip(istream, comments_size)) {
            if (error)
                *error = lstf_vm_loader_error_read;
            goto error_cleanup;
        }
    }

    // now load the data section (if it exists)
    if (program->data_size > 0) {
        program->data = calloc(program->data_size, sizeof *program->data);

        if (!program->data) {
            if (error)
                *error = lstf_vm_loader_error_out_of_memory;
            goto error_cleanup;
        }

        if (!inputstream_read(istream, program->data, program->data_size)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }
    }

    // now load the code section, which is mandatory
    if (program->code_size > 0) {
        program->code = calloc(program->code_size, sizeof *program->code);

        if (!program->code) {
            if (error)
                *error = lstf_vm_loader_error_out_of_memory;
            goto error_cleanup;
        }

        if (!inputstream_read(istream, program->code, program->code_size)) {
            if (error)
                *error = errno ? lstf_vm_loader_error_read : lstf_vm_loader_error_invalid_section_size;
            goto error_cleanup;
        }

        program->entry_point = program->code + entry_point_offset;
    } else {
        if (error)
            *error = lstf_vm_loader_error_no_code_section;
        goto error_cleanup;
    }

    if (debug_entries) {
        // now write the debug entries
        for (iterator it = ptr_list_iterator_create(debug_entries);
                it.has_next; it = iterator_next(it)) {
            lstf_vm_debugentry *entry = iterator_get_item(it);

            ptr_hashmap_insert(program->debug_entries,
                    program->code + entry->instruction_offset, entry);
        }
    }

    if (debug_symbols) {
        // now write the debug symbols
        for (iterator it = ptr_list_iterator_create(debug_symbols);
                it.has_next; it = iterator_next(it)) {
            lstf_vm_debugsym *symbol = iterator_get_item(it);

            ptr_hashmap_insert(program->debug_symbols,
                    program->code + symbol->instruction_offset, symbol);
        }
    }

    if (debug_entries)
        ptr_list_destroy(debug_entries);
    if (debug_symbols)
        ptr_list_destroy(debug_symbols);
    inputstream_unref(istream);
    return program;

//  -------------------------------------------
error_cleanup:
    if (debug_entries)
        ptr_list_destroy(debug_entries);
    if (debug_symbols)
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