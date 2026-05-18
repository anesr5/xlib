#include <xlib/xlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct watch_state {
    int events;
};

struct async_state {
    int called;
    int error;
    char data[128];
    size_t size;
};

struct process_event_state {
    x_process_t *process;
    int called;
    int exit_code;
};

static int write_text(const char *path, const char *text)
{
    x_file_t *file = NULL;
    size_t written = 0U;
    size_t length = strlen(text);
    int error = x_file_open(&file, path, X_FILE_WRITE | X_FILE_CREATE | X_FILE_TRUNCATE);

    if (error != 0) {
        return error;
    }

    error = x_file_write(file, text, length, &written);
    x_file_close(file);

    return error == 0 && written == length ? 0 : EIO;
}

static int read_text(const char *path, char *buffer, size_t buffer_size)
{
    x_file_t *file = NULL;
    size_t bytes_read = 0U;
    int error;

    if (buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

    buffer[0] = '\0';
    error = x_file_open(&file, path, X_FILE_READ);
    if (error != 0) {
        return error;
    }

    error = x_file_read(file, buffer, buffer_size - 1U, &bytes_read);
    x_file_close(file);
    if (error != 0) {
        return error;
    }

    buffer[bytes_read] = '\0';
    return 0;
}

static int equals_lf_or_crlf(const char *value, const char *lf, const char *crlf)
{
    return strcmp(value, lf) == 0 || strcmp(value, crlf) == 0;
}

static void watch_callback(const char *path, int events, void *user_data)
{
    struct watch_state *state = (struct watch_state *)user_data;

    (void)path;
    state->events |= events;
}

static void async_callback(int error, const void *data, size_t size, void *user_data)
{
    struct async_state *state = (struct async_state *)user_data;
    size_t copy_size = size < sizeof(state->data) - 1U ? size : sizeof(state->data) - 1U;

    state->called = 1;
    state->error = error;
    state->size = size;

    if (data != NULL && copy_size > 0U) {
        memcpy(state->data, data, copy_size);
    }
    state->data[copy_size] = '\0';
}

static int process_event_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct process_event_state *state = (struct process_event_state *)user_data;

    (void)source;

    if ((events & X_EVENT_PROCESS) != 0) {
        int error = x_process_wait(state->process, &state->exit_code);
        if (error != 0) {
            return error;
        }

        state->called = 1;
        x_event_loop_stop(loop);
    }

    return 0;
}

static int file_event_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct watch_state *state = (struct watch_state *)user_data;

    (void)source;
    state->events |= events;

    if ((events & X_EVENT_FILE) != 0) {
        x_event_loop_stop(loop);
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *helper_path;
    const char *watch_path = "xlib_v14_watch.tmp";
    const char *file_event_path = "xlib_v14_file_event.tmp";
    const char *async_path = "xlib_v14_async.tmp";
    const char *env_out_path = "xlib_v14_env.out";
    const char *event_out_path = "xlib_v14_event.out";
    const char *pipeline_in_path = "xlib_v14_pipeline.in";
    const char *pipeline_out_path = "xlib_v14_pipeline.out";
    x_file_watcher_t *watcher = NULL;
    x_file_watcher_t *event_watcher = NULL;
    x_async_file_read_t *async_read = NULL;
    x_process_t *process = NULL;
    x_event_loop_t *loop = NULL;
    struct watch_state watch = {0};
    struct async_state async = {0};
    struct process_event_state process_event;
    char buffer[128];
    int error;
    int exit_code = 0;
    char *env[] = {"XLIB_PROCESS_VALUE=v14-env", NULL};
    char *env_args[] = {"env", "7", NULL};
    char *event_args[] = {"env", "5", NULL};
    char *cat_args[] = {"cat", NULL};
    x_process_options_t options;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <process-helper>\n", argv[0]);
        return 2;
    }
    helper_path = argv[1];

    (void)x_file_remove(watch_path);
    (void)x_file_remove(file_event_path);
    (void)x_file_remove(async_path);
    (void)x_file_remove(env_out_path);
    (void)x_file_remove(event_out_path);
    (void)x_file_remove(pipeline_in_path);
    (void)x_file_remove(pipeline_out_path);

    error = x_file_watcher_create(&watcher, watch_path);
    if (error != 0) {
        return error;
    }

    error = write_text(watch_path, "created");
    if (error == 0) {
        error = x_file_watcher_poll(watcher, watch_callback, &watch);
    }
    if (error != 0 || (watch.events & X_FILE_WATCH_CREATED) == 0) {
        return error != 0 ? error : 3;
    }

    watch.events = 0;
    error = write_text(watch_path, "modified-content");
    if (error == 0) {
        error = x_file_watcher_poll(watcher, watch_callback, &watch);
    }
    if (error != 0 || (watch.events & X_FILE_WATCH_MODIFIED) == 0) {
        return error != 0 ? error : 4;
    }

    watch.events = 0;
    error = x_file_remove(watch_path);
    if (error == 0) {
        error = x_file_watcher_poll(watcher, watch_callback, &watch);
    }
    x_file_watcher_destroy(watcher);
    if (error != 0 || (watch.events & X_FILE_WATCH_DELETED) == 0) {
        return error != 0 ? error : 5;
    }

    watch.events = 0;
    error = x_file_watcher_create(&event_watcher, file_event_path);
    if (error == 0) {
        error = write_text(file_event_path, "event-created");
    }
    if (error == 0) {
        error = x_event_loop_create(&loop);
    }
    if (error == 0) {
        x_event_source_t *source = NULL;
        error = x_event_loop_add_file_watcher(loop, &source, event_watcher, 10U, file_event_callback, &watch);
    }
    if (error == 0) {
        error = x_event_loop_run(loop);
    }
    if (loop != NULL) {
        x_event_loop_destroy(loop);
        loop = NULL;
    }
    if (event_watcher != NULL) {
        x_file_watcher_destroy(event_watcher);
        event_watcher = NULL;
    }
    if (error != 0 || (watch.events & X_EVENT_FILE_CREATED) == 0) {
        return error != 0 ? error : 13;
    }

    error = write_text(async_path, "async-data");
    if (error == 0) {
        error = x_async_file_read_all(&async_read, async_path, async_callback, &async);
    }
    if (error == 0) {
        error = x_async_file_read_wait(async_read);
    }
    if (async_read != NULL) {
        x_async_file_read_destroy(async_read);
    }
    if (error != 0 || !async.called || async.error != 0 || strcmp(async.data, "async-data") != 0) {
        return error != 0 ? error : 6;
    }

    memset(&options, 0, sizeof(options));
    options.stdout_path = env_out_path;
    options.environment = env;
    error = x_process_start(&process, helper_path, env_args, &options);
    if (error == 0) {
        error = x_process_wait(process, &exit_code);
    }
    if (process != NULL) {
        x_process_destroy(process);
        process = NULL;
    }
    if (error != 0 || exit_code != 7) {
        return error != 0 ? error : 7;
    }
    error = read_text(env_out_path, buffer, sizeof(buffer));
    if (error != 0 || !equals_lf_or_crlf(buffer, "v14-env\n", "v14-env\r\n")) {
        return error != 0 ? error : 8;
    }

    process_event.process = NULL;
    process_event.called = 0;
    process_event.exit_code = 0;
    memset(&options, 0, sizeof(options));
    options.stdout_path = event_out_path;
    options.environment = env;
    error = x_process_start(&process_event.process, helper_path, event_args, &options);
    if (error == 0) {
        error = x_event_loop_create(&loop);
    }
    if (error == 0) {
        x_event_source_t *source = NULL;
        error = x_event_loop_add_process(loop, &source, process_event.process, 10U, process_event_callback, &process_event);
    }
    if (error == 0) {
        error = x_event_loop_run(loop);
    }
    if (loop != NULL) {
        x_event_loop_destroy(loop);
        loop = NULL;
    }
    if (process_event.process != NULL) {
        x_process_destroy(process_event.process);
    }
    if (error != 0 || !process_event.called || process_event.exit_code != 5) {
        return error != 0 ? error : 9;
    }

    error = write_text(pipeline_in_path, "pipeline-data\n");
    memset(&options, 0, sizeof(options));
    options.stdin_path = pipeline_in_path;
    options.stdout_path = pipeline_out_path;
    if (error == 0) {
        error = x_process_run_pipeline(helper_path, cat_args, helper_path, cat_args, &options, &exit_code);
    }
    if (error != 0 || exit_code != 0) {
        return error != 0 ? error : 10;
    }
    error = read_text(pipeline_out_path, buffer, sizeof(buffer));
    if (error != 0 || !equals_lf_or_crlf(buffer, "pipeline-data\n", "pipeline-data\r\n")) {
        return error != 0 ? error : 11;
    }

    if (x_terminal_is_tty(1) != 0 && x_terminal_is_tty(1) != 1) {
        return 12;
    }
    {
        int columns = 0;
        int rows = 0;
        int terminal_error = x_terminal_size(1, &columns, &rows);
        if (terminal_error != 0 && terminal_error != ENOTTY) {
            return terminal_error;
        }
    }

    (void)x_file_remove(async_path);
    (void)x_file_remove(file_event_path);
    (void)x_file_remove(env_out_path);
    (void)x_file_remove(event_out_path);
    (void)x_file_remove(pipeline_in_path);
    (void)x_file_remove(pipeline_out_path);

    return 0;
}
