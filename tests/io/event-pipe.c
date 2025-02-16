#include "io/event.h"
#include "io/inputstream.h"
#include "io/io-common.h"
#include "io/outputstream.h"
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

// keep the message simple (no newline chars for example that might get
// converted to '\r\n' in printf())
static const char message[] = "Hello, world!";

static int subprocess_entry(void) {
    printf("%s", message);
    return 0;
}

struct data_ready_params {
    inputstream *child_stdout;
    char (*buffer)[BUFSIZ];
};

static void data_ready_cb(const event *ev, void *user_data) {
    struct data_ready_params *params = user_data;
    inputstream *child_stdout = params->child_stdout;
    char (*buffer)[BUFSIZ] = params->buffer;

    if (event_get_result(ev, NULL)) {
        // we can read without blocking
        // FIXME: requires fixing inputstream_ready() for fd-backed streams
        // assert(inputstream_ready(is) &&
        //        "inputstream not ready after event loop signals ready");
        printf("[parent] reading from child...\n");
        if (!inputstream_read(child_stdout, *buffer, sizeof(*buffer) - 1))
            fprintf(stderr, "[parent] failed to read pipe: %s\n", strerror(errno));
        eventloop_quit(ev->loop);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-subprocess") == 0)
        return subprocess_entry();

    if (argc != 1) {
        fprintf(stderr, "usage: %s [-subprocess]\n", argv[0]);
        return 1;
    }

    inputstream *child_stdout = NULL;
    if (!io_communicate(argv[0], (const char *[]){argv[0], "-subprocess", NULL},
                        /*in_stream=*/NULL, &child_stdout,
                        /*err_stream=*/NULL, /*subprocess=*/NULL)) {
        fprintf(stderr, "[parent] failed to launch subprocess: %s\n",
                strerror(errno));
        inputstream_unref(child_stdout);
        return 1;
    }

    // 1. spawn a subprocess
    // 2. create an event loop
    // 3. communicate with the subprocess
    // 4. terminate
    char buffer[BUFSIZ] = { 0 };
    struct data_ready_params params = {child_stdout, &buffer};

    eventloop *loop = eventloop_new();

    eventloop_add_fd(loop, inputstream_get_fd(child_stdout), true,
                     data_ready_cb, &params);

    unsigned num_processed = 0;
    for (unsigned processed = num_processed; eventloop_process(loop, true, &num_processed);
        processed = num_processed) {
        if (processed - num_processed == 0) {
            // avoid busy-waiting
            thrd_sleep(&(struct timespec){.tv_nsec = 200000000}, NULL);
        }
    }

    if (num_processed > 1)
        fprintf(stderr, "[parent] processed too many events: %u\n", num_processed);

    bool equal = true;

    if (!(equal = strcmp(buffer, message) == 0))
        fprintf(stderr, "[parent] expected `%s', got `%s'\n", message, buffer);

    eventloop_destroy(loop);
    inputstream_unref(child_stdout);
    return !(equal && num_processed == 1);  // only 1 event should be processed
}
