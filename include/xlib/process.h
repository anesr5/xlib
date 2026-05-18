#ifndef XLIB_PROCESS_H
#define XLIB_PROCESS_H

#include <stddef.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_process x_process_t;
typedef struct x_pipe x_pipe_t;

typedef struct x_process_options {
    const char *working_directory;
    const char *stdin_path;
    const char *stdout_path;
    const char *stderr_path;
    char *const *environment;
    x_pipe_t *stdin_pipe;
    x_pipe_t *stdout_pipe;
    x_pipe_t *stderr_pipe;
} x_process_options_t;

XLIB_API int x_process_start(
    x_process_t **process,
    const char *path,
    char *const arguments[],
    const x_process_options_t *options);
XLIB_API int x_process_wait(x_process_t *process, int *exit_code);
XLIB_API int x_process_poll(x_process_t *process, int *completed, int *exit_code);
XLIB_API int x_process_terminate(x_process_t *process);
XLIB_API void x_process_destroy(x_process_t *process);

XLIB_API int x_pipe_create(x_pipe_t **read_pipe, x_pipe_t **write_pipe);
XLIB_API int x_pipe_read(x_pipe_t *pipe, void *buffer, size_t size, size_t *bytes_read);
XLIB_API int x_pipe_write(x_pipe_t *pipe, const void *buffer, size_t size, size_t *bytes_written);
XLIB_API void x_pipe_close(x_pipe_t *pipe);

XLIB_API int x_process_run_pipeline(
    const char *first_path,
    char *const first_arguments[],
    const char *second_path,
    char *const second_arguments[],
    const x_process_options_t *options,
    int *exit_code);

XLIB_API int x_environment_get(const char *name, char *buffer, size_t buffer_size);
XLIB_API int x_environment_set(const char *name, const char *value);
XLIB_API int x_environment_unset(const char *name);

#ifdef __cplusplus
}
#endif

#endif
