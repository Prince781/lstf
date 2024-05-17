#pragma once

#include "lstf-common.h"
#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

typedef struct _event event;
typedef void (*async_callback)(event *ev, void *data);
typedef void (*background_proc)(event *ev);

typedef struct _eventloop eventloop;

enum _event_type {
    /**
     * An event. Use `event_return()` or `event_cancel()` to trigger the event.
     */
    event_type_default,

    /**
     * An event triggered by the completion of a task in a background thread.
     * Use `event_return()` or `event_cancel()` to trigger the event from the
     * background thread.
     */
    event_type_bg_task,

    /**
     * An event triggered by a file descriptor becoming ready for reading.
     */
    event_type_io_read,

    /**
     * An event triggered by a file descriptor becoming ready for writing.
     */
    event_type_io_write
} __attribute__((packed));
typedef enum _event_type event_type;

struct _event {
    int fd;
    event_type type;
    atomic_bool is_canceled;
    atomic_bool is_ready;           // can be set to avoid poll()ing
    atomic_int io_errno;            // relevant if the I/O operation failed
    thrd_t thread;                  // reference to the background thread (if relevant)
    background_proc thread_proc;    // procedure that is executed in the background thread (if relevant)
    void *thread_data;              // reference to data used by the background thread procedure (if relevant)
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

/**
 * Completes the event and associates `result` as the result. The callback
 * routine will be invoked on the next iteration of the event loop and when
 * that calls `event_get_result()` it will receive the result.
 */
void event_return(event *ev, void *result);

/**
 * Will return the error code, or 0 if this event did not fail with an error
 * code.
 */
static inline int event_get_errno(event *ev)
{
    return atomic_load_explicit(&ev->io_errno, memory_order_acquire);
}

static inline void event_cancel_with_errno(event *ev, int errnum)
{
    atomic_store_explicit(&ev->is_canceled, true, memory_order_release);
    atomic_store_explicit(&ev->io_errno, errnum, memory_order_release);
}

static inline void event_cancel(event *ev)
{
    event_cancel_with_errno(ev, ECANCELED);
}

/**
 * Will return `false` if the event is canceled.
 */
static inline bool event_is_canceled(event *ev)
{
    return atomic_load_explicit(&ev->is_canceled, memory_order_acquire);
}

static inline bool event_is_ready(event *ev)
{
    return atomic_load_explicit(&ev->is_ready, memory_order_acquire);
}

/**
 * Returns true and the result is stored in `*pointer_ref`. Otherwise returns
 * false if the event was canceled or reached an error.
 *
 * If `pointer_ref` is NULL, the meaning of `true` and `false` is whether the
 * event completed successfully without cancelation or error.
 */
bool event_get_result(event *ev, void **pointer_ref);

struct _eventloop {
    event *pending_events;
    event *pending_events_tail;
    atomic_bool is_running;

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

/**
 * Adds the event to the event list. Returns the previous event in the list.
 */
event *eventloop_add(eventloop *loop, event *ev);

/**
 * Spawns a background thread that runs `task`. When the task is done, it
 * should complete the event passed into it with either `event_return()` or
 * `event_cancel()`. Then, `callback` will be invoked with `callback_data` in
 * the event processing thread (wherever `eventloop_process()` is called).
 * `task` can retrieve `task_data` data by getting `ev->thread_data` from the
 * event passed to it.
 */
event *eventloop_add_bgtask(eventloop      *loop,
                            background_proc task,
                            void           *task_data,
                            async_callback  callback,
                            void           *callback_data);

/**
 * Process all ready events. May block if all events are busy, unless
 * `force_nonblocking` is `true`.
 * 
 * Returns `true` if the event loop should continue to run, or `false`
 * otherwise.
 */
bool eventloop_process(eventloop *loop, bool force_nonblocking);

void eventloop_signal(eventloop *loop);

void eventloop_quit(eventloop *loop);

void eventloop_destroy(eventloop *loop);
