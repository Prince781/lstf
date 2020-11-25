#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lstf-report.h"

static const char *lstf_report_domain_to_string(lstf_report_domain domain)
{
    switch (domain) {
        case lstf_report_domain_error:
            return "error";
        case lstf_report_domain_info:
            return "info";
        case lstf_report_domain_warning:
            return "warning";
    }

    fprintf(stderr, "%s: invalid domain %d\n", __func__, domain);
    abort();
}

static unsigned max(unsigned a, unsigned b)
{
    return a > b ? a : b;
}

#if (_WIN32 || _WIN64)
#include <windows.h>
#include <consoleapi.h>
#include <io.h>
static bool is_ascii_terminal(FILE *file) {
    HANDLE file_handle = _get_osfhandle(fileno(file));
    if (!file_handle)
        return false;
    HWND win32_console_hwnd = GetStdHandle(file_handle);
    if (win32_console_hwnd)
        return !!SetConsoleMode(win32_console, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return false;
}
#else
#include <unistd.h>
static bool is_ascii_terminal(FILE *file) {
    return isatty(fileno(file));
}
#endif

void lstf_report(const lstf_sourceref *source_ref, lstf_report_domain domain, const char *message, ...)
{
    // TODO: print colorized, formatted output if this is a terminal
    va_list args;
    bool stderr_is_terminal = is_ascii_terminal(stderr);
    unsigned lines_log10 = 0;

    for (unsigned total_lines = source_ref->file->total_lines; total_lines > 0; total_lines /= 10)
        lines_log10++;

    lines_log10 = max(lines_log10, 4);

    va_start(args, message);
    const char *bold_begin = "";
    const char *color_begin = "";
    const char *normal_end = "";

    if (stderr_is_terminal) {
        bold_begin = "\x1b[1m";
        switch (domain) {
            case lstf_report_domain_error:
                color_begin = "\x1b[31m";   // ANSI red
                break;
            case lstf_report_domain_info:
                color_begin = "\x1b[36m";   // ANSI cyan
                break;
            case lstf_report_domain_warning:
                color_begin = "\x1b[35m";   // ANSI magenta
                break;
        }
        normal_end = "\x1b[0m";
    }
    fprintf(stderr, "%s%s:%d.%d-%d.%d: %s%s:%s ", bold_begin,
            source_ref->file->filename,
            source_ref->begin.line, source_ref->begin.column,
            source_ref->end.line, source_ref->end.column,
            color_begin,
            lstf_report_domain_to_string(domain),
            normal_end);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);

    // print context and arrows for errors across a single line
    if (source_ref->begin.line == source_ref->end.line) {
        const char *line_begin = source_ref->begin.pos;
        const char *line_end = source_ref->end.pos;
        if (line_begin > source_ref->file->content && (*line_begin == '\n' || *line_begin == '\r'))
            line_begin--;
        // move beginning to beginning of line
        while (line_begin > source_ref->file->content && !(*line_begin == '\n' || *line_begin == '\r'))
            line_begin--;
        if (*line_begin == '\n' || *line_begin == '\r')
            line_begin++;
        // move end to end of line
        while (*line_end && !(*line_end == '\n' || *line_end == '\r'))
            line_end++;

        // print the context
        fprintf(stderr, " %*u | ", lines_log10, source_ref->begin.line);

        fwrite(line_begin, source_ref->begin.pos - line_begin, 1, stderr);
        fprintf(stderr, "%s%s", bold_begin, color_begin);
        if (source_ref->end.pos + 1 < line_end) {
            fwrite(source_ref->begin.pos, (source_ref->end.pos + 1) - source_ref->begin.pos, 1, stderr);
            fprintf(stderr, "%s", normal_end);
            fwrite(source_ref->end.pos + 1, line_end - (source_ref->end.pos + 1), 1, stderr);
        } else {
            fwrite(source_ref->begin.pos, source_ref->end.pos - source_ref->begin.pos, 1, stderr);
            fprintf(stderr, "%s", normal_end);
            fwrite(source_ref->end.pos, line_end - source_ref->end.pos, 1, stderr);
        }
        fputc('\n', stderr);

        // write the underline
        fprintf(stderr, " %*s | ", lines_log10, " "); 

        fprintf(stderr, "%s%s", bold_begin, color_begin);
        for (unsigned pos = 1; pos <= source_ref->end.column; pos++)
            fputc(source_ref->begin.column <= pos && pos <= source_ref->end.column ? 
                    (pos == source_ref->begin.column ? '^' : '~') : ' ', stderr);
        fprintf(stderr, "%s", normal_end);
        fputc('\n', stderr);
    }

    // TODO: print context for errors across multiple lines
}

