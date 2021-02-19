#include "io/inputstream.h"
#include "io/outputstream.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void)
{
    outputstream *ostream = outputstream_new_from_path("test-file.txt", "w+");
    inputstream *istream = inputstream_new_from_outputstream(ostream);
    int retval = 0;

    // 1. write to output stream
    // 2. confirm that input stream has same

    const char str[] = "this is a testing message";
    if (outputstream_write_string(ostream, str) != strlen(str)) {
        retval = 1;
        fprintf(stderr, "failed to write string: %s\n", strerror(errno));
    }

    char buffer[128] = { 0 };
    if (inputstream_read(istream, buffer, sizeof buffer - 1) == 0) {
        retval = 1;
        fprintf(stderr, "failed to read string: %s\n", strerror(errno));
    } else if (strncmp(buffer, str, sizeof str) != 0) {
        retval = 1;
        fprintf(stderr, "data read from pipe does not equal data put into pipe\n");
    }

    inputstream_unref(istream);
    return retval;
}
