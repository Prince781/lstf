#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

typedef struct _event event;
typedef void (*async_callback)(event *ev, void *data);
typedef struct _eventloop eventloop;

struct _event {
    int fd;
    struct {                        // flags
        bool is_read_operation;    
        bool is_io_operation;    
        bool is_bg_operation;       // whether this is an operation running in a background thread
        atomic_bool is_canceled;
        atomic_bool is_ready;       // can be set to avoid poll()ing
    };
    int io_errno;                   // relevant if the I/O operation failed
    thrd_t thread;                  // reference to the background thread (if relevant)
    async_callback callback;
    void *callback_data;
    eventloop *loop;
    void *result;
    struct _event *next;
};

event *event_new(async_callback callback, void *callback_data);

event *event_new_from_fd(int            fd,
                         bool           is_read_operation,
                         async_callback callback,
                         void          *callback_data);

event *event_new_for_background(async_callback callback, void *callback_data);

void event_return(event *ev, void *result);

static inline int event_get_errno(event *ev)
{
    return ev->io_errno;
}

void event_cancel_with_errno(event *ev, int errnum);

static inline void event_cancel(event *ev)
{
    event_cancel_with_errno(ev, ECANCELED);
}

bool event_is_canceled(event *ev);

bool event_is_ready(event *ev);

bool event_get_result(event *ev, void **pointer_ref);

static inline void event_list_prepend(event **list, event *ev)
{
    ev->next = *list;
    *list = ev;
}

struct _eventloop {
    event *pending_events;
    event *pending_events_tail;
    bool is_running;

#if defined(_WIN32) || defined(_WIN64)
    /**
     * An event handle (created with CreateEvent()) to wait on/signal for when
     * background (thread) task becomes ready.
     */
    HANDLE bg_eventh;
#else
    /**
     * A file descriptor to poll for when background (thread) tasks are ready.
     * This is needed so that we can wait for tasks that are not I/O-bound
     * without being busy.
     */
    int bg_eventfd;

    /**
     * A file descriptor to write to, to signal that a background task may be
     * ready.
     */
    int bg_signalfd;
#endif

};

eventloop *eventloop_new(void);

void eventloop_add(eventloop *loop, event *ev);

void eventloop_remove(eventloop *loop, event *ev, event *prev);

/**
 * Process all ready events. May block if all events are busy, unless
 * `force_nonblocking` is `true`.
 * 
 * Returns `true` if the event loop should continue to run, or `false`
 * otherwise.
 */
bool eventloop_process(eventloop *loop, bool force_nonblocking);

void eventloop_signal(eventloop *loop);

void eventloop_destroy(eventloop *loop);
