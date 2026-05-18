#ifndef _WIN32
#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/event.h>
#include <xlib/time.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <sys/epoll.h>
#define XLIB_EVENT_USE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/event.h>
#include <sys/time.h>
#define XLIB_EVENT_USE_KQUEUE 1
#endif

enum x_event_source_type {
    X_EVENT_SOURCE_SOCKET = 1,
    X_EVENT_SOURCE_TIMER = 2,
    X_EVENT_SOURCE_PROCESS = 3,
    X_EVENT_SOURCE_FILE = 4
};

struct x_event_source {
    x_event_loop_t *loop;
    int type;
    int active;
    int internal;
    int events;
    uintptr_t native_handle;
    x_socket_t *socket;
    x_process_t *process;
    x_file_watcher_t *watcher;
    uint64_t due_ns;
    uint64_t interval_ns;
    x_event_callback callback;
    void *user_data;
};

struct x_event_loop {
    x_event_source_t **sources;
    size_t count;
    size_t capacity;
    int running;
    int stop_requested;
    x_socket_t *wake_receiver;
    x_socket_t *wake_sender;
    x_socket_address_t wake_address;
    x_event_source_t *wake_source;

#if defined(XLIB_EVENT_USE_EPOLL)
    int backend_fd;
#elif defined(XLIB_EVENT_USE_KQUEUE)
    int backend_fd;
#endif
};

static uint64_t x_event_ms_to_ns(uint64_t milliseconds)
{
    if (milliseconds > UINT64_MAX / 1000000ULL) {
        return UINT64_MAX;
    }

    return milliseconds * 1000000ULL;
}

static int x_event_loop_reserve(x_event_loop_t *loop, size_t needed)
{
    x_event_source_t **sources;
    size_t capacity;

    if (needed <= loop->capacity) {
        return 0;
    }

    capacity = loop->capacity == 0U ? 8U : loop->capacity * 2U;
    while (capacity < needed) {
        capacity *= 2U;
    }

    sources = (x_event_source_t **)realloc(loop->sources, capacity * sizeof(*sources));
    if (sources == NULL) {
        return ENOMEM;
    }

    loop->sources = sources;
    loop->capacity = capacity;
    return 0;
}

static void x_event_loop_compact(x_event_loop_t *loop)
{
    size_t read_index;
    size_t write_index = 0U;

    for (read_index = 0U; read_index < loop->count; ++read_index) {
        x_event_source_t *source = loop->sources[read_index];
        if (source->active) {
            loop->sources[write_index++] = source;
        } else {
            free(source);
        }
    }

    loop->count = write_index;
}

static size_t x_event_loop_active_user_sources(x_event_loop_t *loop)
{
    size_t i;
    size_t count = 0U;

    for (i = 0U; i < loop->count; ++i) {
        x_event_source_t *source = loop->sources[i];
        if (source->active && !source->internal) {
            ++count;
        }
    }

    return count;
}

static int x_event_source_dispatch(x_event_source_t *source, int events)
{
    if (!source->active || source->callback == NULL) {
        return 0;
    }

    return source->callback(source->loop, source, events, source->user_data);
}

static int x_event_file_flags(int watch_events)
{
    int events = X_EVENT_FILE;

    if ((watch_events & X_FILE_WATCH_CREATED) != 0) {
        events |= X_EVENT_FILE_CREATED;
    }
    if ((watch_events & X_FILE_WATCH_MODIFIED) != 0) {
        events |= X_EVENT_FILE_MODIFIED;
    }
    if ((watch_events & X_FILE_WATCH_DELETED) != 0) {
        events |= X_EVENT_FILE_DELETED;
    }

    return events;
}

static void x_event_file_watch_callback(const char *path, int events, void *user_data)
{
    int *pending_events = (int *)user_data;

    (void)path;
    *pending_events |= events;
}

#if defined(XLIB_EVENT_USE_EPOLL)
static uint32_t x_event_epoll_flags(int events)
{
    uint32_t flags = 0U;

    if ((events & X_EVENT_READ) != 0) {
        flags |= EPOLLIN;
    }

    if ((events & X_EVENT_WRITE) != 0) {
        flags |= EPOLLOUT;
    }

    return flags;
}

static int x_event_backend_add_socket(x_event_loop_t *loop, x_event_source_t *source)
{
    struct epoll_event event_data;

    memset(&event_data, 0, sizeof(event_data));
    event_data.events = x_event_epoll_flags(source->events);
    event_data.data.ptr = source;

    if (epoll_ctl(loop->backend_fd, EPOLL_CTL_ADD, (int)source->native_handle, &event_data) != 0) {
        return errno;
    }

    return 0;
}

static void x_event_backend_remove_socket(x_event_loop_t *loop, x_event_source_t *source)
{
    epoll_ctl(loop->backend_fd, EPOLL_CTL_DEL, (int)source->native_handle, NULL);
}
#elif defined(XLIB_EVENT_USE_KQUEUE)
static int x_event_backend_add_socket(x_event_loop_t *loop, x_event_source_t *source)
{
    struct kevent changes[2];
    int count = 0;

    if ((source->events & X_EVENT_READ) != 0) {
        EV_SET(&changes[count++], source->native_handle, EVFILT_READ, EV_ADD, 0, 0, source);
    }

    if ((source->events & X_EVENT_WRITE) != 0) {
        EV_SET(&changes[count++], source->native_handle, EVFILT_WRITE, EV_ADD, 0, 0, source);
    }

    if (count > 0 && kevent(loop->backend_fd, changes, count, NULL, 0, NULL) != 0) {
        return errno;
    }

    return 0;
}

static void x_event_backend_remove_socket(x_event_loop_t *loop, x_event_source_t *source)
{
    struct kevent changes[2];
    int count = 0;

    if ((source->events & X_EVENT_READ) != 0) {
        EV_SET(&changes[count++], source->native_handle, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }

    if ((source->events & X_EVENT_WRITE) != 0) {
        EV_SET(&changes[count++], source->native_handle, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }

    if (count > 0) {
        kevent(loop->backend_fd, changes, count, NULL, 0, NULL);
    }
}
#else
static int x_event_backend_add_socket(x_event_loop_t *loop, x_event_source_t *source)
{
    (void)loop;
    (void)source;
    return 0;
}

static void x_event_backend_remove_socket(x_event_loop_t *loop, x_event_source_t *source)
{
    (void)loop;
    (void)source;
}
#endif

static int x_event_loop_next_timeout_ms(x_event_loop_t *loop, int *has_timeout, int *timeout_ms)
{
    uint64_t now;
    uint64_t best_delta = UINT64_MAX;
    size_t i;
    int error;

    *has_timeout = 0;
    *timeout_ms = -1;

    error = x_time_monotonic_ns(&now);
    if (error != 0) {
        return error;
    }

    for (i = 0U; i < loop->count; ++i) {
        x_event_source_t *source = loop->sources[i];
        if (!source->active
            || (source->type != X_EVENT_SOURCE_TIMER
                && source->type != X_EVENT_SOURCE_PROCESS
                && source->type != X_EVENT_SOURCE_FILE)) {
            continue;
        }

        *has_timeout = 1;
        if (source->due_ns <= now) {
            *timeout_ms = 0;
            return 0;
        }

        if (source->due_ns - now < best_delta) {
            best_delta = source->due_ns - now;
        }
    }

    if (*has_timeout) {
        uint64_t milliseconds = (best_delta + 999999ULL) / 1000000ULL;
        *timeout_ms = milliseconds > (uint64_t)INT_MAX ? INT_MAX : (int)milliseconds;
    }

    return 0;
}

static int x_event_loop_dispatch_timers(x_event_loop_t *loop)
{
    uint64_t now;
    size_t i;
    int error = x_time_monotonic_ns(&now);

    if (error != 0) {
        return error;
    }

    for (i = 0U; i < loop->count; ++i) {
        x_event_source_t *source = loop->sources[i];

        if (!source->active
            || (source->type != X_EVENT_SOURCE_TIMER
                && source->type != X_EVENT_SOURCE_PROCESS
                && source->type != X_EVENT_SOURCE_FILE)
            || source->due_ns > now) {
            continue;
        }

        if (source->type == X_EVENT_SOURCE_PROCESS) {
            int completed = 0;

            error = x_process_poll(source->process, &completed, NULL);
            if (error != 0) {
                return error;
            }

            if (!completed) {
                source->due_ns = now + source->interval_ns;
                continue;
            }

            error = x_event_source_dispatch(source, X_EVENT_PROCESS);
            if (error != 0) {
                return error;
            }

            if (source->active) {
                x_event_source_remove(source);
            }
            continue;
        }

        if (source->type == X_EVENT_SOURCE_FILE) {
            int pending_events = 0;

            error = x_file_watcher_poll(source->watcher, x_event_file_watch_callback, &pending_events);
            if (error != 0) {
                return error;
            }

            if (pending_events != 0) {
                error = x_event_source_dispatch(source, x_event_file_flags(pending_events));
                if (error != 0) {
                    return error;
                }
            }

            if (source->active) {
                source->due_ns = now + source->interval_ns;
            }
            continue;
        }

        error = x_event_source_dispatch(source, X_EVENT_TIMER);
        if (error != 0) {
            return error;
        }

        if (!source->active) {
            continue;
        }

        if (source->interval_ns == 0U) {
            x_event_source_remove(source);
        } else {
            do {
                source->due_ns += source->interval_ns;
            } while (source->due_ns <= now);
        }
    }

    return 0;
}

static int x_event_loop_wake_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    char buffer[64];
    size_t received;
    x_socket_address_t address;
    x_event_loop_t *owner = (x_event_loop_t *)user_data;

    (void)loop;
    (void)source;
    (void)events;

    for (;;) {
        int error = x_socket_receive_from(owner->wake_receiver, &address, buffer, sizeof(buffer), &received);
        if (error == EWOULDBLOCK || error == EAGAIN) {
            return 0;
        }

        if (error != 0) {
            return error;
        }

        if (received == 0U) {
            return 0;
        }
    }
}

#if defined(XLIB_EVENT_USE_EPOLL)
static int x_event_loop_wait_sockets(x_event_loop_t *loop, int timeout_ms)
{
    struct epoll_event events[32];
    int count;
    int i;

    count = epoll_wait(loop->backend_fd, events, 32, timeout_ms);
    if (count < 0) {
        return errno == EINTR ? 0 : errno;
    }

    for (i = 0; i < count; ++i) {
        int flags = 0;
        x_event_source_t *source = (x_event_source_t *)events[i].data.ptr;

        if ((events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) != 0) {
            flags |= X_EVENT_READ;
        }

        if ((events[i].events & EPOLLOUT) != 0) {
            flags |= X_EVENT_WRITE;
        }

        if (source != NULL && source->active && flags != 0) {
            int error = x_event_source_dispatch(source, flags);
            if (error != 0) {
                return error;
            }
        }
    }

    return 0;
}
#elif defined(XLIB_EVENT_USE_KQUEUE)
static int x_event_loop_wait_sockets(x_event_loop_t *loop, int timeout_ms)
{
    struct kevent events[32];
    struct timespec timeout;
    struct timespec *timeout_pointer = NULL;
    int count;
    int i;

    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        timeout_pointer = &timeout;
    }

    count = kevent(loop->backend_fd, NULL, 0, events, 32, timeout_pointer);
    if (count < 0) {
        return errno == EINTR ? 0 : errno;
    }

    for (i = 0; i < count; ++i) {
        int flags = 0;
        x_event_source_t *source = (x_event_source_t *)events[i].udata;

        if (events[i].filter == EVFILT_READ) {
            flags |= X_EVENT_READ;
        } else if (events[i].filter == EVFILT_WRITE) {
            flags |= X_EVENT_WRITE;
        }

        if (source != NULL && source->active && flags != 0) {
            int error = x_event_source_dispatch(source, flags);
            if (error != 0) {
                return error;
            }
        }
    }

    return 0;
}
#else
static int x_event_loop_wait_sockets(x_event_loop_t *loop, int timeout_ms)
{
    fd_set read_set;
    fd_set write_set;
    struct timeval timeout;
    struct timeval *timeout_pointer = NULL;
    int has_socket = 0;
    int result;
    int max_fd = 0;
    size_t i;

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    for (i = 0U; i < loop->count; ++i) {
        x_event_source_t *source = loop->sources[i];
        if (!source->active || source->type != X_EVENT_SOURCE_SOCKET) {
            continue;
        }

#ifdef _WIN32
        if ((source->events & X_EVENT_READ) != 0) {
            FD_SET((SOCKET)source->native_handle, &read_set);
        }
        if ((source->events & X_EVENT_WRITE) != 0) {
            FD_SET((SOCKET)source->native_handle, &write_set);
        }
#else
        if ((source->events & X_EVENT_READ) != 0) {
            FD_SET((int)source->native_handle, &read_set);
        }
        if ((source->events & X_EVENT_WRITE) != 0) {
            FD_SET((int)source->native_handle, &write_set);
        }
        if ((int)source->native_handle > max_fd) {
            max_fd = (int)source->native_handle;
        }
#endif
        has_socket = 1;
    }

    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (long)(timeout_ms % 1000) * 1000L;
        timeout_pointer = &timeout;
    }

    if (!has_socket) {
        if (timeout_ms < 0) {
            return 0;
        }
        return x_time_sleep_ms((uint64_t)timeout_ms);
    }

    result = select(max_fd + 1, &read_set, &write_set, NULL, timeout_pointer);
    if (result < 0) {
#ifdef _WIN32
        return EIO;
#else
        return errno == EINTR ? 0 : errno;
#endif
    }

    if (result == 0) {
        return 0;
    }

    for (i = 0U; i < loop->count; ++i) {
        x_event_source_t *source = loop->sources[i];
        int flags = 0;

        if (!source->active || source->type != X_EVENT_SOURCE_SOCKET) {
            continue;
        }

#ifdef _WIN32
        if ((source->events & X_EVENT_READ) != 0 && FD_ISSET((SOCKET)source->native_handle, &read_set)) {
            flags |= X_EVENT_READ;
        }
        if ((source->events & X_EVENT_WRITE) != 0 && FD_ISSET((SOCKET)source->native_handle, &write_set)) {
            flags |= X_EVENT_WRITE;
        }
#else
        if ((source->events & X_EVENT_READ) != 0 && FD_ISSET((int)source->native_handle, &read_set)) {
            flags |= X_EVENT_READ;
        }
        if ((source->events & X_EVENT_WRITE) != 0 && FD_ISSET((int)source->native_handle, &write_set)) {
            flags |= X_EVENT_WRITE;
        }
#endif

        if (flags != 0) {
            int error = x_event_source_dispatch(source, flags);
            if (error != 0) {
                return error;
            }
        }
    }

    return 0;
}
#endif

int x_event_loop_create(x_event_loop_t **loop)
{
    x_event_loop_t *created;
    x_socket_address_t bind_address;
    int error;

    if (loop == NULL) {
        return EINVAL;
    }

    *loop = NULL;

    created = (x_event_loop_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#if defined(XLIB_EVENT_USE_EPOLL)
    /* EPOLL_CLOEXEC prevents the epoll fd from being inherited by child
     * processes spawned via x_process_start(). */
    created->backend_fd = epoll_create1(EPOLL_CLOEXEC);
    if (created->backend_fd < 0) {
        int error = errno;
        free(created);
        return error;
    }
#elif defined(XLIB_EVENT_USE_KQUEUE)
    created->backend_fd = kqueue();
    if (created->backend_fd < 0) {
        int error = errno;
        free(created);
        return error;
    }
    /* kqueue() has no CLOEXEC flag; apply it with fcntl. */
    (void)fcntl(created->backend_fd, F_SETFD, FD_CLOEXEC);
#endif

    error = x_socket_udp(&created->wake_receiver);
    if (error != 0) {
#if defined(XLIB_EVENT_USE_EPOLL) || defined(XLIB_EVENT_USE_KQUEUE)
        close(created->backend_fd);
#endif
        free(created);
        return error;
    }

    error = x_socket_udp(&created->wake_sender);
    if (error != 0) {
        x_socket_close(created->wake_receiver);
#if defined(XLIB_EVENT_USE_EPOLL) || defined(XLIB_EVENT_USE_KQUEUE)
        close(created->backend_fd);
#endif
        free(created);
        return error;
    }

    error = x_address_resolve(&bind_address, "127.0.0.1", "0", X_SOCKET_UDP, 1);
    if (error == 0) {
        error = x_socket_bind(created->wake_receiver, &bind_address);
    }
    if (error == 0) {
        error = x_socket_local_address(created->wake_receiver, &created->wake_address);
    }
    if (error == 0) {
        error = x_socket_set_nonblocking(created->wake_receiver, 1);
    }
    if (error == 0) {
        error = x_event_loop_add_socket(
            created,
            &created->wake_source,
            created->wake_receiver,
            X_EVENT_READ,
            x_event_loop_wake_callback,
            created);
    }

    if (error != 0) {
        x_socket_close(created->wake_sender);
        x_socket_close(created->wake_receiver);
#if defined(XLIB_EVENT_USE_EPOLL) || defined(XLIB_EVENT_USE_KQUEUE)
        close(created->backend_fd);
#endif
        free(created->sources);
        free(created);
        return error;
    }

    created->wake_source->internal = 1;
    *loop = created;
    return 0;
}

int x_event_loop_run(x_event_loop_t *loop)
{
    int error = 0;

    if (loop == NULL) {
        return EINVAL;
    }

    loop->running = 1;
    loop->stop_requested = 0;

    while (!loop->stop_requested && x_event_loop_active_user_sources(loop) > 0U) {
        int has_timeout;
        int timeout_ms;

        error = x_event_loop_dispatch_timers(loop);
        if (error != 0 || loop->stop_requested) {
            break;
        }

        x_event_loop_compact(loop);
        if (x_event_loop_active_user_sources(loop) == 0U) {
            break;
        }

        error = x_event_loop_next_timeout_ms(loop, &has_timeout, &timeout_ms);
        if (error != 0) {
            break;
        }

        error = x_event_loop_wait_sockets(loop, has_timeout ? timeout_ms : -1);
        if (error != 0) {
            break;
        }

        error = x_event_loop_dispatch_timers(loop);
        if (error != 0) {
            break;
        }

        x_event_loop_compact(loop);
    }

    loop->running = 0;
    return error;
}

int x_event_loop_wake(x_event_loop_t *loop)
{
    size_t sent = 0U;

    if (loop == NULL || loop->wake_sender == NULL) {
        return EINVAL;
    }

    return x_socket_send_to(loop->wake_sender, &loop->wake_address, "w", 1U, &sent);
}

int x_event_loop_cancel(x_event_loop_t *loop)
{
    if (loop == NULL) {
        return EINVAL;
    }

    loop->stop_requested = 1;
    return x_event_loop_wake(loop);
}

void x_event_loop_stop(x_event_loop_t *loop)
{
    if (loop != NULL) {
        loop->stop_requested = 1;
        (void)x_event_loop_wake(loop);
    }
}

void x_event_loop_destroy(x_event_loop_t *loop)
{
    size_t i;

    if (loop == NULL) {
        return;
    }

    for (i = 0U; i < loop->count; ++i) {
        x_event_source_t *source = loop->sources[i];
        /* Remove active socket sources from the backend before freeing so that
         * epoll/kqueue registrations are properly cleaned up. */
        if (source->active && source->type == X_EVENT_SOURCE_SOCKET) {
            x_event_backend_remove_socket(loop, source);
        }
        free(source);
    }

#if defined(XLIB_EVENT_USE_EPOLL) || defined(XLIB_EVENT_USE_KQUEUE)
    if (loop->backend_fd >= 0) {
        close(loop->backend_fd);
    }
#endif

    if (loop->wake_sender != NULL) {
        x_socket_close(loop->wake_sender);
    }

    if (loop->wake_receiver != NULL) {
        x_socket_close(loop->wake_receiver);
    }

    free(loop->sources);
    free(loop);
}

int x_event_loop_add_socket(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_socket_t *socket,
    int events,
    x_event_callback callback,
    void *user_data)
{
    x_event_source_t *created;
    int error;

    if (loop == NULL || source == NULL || socket == NULL || callback == NULL) {
        return EINVAL;
    }

    if ((events & (X_EVENT_READ | X_EVENT_WRITE)) == 0) {
        return EINVAL;
    }

    *source = NULL;

    error = x_event_loop_reserve(loop, loop->count + 1U);
    if (error != 0) {
        return error;
    }

    created = (x_event_source_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->loop = loop;
    created->type = X_EVENT_SOURCE_SOCKET;
    created->active = 1;
    created->events = events;
    created->socket = socket;
    created->native_handle = x_socket_native_handle(socket);
    created->callback = callback;
    created->user_data = user_data;

    error = x_event_backend_add_socket(loop, created);
    if (error != 0) {
        free(created);
        return error;
    }

    loop->sources[loop->count++] = created;
    *source = created;
    return 0;
}

int x_event_loop_add_timer(
    x_event_loop_t *loop,
    x_event_source_t **source,
    uint64_t delay_ms,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data)
{
    x_event_source_t *created;
    uint64_t now;
    int error;

    if (loop == NULL || source == NULL || callback == NULL) {
        return EINVAL;
    }

    *source = NULL;

    error = x_event_loop_reserve(loop, loop->count + 1U);
    if (error != 0) {
        return error;
    }

    error = x_time_monotonic_ns(&now);
    if (error != 0) {
        return error;
    }

    created = (x_event_source_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->loop = loop;
    created->type = X_EVENT_SOURCE_TIMER;
    created->active = 1;
    created->events = X_EVENT_TIMER;
    created->due_ns = now + x_event_ms_to_ns(delay_ms);
    created->interval_ns = x_event_ms_to_ns(interval_ms);
    created->callback = callback;
    created->user_data = user_data;

    loop->sources[loop->count++] = created;
    *source = created;
    return 0;
}

int x_event_loop_add_process(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_process_t *process,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data)
{
    x_event_source_t *created;
    uint64_t now;
    int error;

    if (loop == NULL || source == NULL || process == NULL || callback == NULL) {
        return EINVAL;
    }

    *source = NULL;

    error = x_event_loop_reserve(loop, loop->count + 1U);
    if (error != 0) {
        return error;
    }

    error = x_time_monotonic_ns(&now);
    if (error != 0) {
        return error;
    }

    created = (x_event_source_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->loop = loop;
    created->type = X_EVENT_SOURCE_PROCESS;
    created->active = 1;
    created->events = X_EVENT_PROCESS;
    created->process = process;
    created->due_ns = now;
    created->interval_ns = x_event_ms_to_ns(interval_ms == 0U ? 10U : interval_ms);
    created->callback = callback;
    created->user_data = user_data;

    loop->sources[loop->count++] = created;
    *source = created;
    return 0;
}

int x_event_loop_add_file_watcher(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_file_watcher_t *watcher,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data)
{
    x_event_source_t *created;
    uint64_t now;
    int error;

    if (loop == NULL || source == NULL || watcher == NULL || callback == NULL) {
        return EINVAL;
    }

    *source = NULL;

    error = x_event_loop_reserve(loop, loop->count + 1U);
    if (error != 0) {
        return error;
    }

    error = x_time_monotonic_ns(&now);
    if (error != 0) {
        return error;
    }

    created = (x_event_source_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->loop = loop;
    created->type = X_EVENT_SOURCE_FILE;
    created->active = 1;
    created->events = X_EVENT_FILE;
    created->watcher = watcher;
    created->due_ns = now;
    created->interval_ns = x_event_ms_to_ns(interval_ms == 0U ? 100U : interval_ms);
    created->callback = callback;
    created->user_data = user_data;

    loop->sources[loop->count++] = created;
    *source = created;
    return 0;
}

void x_event_source_remove(x_event_source_t *source)
{
    if (source == NULL || !source->active) {
        return;
    }

    if (source->type == X_EVENT_SOURCE_SOCKET) {
        x_event_backend_remove_socket(source->loop, source);
    }

    source->active = 0;
}
