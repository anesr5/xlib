#ifndef XLIB_EVENT_H
#define XLIB_EVENT_H

#include <stdint.h>

#include <xlib/filesystem.h>
#include <xlib/network.h>
#include <xlib/process.h>
#include <xlib/tls.h>
#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_event_loop x_event_loop_t;
typedef struct x_event_source x_event_source_t;

typedef enum x_event_flags {
    X_EVENT_READ = 1,
    X_EVENT_WRITE = 2,
    X_EVENT_TIMER = 4,
    X_EVENT_PROCESS = 8,
    X_EVENT_FILE = 16,
    X_EVENT_FILE_CREATED = 32,
    X_EVENT_FILE_MODIFIED = 64,
    X_EVENT_FILE_DELETED = 128,
    X_EVENT_SIGNAL = 256,
    X_EVENT_TLS = 512
} x_event_flags_t;

typedef enum x_event_source_type {
    X_EVENT_SOURCE_UNKNOWN = 0,
    X_EVENT_SOURCE_SOCKET = 1,
    X_EVENT_SOURCE_TIMER = 2,
    X_EVENT_SOURCE_PROCESS = 3,
    X_EVENT_SOURCE_FILE_WATCHER = 4,
    X_EVENT_SOURCE_PIPE = 5,
    X_EVENT_SOURCE_SIGNAL = 6,
    X_EVENT_SOURCE_FILE_IO = 7,
    X_EVENT_SOURCE_TLS_HANDSHAKE = 8
} x_event_source_type_t;

typedef int (*x_event_callback)(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data);

XLIB_API int x_event_loop_create(x_event_loop_t **loop);
XLIB_API int x_event_loop_run(x_event_loop_t *loop);
XLIB_API int x_event_loop_wake(x_event_loop_t *loop);
XLIB_API int x_event_loop_cancel(x_event_loop_t *loop);
XLIB_API void x_event_loop_stop(x_event_loop_t *loop);
XLIB_API void x_event_loop_destroy(x_event_loop_t *loop);

XLIB_API int x_event_loop_add_socket(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_socket_t *socket,
    int events,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_timer(
    x_event_loop_t *loop,
    x_event_source_t **source,
    uint64_t delay_ms,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_process(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_process_t *process,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_file_watcher(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_file_watcher_t *watcher,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_file(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_file_t *file,
    int events,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_pipe(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_pipe_t *pipe,
    int events,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_signal(
    x_event_loop_t *loop,
    x_event_source_t **source,
    int signal_number,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_loop_add_tls_handshake(
    x_event_loop_t *loop,
    x_event_source_t **source,
    x_tls_stream_t *stream,
    uint64_t interval_ms,
    x_event_callback callback,
    void *user_data);

XLIB_API int x_event_source_type(const x_event_source_t *source);
XLIB_API void x_event_source_remove(x_event_source_t *source);

#ifdef __cplusplus
}
#endif

#endif
