#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lstf-report.h"

void lstf_report(const lstf_sourceref *source_ref, const char *domain, const char *message, ...)
{
    // TODO: print colorized, formatted output if this is a terminal
    va_list args;

    // print context and arrows for errors across a single line
    if (source_ref->begin.line == source_ref->end.line) {
        const char *line_begin = source_ref->begin.pos;
        const char *line_end = source_ref->end.pos;
        while (line_begin > source_ref->file->content && !(*line_begin == '\n' || *line_begin == '\r'))
            line_begin--;
        if (*line_begin == '\n' || *line_begin == '\r')
            line_begin++;
        while (*line_end && !(*line_end == '\n' || *line_end == '\r'))
            line_end++;
        fwrite(line_begin, line_end - line_begin, 1, stderr);
        fputc('\n', stderr);
        // write the underline
        for (unsigned pos = 1; pos <= source_ref->end.column; pos++)
            fputc(source_ref->begin.column <= pos && pos <= source_ref->end.column ? 
                    (pos == source_ref->begin.column ? '^' : '~') : ' ', stderr);
        fputc('\n', stderr);
    }

    // TODO: print context for errors across multiple lines

    va_start(args, message);
    fprintf(stderr, "%s:%d.%d-%d.%d: %s: ", source_ref->file->filename,
                source_ref->begin.line, source_ref->begin.column,
                source_ref->end.line, source_ref->end.column,
                domain);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
}

