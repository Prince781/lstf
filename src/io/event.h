#pragma once

#include "data-structures/array.h"
#include "io/io-common.h"
#include "lstf-common.h"
#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

typedef struct _event event;
typedef void (*async_callback)(const event *ev, void *data);
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
   * An event triggered by a subprocess completing or terminating.
   */
  event_type_subprocess,

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
    event_type type;
    atomic_bool is_canceled;
    atomic_bool is_ready;               // can be set to avoid poll()ing
    atomic_int io_errno;                // relevant if the I/O operation failed

    /**
     * Handles to resources if this event is triggered by file changes,
     * completion of a thread, or process termination.
     */
    union {
        int fd;                         // file descriptor

        /**
         * Holds data for background threads.
         */
        struct {
            thrd_t thread;              // reference to the background thread
            background_proc thread_proc;// procedure that is executed in the
                                        // background thread
            void *thread_data;          // reference to data used by the background thread
                                        // procedure
        };

        io_process process;             // process ID/handle
    };

    async_callback callback;
    void *callback_data;

    eventloop *loop;                    // a reference to the parent loop
    void *result;
    struct _event *next;
};

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
static inline int event_get_errno(const event *ev)
{
    return atomic_load_explicit(&ev->io_errno, memory_order_acquire);
}

/**
 * Cancels the event and associates `errnum` as the error code. The callback
 * routine will be invoked on the next iteration of the event loop and when that
 * calls `event_get_result()` it will fail, indicating an error.
 */
static inline void event_cancel_with_errno(event *ev, int errnum)
{
    atomic_store_explicit(&ev->is_canceled, true, memory_order_release);
    atomic_store_explicit(&ev->io_errno, errnum, memory_order_release);
}

/**
 * Cancels the event. The callback routine will be invoked on the next iteration
 * of the event loop and when that calls `event_get_result()` it will fail with
 * an error.
 */
static inline void event_cancel(event *ev)
{
    event_cancel_with_errno(ev, ECANCELED);
}

/**
 * Determines whether the event is canceled. An event cannot be both ready and
 * canceled.
 */
static inline bool event_is_canceled(const event *ev)
{
    return atomic_load_explicit(&ev->is_canceled, memory_order_acquire);
}

/**
 * Determines whether the event is finished in a successful state. An event
 * cannot be both ready and canceled.
 */
static inline bool event_is_ready(const event *ev)
{
    return atomic_load_explicit(&ev->is_ready, memory_order_acquire);
}

/**
 * Returns true and the result is stored in `*pointer_ref`. Otherwise returns
 * false if the event was canceled or reached an error. Then call
 * event_get_errno() to see why the event was cancelled.
 *
 * If `pointer_ref` is NULL, the meaning of `true` and `false` is whether the
 * event completed successfully without cancelation or error.
 */
bool event_get_result(const event *ev, void **pointer_ref)
    __attribute__((warn_unused_result));

struct _eventloop {
    event *pending_events;
    event *pending_events_tail;
    atomic_bool is_running;

#if !(defined(_WIN32) || defined(_WIN64))
    atomic_bool monitoring_thread_running;
#endif

#if defined(_WIN32) || defined(_WIN64)
    /**
     * An event handle (created with CreateEvent()) to wait on/signal for when
     * background (thread) task becomes ready.
     *
     * @see eventloop_signal()
     */
    HANDLE bg_eventh;
#else
    /**
     * A file descriptor to poll() for when background tasks (processes or
     * threads) are ready. This is needed so that we can wait for tasks that are
     * not I/O-bound without being busy. The background thread writes to
     * bg_signalfd, triggering an event on bg_eventfd.
     *
     * @see eventloop_signal()
     */
    int bg_eventfd;

    /**
     * A file descriptor to write() to, signaling that there are background
     * tasks (processes or threads) that are ready.
     *
     * @see eventloop_signal()
     */
    int bg_signalfd;

    /**
     * Thread to monitor subprocesses.
     *
     * @see eventloop_add_subprocess()
     */
    thrd_t monitoring_thread;

    mtx_t monitoring_lock;
#endif

    /**
     * Background processes that are ready but may not yet be associated with an
     * event.
     */
    array(io_procstat) ready_processes;

    /**
     * Background processes that are not ready. We need to keep track of exactly
     * which processes are managed to avoid confusion if there are unmanaged
     * children.
     */
    array(io_process) processes;
};

#define eventloop_processes_foreach(loop, element_name, statements)            \
    do {                                                                       \
        eventloop *element_name##loop = (loop);                                \
        mtx_lock(&(element_name##loop)->monitoring_lock);                      \
        for (unsigned i = 0; i < (element_name##loop)->processes.length;       \
             ++i) {                                                            \
            io_process *element_name =                                         \
                &(element_name##loop)->processes.elements[i];                  \
            statements;                                                        \
        }                                                                      \
        mtx_unlock(&(element_name##loop)->monitoring_lock);                    \
    } while (0)

__attribute__((warn_unused_result))
eventloop *eventloop_new(void);

/**
 * Creates a new event with a given callback. This event is static and won't
 * complete unless another event or piece of code calls `event_return()` /
 * `event_cancel*()` on it.
 */
__attribute__((warn_unused_result))
event *eventloop_add(eventloop     *loop,
                     async_callback callback,
                     void          *callback_data);

/**
 * Adds a new file descriptor to the event loop. This event will complete on its
 * own when the file descriptor is ready for reading or writing, depending on
 * [is_read_operation].
 */
event *eventloop_add_fd(eventloop     *loop,
                        int            fd,
                        bool           is_read_operation,
                        async_callback callback,
                        void          *callback_data);

/**
 * Spawns a background thread that runs `task`. When the task is done, it
 * should complete the event passed into it with either `event_return()` or
 * `event_cancel()`. Then, `callback` will be invoked with `callback_data` in
 * the event processing thread (wherever `eventloop_process()` is called,
 * usually the main thread). `task` can retrieve `task_data` data by getting
 * `ev->thread_data` from the event passed to it.
 */
event *eventloop_add_bgtask(eventloop      *loop,
                            background_proc task,
                            void           *task_data,
                            async_callback  callback,
                            void           *callback_data);

/**
 * Adds a new process identifier for a live (running or dead but not-yet-reaped)
 * process to the event loop. This event will complete on its own when the
 * process finishes.
 */
event *eventloop_add_subprocess(eventloop     *loop, 
                                io_process     process,
                                async_callback callback,
                                void          *callback_data);

/**
 * Process all ready events. May block if all events are busy, unless
 * `force_nonblocking` is `true`.
 * 
 * Returns `true` if the event loop should continue to run, or `false`
 * otherwise.
 *
 * @param num_processed (optional)
 */
bool eventloop_process(eventloop *loop, 
                       bool       force_nonblocking,
                       unsigned  *num_processed);

void eventloop_quit(eventloop *loop);

void eventloop_destroy(eventloop *loop);
