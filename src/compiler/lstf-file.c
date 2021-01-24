#include "lstf-file.h"
#include "lstf-scope.h"
#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include "lstf-variable.h"
#include "lstf-block.h"
#include "lstf-codenode.h"
#include "data-structures/collection.h"
#include "data-structures/ptr-list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

lstf_file *lstf_file_load(const char *filename)
{
    lstf_file *file = NULL;
    FILE *stream = NULL;
    char *content = NULL;
    size_t content_length = 0;
    size_t buffer_size = 0;

    if ((stream = fopen(filename, "r")) == NULL)
        return NULL;

    file = calloc(1, sizeof *file);
    if (!file) {
        perror("failed to create lstf_file");
        abort();
    }
    file->filename = strdup(filename);

    // read in entire file
    while (!feof(stream) && !ferror(stream)) {
        if (content_length >= buffer_size) {
            if (buffer_size == 0)
                buffer_size = 1024;
            else
                buffer_size = buffer_size + buffer_size / 2;
            char *new_content = realloc(content, buffer_size);
            if (!new_content) {
                perror("could not expand file buffer");
                exit(EXIT_FAILURE);
            }
            content = new_content;
        }
        content_length += fread(content + content_length, 1, buffer_size - content_length, stream);
    }

    if (ferror(stream)) {
        fclose(stream);
        free(content);
        free(file->filename);
        free(file);
        return NULL;
    }

    file->content = content;
    if (file->content)
        file->content[content_length] = '\0';

    fclose(stream);

    file->main_block = lstf_codenode_ref(lstf_block_new());

    return file;
}

void lstf_file_unload(lstf_file *file)
{
    free(file->content);
    file->content = NULL;
    free(file->filename);
    file->filename = NULL;
    lstf_codenode_unref(file->main_block);
    free(file);
}
