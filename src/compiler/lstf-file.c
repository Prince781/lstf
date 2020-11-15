#include "lstf-file.h"
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
    file->filename = strdup(filename);

    // read in entire file
    while (!feof(stream) && !ferror(stream)) {
        if (content_length >= buffer_size) {
            if (buffer_size == 0)
                buffer_size = 1024 * 1024;
            else
                buffer_size *= 2;
            char *new_content = realloc(content, buffer_size);
            if (!new_content) {
                perror("could not expand file buffer");
                exit(EXIT_FAILURE);
            }
            content = new_content;
        }
        content_length += fread(content, 1, buffer_size - content_length, stream);
    }

    if (ferror(stream)) {
        fprintf(stderr, "%s: error reading LSTF script `%s': %s\n", __func__, filename, strerror(errno));
        free(content);
        free(file->filename);
        free(file);
    }

    file->content = content;
    if (file->content)
        file->content[content_length] = '\0';

    fclose(stream);

    return file;
}

void lstf_file_unload(lstf_file *file)
{
    free(file->content);
    free(file->filename);
    free(file);
}
