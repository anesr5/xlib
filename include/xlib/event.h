#ifndef XLIB_EVENT_H
#define XLIB_EVENT_H

#include <stdint.h>

#include <xlib/filesystem.h>
#include <xlib/network.h>
#include <xlib/process.h>
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
    X_EVENT_FILE_DELETED = 128
} x_event_flags_t;

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

XLIB_API void x_event_source_remove(x_event_source_t *source);

#ifdef __cplusplus
}
#endif

#endif
