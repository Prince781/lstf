#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <synchapi.h>
#else
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif
#include <fcntl.h>

#if defined(_WIN32) || defined(_WIN64)
static char *win32_errormsg(int errnum)
{
    static thread_local char buffer[1024];

    if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            errnum,
            0,
            buffer,
            sizeof buffer,
            NULL))
        snprintf(buffer, sizeof buffer, "Windows error %#0x", errnum);

    return buffer;
}
#endif

__attribute__((warn_unused_result))
static event *event_new(async_callback callback, void *callback_data)
{
    event *ev = calloc(1, sizeof *ev);

    // TODO: handle NULL

    ev->type = event_type_default;
    ev->callback = callback;
    ev->callback_data = callback_data;

    return ev;
}

__attribute__((warn_unused_result))
static event *event_new_from_fd(int            fd,
                                bool           is_read_operation,
                                async_callback callback,
                                void          *callback_data)
{
    event *ev = calloc(1, sizeof *ev);

    // TODO: handle NULL

    ev->fd = fd;
    ev->type = is_read_operation ? event_type_io_read : event_type_io_write;
    ev->callback = callback;
    ev->callback_data = callback_data;

    return ev;
}

/**
 * (to be used from a background thread)
 *
 * Signal the event loop that a background task may be ready.
 */
static void eventloop_signal(eventloop *loop)
{
#if defined(_WIN32) || defined(_WIN64)
    if (!SetEvent(loop->bg_eventh)) {
        fprintf(stderr, "%s: failed to signal event loop: %s\n",
                __func__, win32_errormsg(GetLastError()));
        abort();
    }
#else
    if (write(loop->bg_signalfd, "a", 1) == -1) {
        fprintf(stderr, "%s: failed to signal event loop: %s\n",
                __func__, strerror(errno));
        abort();
    }
#endif
}

void event_return(event *ev, void *result)
{
    bool was_ready =
        atomic_exchange_explicit(&ev->is_ready, true, memory_order_release);
    assert(!was_ready && "cannot complete an event twice!");

    ev->result = result;

    if (ev->type == event_type_bg_task)
        /**
         * We only need to signal the event loop if we're completing this event
         * from a background thread. If we're in the foreground, this isn't
         * necessary because we just set [is_ready] to true.
         */
        eventloop_signal(ev->loop);
}

bool event_get_result(const event *ev, void **pointer_ref)
{
    if (event_is_canceled(ev) || event_get_errno(ev))
        return false;

    assert(event_is_ready(ev) && "must get result when event is ready");

    // at this point we don't have to use atomics since no concurrent procedure
    // will modify event anymore
    if (pointer_ref)
        *pointer_ref = ev->result;
    return true;
}

eventloop *eventloop_new(void)
{
    eventloop *loop = calloc(1, sizeof *loop);
    loop->is_running = true;

    // create the event handle/FD for non-I/O background tasks
#if defined(_WIN32) || defined(_WIN64)
    if (!(loop->bg_eventh = CreateEvent(NULL, false, false, NULL))) {
        fprintf(stderr, "%s: failed to create event handle: %s\n",
                __func__, win32_errormsg(GetLastError()));
        abort();
    }
#else
    int fds[2];
    if (pipe(fds) == -1) {
        fprintf(stderr, "%s: failed to create event FD with pipe: %s\n",
                __func__, strerror(errno));
        abort();
    }
    loop->bg_eventfd = fds[0];
    loop->bg_signalfd = fds[1];

    if (fcntl(loop->bg_eventfd, F_SETFL, fcntl(loop->bg_eventfd, F_GETFL) | O_NONBLOCK) == -1 ||
            fcntl(loop->bg_signalfd, F_SETFL, fcntl(loop->bg_signalfd, F_GETFL) | O_NONBLOCK)) {
        fprintf(stderr, "%s: failed to set fds to non-blocking: %s\n",
                __func__, strerror(errno));
        abort();
    }

    mtx_init(&loop->monitoring_lock, mtx_plain);
#endif

    return loop;
}

/**
 * Adds the event to the event list. Returns the previous event.
 */
static event *eventloop_add_event(eventloop *loop, event *ev) 
{
    assert(!ev->loop && "event is already part of an event loop");
    assert(!ev->is_ready && "cannot add completed event to an event loop");
    assert(!ev->is_canceled && "cannot add canceled event to an event loop");
    event *prev = NULL;
    if (!loop->pending_events) {
        assert(!loop->pending_events_tail);
        loop->pending_events = ev;
        loop->pending_events_tail = ev;
    } else {
        assert(loop->pending_events_tail);
        prev = loop->pending_events_tail;
        if (loop->pending_events == loop->pending_events_tail) {
            loop->pending_events_tail = ev;
            loop->pending_events->next = loop->pending_events_tail;
        } else {
            loop->pending_events_tail->next = ev;
            loop->pending_events_tail = ev;
        }
    }
    ev->loop = loop;
    // fprintf(stderr, "[DEBUG] queued a new event\n");
    return prev;
}

static void eventloop_remove(eventloop *loop, event *ev, event *prev)
{
    if (!prev) {
        if (loop->pending_events == loop->pending_events_tail)
            loop->pending_events_tail = loop->pending_events->next;
        loop->pending_events = loop->pending_events->next;
    } else {
        prev->next = ev->next;
    }
    if (ev == loop->pending_events_tail)
        loop->pending_events_tail = prev;
    ev->next = NULL;
}

event *eventloop_add(eventloop     *loop,
                     async_callback callback,
                     void          *callback_data) {
    event *ev = event_new(callback, callback_data);
    eventloop_add_event(loop, ev);
    return ev;
}

event *eventloop_add_fd(eventloop     *loop,
                        int            fd,
                        bool           is_read_operation,
                        async_callback callback,
                        void          *callback_data)
{
    event *ev = event_new_from_fd(fd, is_read_operation, callback, callback_data);
    eventloop_add_event(loop, ev);
    return ev;
}

static int eventloop_bgtask_start(void *data)
{
    event *ev = data;
    ev->thread_proc(ev);
    assert((event_is_ready(ev) || event_is_canceled(ev)) &&
           "background process did not complete or cancel the event!");
    return 0;
}

event *eventloop_add_bgtask(eventloop      *loop,
                            background_proc task,
                            void           *task_data,
                            async_callback  callback,
                            void           *callback_data)
{
    event *ev = event_new(callback, callback_data);
    ev->type = event_type_bg_task;
    ev->thread_proc = task;
    ev->thread_data = task_data;
    event *prev = eventloop_add_event(loop, ev);

    if (thrd_create(&ev->thread, eventloop_bgtask_start, ev) != thrd_success) {
        eventloop_remove(loop, ev, prev);
        free(ev);
        return NULL;
    }

    return ev;
}

#define eventloop_pending_events_foreach(loop, pending, prev, statements)      \
    {                                                                          \
        mtx_lock(&loop->monitoring_lock);                                      \
        for (event *pending = loop->pending_events, *prev = NULL; pending;) {  \
            event *next = pending->next;                                       \
            { statements; }                                                    \
            if (pending)                                                       \
                prev = pending;                                                \
            pending = next;                                                    \
        }                                                                      \
        mtx_unlock(&loop->monitoring_lock);                                    \
    }

static inline void event_list_prepend(event **list, event *ev)
{
    ev->next = *list;
    *list = ev;
}

#if !(defined(_WIN32) || defined(_WIN64))
/**
 * The monitor for subprocesses. We only launch this when we add a new
 * subprocess into the queue, if the monitor is not already running.
 */
static int eventloop_monitor_subprocesses(void *user_data)
{
    eventloop *loop = user_data;

    // TODO: We want a way to monitor only exactly those processes that are part
    // of the event loop, without busy-waiting, conforming to POSIX. Is this
    // possible? Or maybe we have to busy-wait? Or perhaps we have to drop POSIX?
    while (loop->monitoring_thread_running) {
        // first, look in our queue for previously unmanaged processes
        mtx_lock(&loop->monitoring_lock);
        for (unsigned i = 0; i < loop->ready_processes.length; ) {
            io_procstat *proc =
                &loop->ready_processes
                     .elements[loop->ready_processes.length - 1];
            bool removed = false;
            eventloop_pending_events_foreach(loop, pending, prev, {
                (void)prev;
                if (pending->type == event_type_subprocess && pending->process == proc->process) {
                    event_return(pending, (void *)(intptr_t)proc->status);
                    eventloop_signal(loop);
                    // TODO: use linked list instead of array?
                    array_remove(&loop->ready_processes, i);
                    removed = true;
                }
            });
            if (!removed)
                ++i;
        }
        mtx_unlock(&loop->monitoring_lock);

        // wait for a process...
        int   child_status = 0;
        pid_t child_pid    = -1;
        while ((child_pid = wait(&child_status)) == (pid_t)-1) {
            if (errno == ECHILD) {
                // no child processes. wait for .2 s.
                thrd_sleep(&(struct timespec){.tv_nsec = 200000000}, NULL);
                break;
            }
            if (errno != EAGAIN) {
                fprintf(stderr, "%s: unhandled error with wait(): %s\n",
                        __func__, strerror(errno));
                abort();
            }
        }

        if (child_pid != -1) {
            bool is_monitored = false;

            // check if this child is a managed process
            eventloop_pending_events_foreach(loop, pending, prev, {
                (void)prev;
                if (pending->type == event_type_subprocess &&
                    pending->process == child_pid) {
                    event_return(pending, (void *)(intptr_t)child_status);
                    eventloop_signal(loop);
                    is_monitored = true;
                }
            });

            if (!is_monitored) {
                // child not managed. save the process for a later event
                mtx_lock(&loop->monitoring_lock);
                array_add(&loop->ready_processes,
                          ((io_procstat){child_pid, child_status}));
                mtx_unlock(&loop->monitoring_lock);
            }
        }
    }

    return 0;
}
#endif

event *eventloop_add_subprocess(eventloop     *loop,
                                io_process     process,
                                async_callback callback,
                                void          *callback_data)
{
    event *ev = event_new(callback, callback_data);
    ev->type = event_type_subprocess;
    ev->process = process;
    event *prev = eventloop_add_event(loop, ev);

    mtx_lock(&loop->monitoring_lock);
#if !(defined(_WIN32) || defined(_WIN64))
    // There is no OS primitive on POSIX for monitoring both file descriptors
    // and subprocesses at the same time (alas, we can't use Linux's pidfds). So
    // on POSIX, we launch a monitoring thread that will write to
    // [loop->bg_signalfd] when a subprocess is done. See
    // eventloop_monitor_subprocesses() for more details.
    array_add(&loop->processes, process);
    if (!loop->monitoring_thread_running) {
        loop->monitoring_thread_running = true;
        if (thrd_create(&loop->monitoring_thread,
                        eventloop_monitor_subprocesses, loop) != thrd_success) {
            mtx_unlock(&loop->monitoring_lock);
            eventloop_remove(loop, ev, prev);
            free(ev);
            return NULL;
        }
    }
    thrd_detach(loop->monitoring_thread);
#endif
    mtx_unlock(&loop->monitoring_lock);

    return ev;
}

#if defined(_WIN32) || defined(_WIN64)
typedef struct {
    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    unsigned num_handles;
    int timeout_ms;
} eventloop_poll_thread_state;

static void eventloop_poll_thread_setup_state(eventloop_poll_thread_state *state,
                                              const int                    i,
                                              bool                        *added_bgeventh,
                                              HANDLE                       barrier,
                                              eventloop                   *loop,
                                              event                      **current_ev)
{
    if (i == 0) {
        state->handles[i] = barrier;
        state->num_handles++;
    } else if (!*added_bgeventh) {
        state->handles[i] = loop->bg_eventh;
        *added_bgeventh = true;
        state->num_handles++;
    } else if (*current_ev) {
        while (*current_ev && (*current_ev)->fd == -1)
            *current_ev = (*current_ev)->next;

        if (*current_ev) {
            HANDLE h = (HANDLE) _get_osfhandle((*current_ev)->fd);
            if (h == INVALID_HANDLE_VALUE) {
                fprintf(stderr, "%s: failed to convert fd %d to Win32 handle: %s\n",
                        __func__, (*current_ev)->fd, win32_errormsg(GetLastError()));
                abort();
            }
            state->handles[i] = h;
            *current_ev = (*current_ev)->next;
            state->num_handles++;
        }
    }
}

static int eventloop_poll_thread(void *user_data)
{
    const eventloop_poll_thread_state *state = user_data;

    DWORD wait_status =
        WaitForMultipleObjects(state->num_handles, state->handles, false, state->timeout_ms);

    if (wait_status == WAIT_FAILED) {
        fprintf(stderr, "WaitForMultipleObjects() failed: %s\n", win32_errormsg(GetLastError()));
        abort();
    } else if (!((wait_status >= WAIT_OBJECT_0 && wait_status < WAIT_OBJECT_0 + state->num_handles) ||
                wait_status == WAIT_TIMEOUT)) {
        fprintf(stderr, "WaitForMultipleObjects(): unexpected abandoned mutex in wait list\n");
        abort();
    }

    // the first thread to break out of its wait should wake up the other threads
    if (!SetEvent(state->handles[0])) {
        fprintf(stderr, "failed to signal barrier: %s\n", win32_errormsg(GetLastError()));
        abort();
    }
    return 0;
}
#endif

/**
 * Wait for an event to happen. This will block if there are no events that
 * have already completed (when *ready_events != NULL) and force_nonblocking is
 * false. Otherwise this will return immediately.
 */
static void eventloop_poll(eventloop *loop,
                           event    **ready_events,
                           bool       force_nonblocking,
                           unsigned   num_io_pending,
                           bool       have_non_io_tasks)

{
    if (num_io_pending + have_non_io_tasks == 0)
        return;

#if defined(_WIN32) || defined(_WIN64)
    // Windows
    int timeout_ms = (*ready_events && !force_nonblocking) ? 0 : INFINITE;

    // because of limitations with WaitForMultipleObjectsEx(), we must split the
    // waiting across multiple threads

    HANDLE barrier = CreateEvent(NULL, false, false, NULL);

    const unsigned bucket_size = MAXIMUM_WAIT_OBJECTS - 1 /* because of the barrier */;
    const unsigned num_bg_threads = (num_io_pending + have_non_io_tasks + (bucket_size - 1)) / bucket_size - 1;

    thrd_t *bg_threads = calloc(num_bg_threads, sizeof *bg_threads);
    eventloop_poll_thread_state *bg_states = calloc(num_bg_threads, sizeof *bg_states);
    eventloop_poll_thread_state fg_state;
    event *current_ev = loop->pending_events;
    bool added_bgeventh = false;

    // set up foreground polling thread
    fg_state.num_handles = 0;
    fg_state.timeout_ms = timeout_ms;
    for (unsigned i = 0; i < ARRAYSIZE(fg_state.handles) && current_ev; i++)
        eventloop_poll_thread_setup_state(&fg_state, i, &added_bgeventh, barrier, loop, &current_ev);

    // set up each background polling thread
    for (unsigned i = 0; i < num_bg_threads; i++) {
        bg_states[i].num_handles = 0;
        bg_states[i].timeout_ms = timeout_ms;
        for (unsigned j = 0; j < ARRAYSIZE(bg_states[i].handles) && current_ev; j++)
            eventloop_poll_thread_setup_state(&bg_states[i], j, &added_bgeventh, barrier, loop, &current_ev);

        // fire the background polling thread
        if (thrd_create(&bg_threads[i], eventloop_poll_thread, &bg_states[i]) == thrd_error) {
            fprintf(stderr, "%s: failed to create a background polling thread: %s\n",
                    __func__, win32_errormsg(GetLastError()));
            abort();
        }
    }

    // poll in foreground
    (void) eventloop_poll_thread(&fg_state);

    // join all background poll threads
    for (unsigned i = 0; i < num_bg_threads; i++) {
        if (thrd_join(bg_threads[i], NULL) == thrd_error) {
            fprintf(stderr, "%s: failed to join polling thread #%u: %s\n",
                    __func__, i + 1, win32_errormsg(GetLastError()));
            abort();
        }
    }

    // gather all I/O-ready tasks
    for (event *pending = loop->pending_events,
               *prev    = NULL;
                pending; ) {
        event *next = pending->next;

        if (pending->type == event_type_io_read || pending->type == event_type_io_write) {
            if (WaitForSingleObject((HANDLE) _get_osfhandle(pending->fd), 0) == WAIT_OBJECT_0) {
                // remove [pending] off the list of pending tasks and add
                // it to the list of ready tasks
                event_return(pending, NULL);
                eventloop_remove(loop, pending, prev);
                event_list_prepend(ready_events, pending);
                pending = NULL;
            }
        } else if (pending->type == event_type_bg_task && event_is_ready(pending)) {
            eventloop_remove(loop, pending, prev);
            event_list_prepend(ready_events, pending);
            pending = NULL;
        }

        prev = pending;
        pending = next;
    }
    
    free(bg_threads);
    free(bg_states);
    CloseHandle(barrier);
#else
    // POSIX

    struct pollfd *pollfds = calloc(num_io_pending + have_non_io_tasks, sizeof *pollfds);
    unsigned p = 0,
             p_bgevent = 0 /* at the very end of `pollfds` */;
    eventloop_pending_events_foreach(loop, pending, prev, {
        (void)prev;
        if (pending->type == event_type_io_read || pending->type == event_type_io_write) {
            pollfds[p].fd = pending->fd;
            pollfds[p].events = pending->type == event_type_io_read ? POLLIN : POLLOUT;
            ++p;
        }
    });

    if (have_non_io_tasks) {
        pollfds[p].fd = loop->bg_eventfd;
        pollfds[p].events = POLLIN;
        p_bgevent = p;
        ++p;
    }

    // poll without waiting (0) if there are other ready tasks
    // otherwise, poll with an indefinite wait (-1)
    int timeout_ms = (*ready_events && !force_nonblocking) ? 0 : -1;

    while (poll(pollfds, num_io_pending + have_non_io_tasks, timeout_ms) == -1 &&
            (errno == EAGAIN || errno == EINTR))
        ;

    // gather all I/O-ready tasks
    p = 0;
    eventloop_pending_events_foreach(loop, pending, prev, {
        if (pending->type == event_type_io_read || pending->type == event_type_io_write) {
            if (pollfds[p].revents & (pending->type == event_type_io_read ? POLLIN : POLLOUT)) {
                // remove [pending] off the list of pending tasks and add
                // it to the list of ready tasks
                event_return(pending, NULL);
                eventloop_remove(loop, pending, prev);
                event_list_prepend(ready_events, pending);
                pending = NULL;
            } else if (pollfds[p].revents & (POLLHUP | POLLERR)) {
                event_cancel_with_errno(pending, pollfds[p].revents & POLLHUP ? EPIPE : ECANCELED);
                eventloop_remove(loop, pending, prev);
                event_list_prepend(ready_events, pending);
                pending = NULL;
            }
            ++p;
        } else if (pending->type == event_type_bg_task || pending->type == event_type_subprocess) {
            // (have_non_io_tasks == true)
            if ((pollfds[p_bgevent].revents & POLLIN) && event_is_ready(pending)) {
                eventloop_remove(loop, pending, prev);
                event_list_prepend(ready_events, pending);
                pending = NULL;
            }
        }
    });

    // clear the thread event fd
    if (have_non_io_tasks && pollfds[p_bgevent].revents & POLLIN) {
        char buffer[BUFSIZ];
        while (read(pollfds[p_bgevent].fd, &buffer, sizeof buffer) == -1 &&
               errno == EAGAIN)
            ;
    }

    free(pollfds);
#endif
}


bool eventloop_process(eventloop *loop, 
                       bool       force_nonblocking,
                       unsigned  *num_processed)
{
    event *ready_events = NULL;

    unsigned num_io_pending = 0;
    bool have_non_io_tasks = false;

    eventloop_pending_events_foreach(loop, pending, prev, {
        if (event_is_ready(pending) || event_is_canceled(pending)) {
            // remove [pending] off the list of pending tasks and add it to
            // the list of [ready_tasks]
            eventloop_remove(loop, pending, prev);
            event_list_prepend(&ready_events, pending);
            pending = NULL;
        } else if (pending->type == event_type_io_read || pending->type == event_type_io_write) {
            num_io_pending++;
        } else if (pending->type == event_type_bg_task || pending->type == event_type_subprocess) {
            have_non_io_tasks = true;
        }
    });

    // wait for events
    eventloop_poll(loop, &ready_events, force_nonblocking, num_io_pending, have_non_io_tasks);

    while (ready_events) {
        event *ev = ready_events;

        ready_events = ready_events->next;
        ev->next = NULL;

        ev->callback(ev, ev->callback_data);

        free(ev);
        // fprintf(stderr, "[DEBUG] processed and removed an event\n");

        if (num_processed)
            ++*num_processed;
    }

    // two possible conditions that terminate an event loop:
    // 1. no more pending events
    // 2. explicit termination was requested
    return loop->is_running && loop->pending_events;
}

void eventloop_quit(eventloop *loop)
{
    loop->is_running = false;
    loop->monitoring_thread_running = false;
}

void eventloop_destroy(eventloop *loop)
{
    loop->is_running = false;
    while (loop->pending_events) {
        event *ev = loop->pending_events;
        event_cancel(ev);
        eventloop_remove(loop, loop->pending_events, NULL);
        free(ev);
    }

    // close event handle/FD
#if defined(_WIN32) || defined(_WIN64)
    CloseHandle(loop->bg_eventh);
#else
    close(loop->bg_eventfd);
    close(loop->bg_signalfd);
    mtx_destroy(&loop->monitoring_lock);
#endif

    free(loop);
}
