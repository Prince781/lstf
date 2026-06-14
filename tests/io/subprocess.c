#include "io/io-common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *badprog = "./not-found";
    int saved_errno = 0;
    if (!io_communicate(badprog, (const char *[]){badprog, NULL}, NULL, NULL,
                        NULL, NULL)) {
        saved_errno = errno;
        fprintf(stderr, "[parent] failed to launch `%s': %s\n", badprog,
                strerror(errno));
    }
    return saved_errno == ENOENT ? 0 : 1;
}
