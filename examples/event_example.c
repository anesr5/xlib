#include <xlib/event.h>
#include <xlib/network.h>
#include <xlib/process.h>
#include <xlib/thread.h>
#include <xlib/time.h>

#include <stdio.h>
#include <string.h>

struct event_state {
    x_socket_t *server;
    x_socket_t *client;
    int timer_fired;
    int repeat_count;
    int server_done;
    int client_done;
};

static void maybe_stop(x_event_loop_t *loop, struct event_state *state)
{
    if (state->timer_fired && state->repeat_count >= 3 && state->server_done && state->client_done) {
        x_event_loop_stop(loop);
    }
}

static int timer_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct event_state *state = (struct event_state *)user_data;

    (void)events;

    state->timer_fired = 1;
    x_event_source_remove(source);
    maybe_stop(loop, state);
    return 0;
}

static int repeat_timer_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct event_state *state = (struct event_state *)user_data;

    (void)events;

    ++state->repeat_count;
    if (state->repeat_count >= 3) {
        x_event_source_remove(source);
    }

    maybe_stop(loop, state);
    return 0;
}

static int server_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct event_state *state = (struct event_state *)user_data;
    x_socket_t *accepted;
    char buffer[16];
    size_t actual;

    (void)events;

    if (x_socket_accept(state->server, &accepted) != 0) {
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive(accepted, buffer, sizeof(buffer) - 1U, &actual) != 0
        || actual != 4U
        || strcmp(buffer, "ping") != 0) {
        x_socket_close(accepted);
        return 1;
    }

    if (x_socket_send(accepted, "pong", 4U, &actual) != 0 || actual != 4U) {
        x_socket_close(accepted);
        return 1;
    }

    x_socket_close(accepted);
    state->server_done = 1;
    x_event_source_remove(source);
    maybe_stop(loop, state);
    return 0;
}

static int client_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct event_state *state = (struct event_state *)user_data;
    char buffer[16];
    size_t actual;

    (void)events;

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive(state->client, buffer, sizeof(buffer) - 1U, &actual) != 0
        || actual != 4U
        || strcmp(buffer, "pong") != 0) {
        return 1;
    }

    state->client_done = 1;
    x_event_source_remove(source);
    maybe_stop(loop, state);
    return 0;
}

static int cancel_thread(void *data)
{
    x_event_loop_t *loop = (x_event_loop_t *)data;

    x_time_sleep_ms(50U);
    return x_event_loop_cancel(loop);
}

static int never_timer_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    (void)loop;
    (void)source;
    (void)events;
    (void)user_data;
    return 1;
}

static int run_cancel_check(void)
{
    x_event_loop_t *loop;
    x_event_source_t *source;
    x_thread_t *thread;
    int result = 1;

    if (x_event_loop_create(&loop) != 0) {
        return 1;
    }

    if (x_event_loop_add_timer(loop, &source, 5000U, 0U, never_timer_callback, NULL) != 0
        || x_event_source_type(source) != X_EVENT_SOURCE_TIMER) {
        x_event_loop_destroy(loop);
        return 1;
    }

    if (x_thread_create(&thread, cancel_thread, loop) != 0) {
        x_event_loop_destroy(loop);
        return 1;
    }

    if (x_event_loop_run(loop) != 0) {
        x_thread_join(thread, NULL);
        x_thread_destroy(thread);
        x_event_loop_destroy(loop);
        return 1;
    }

    if (x_thread_join(thread, &result) != 0 || result != 0) {
        x_thread_destroy(thread);
        x_event_loop_destroy(loop);
        return 1;
    }

    x_thread_destroy(thread);
    x_event_loop_destroy(loop);
    return 0;
}

struct file_io_state {
    x_file_t *file;
    int done;
};

static int file_io_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct file_io_state *state = (struct file_io_state *)user_data;
    size_t actual = 0U;

    if ((events & X_EVENT_WRITE) == 0) {
        return 1;
    }

    if (x_file_write(state->file, "ok", 2U, &actual) != 0 || actual != 2U) {
        return 1;
    }

    state->done = 1;
    x_event_source_remove(source);
    x_event_loop_stop(loop);
    return 0;
}

static int run_file_io_check(void)
{
    x_event_loop_t *loop;
    x_event_source_t *source;
    x_file_t *file;
    struct file_io_state state;

    x_file_remove("xlib_event_file_io.tmp");

    if (x_event_loop_create(&loop) != 0) {
        return 1;
    }

    if (x_file_open(&file, "xlib_event_file_io.tmp", X_FILE_WRITE | X_FILE_CREATE | X_FILE_TRUNCATE) != 0) {
        x_event_loop_destroy(loop);
        return 1;
    }

    state.file = file;
    state.done = 0;

    if (x_event_loop_add_file(loop, &source, file, X_EVENT_WRITE, 1U, file_io_callback, &state) != 0
        || x_event_source_type(source) != X_EVENT_SOURCE_FILE_IO
        || x_event_loop_run(loop) != 0
        || !state.done) {
        x_file_close(file);
        x_file_remove("xlib_event_file_io.tmp");
        x_event_loop_destroy(loop);
        return 1;
    }

    x_file_close(file);
    x_file_remove("xlib_event_file_io.tmp");
    x_event_loop_destroy(loop);
    return 0;
}

struct pipe_state {
    x_pipe_t *read_pipe;
    int done;
};

static int pipe_callback(
    x_event_loop_t *loop,
    x_event_source_t *source,
    int events,
    void *user_data)
{
    struct pipe_state *state = (struct pipe_state *)user_data;
    char buffer[8];
    size_t actual = 0U;

    if ((events & X_EVENT_READ) == 0) {
        return 1;
    }

    if (x_pipe_read(state->read_pipe, buffer, sizeof(buffer), &actual) != 0 || actual != 2U
        || buffer[0] != 'i' || buffer[1] != 'o') {
        return 1;
    }

    state->done = 1;
    x_event_source_remove(source);
    x_event_loop_stop(loop);
    return 0;
}

static int run_pipe_check(void)
{
    x_event_loop_t *loop;
    x_event_source_t *source;
    x_pipe_t *read_pipe;
    x_pipe_t *write_pipe;
    struct pipe_state state;
    size_t actual = 0U;

    if (x_event_loop_create(&loop) != 0) {
        return 1;
    }

    if (x_pipe_create(&read_pipe, &write_pipe) != 0) {
        x_event_loop_destroy(loop);
        return 1;
    }

    state.read_pipe = read_pipe;
    state.done = 0;

    if (x_event_loop_add_pipe(loop, &source, read_pipe, X_EVENT_READ, 1U, pipe_callback, &state) != 0
        || x_event_source_type(source) != X_EVENT_SOURCE_PIPE
        || x_pipe_write(write_pipe, "io", 2U, &actual) != 0
        || actual != 2U
        || x_event_loop_run(loop) != 0
        || !state.done) {
        x_pipe_close(write_pipe);
        x_pipe_close(read_pipe);
        x_event_loop_destroy(loop);
        return 1;
    }

    x_pipe_close(write_pipe);
    x_pipe_close(read_pipe);
    x_event_loop_destroy(loop);
    return 0;
}

static int run_socket_and_timer_check(void)
{
    struct event_state state;
    x_event_loop_t *loop;
    x_event_source_t *server_source;
    x_event_source_t *client_source;
    x_event_source_t *timer_source;
    x_event_source_t *repeat_source;
    x_socket_address_t bind_address;
    x_socket_address_t local_address;
    uint16_t port;
    char port_text[16];
    size_t actual;

    memset(&state, 0, sizeof(state));

    if (x_event_loop_create(&loop) != 0) {
        return 1;
    }

    if (x_socket_tcp(&state.server) != 0 || x_socket_tcp(&state.client) != 0) {
        x_event_loop_destroy(loop);
        return 1;
    }

    x_socket_set_reuse_address(state.server, 1);
    x_socket_set_nonblocking(state.server, 1);

    if (x_address_resolve(&bind_address, "127.0.0.1", "0", X_SOCKET_TCP, 0) != 0
        || x_socket_bind(state.server, &bind_address) != 0
        || x_socket_listen(state.server, 1) != 0
        || x_socket_local_address(state.server, &local_address) != 0
        || x_address_port(&local_address, &port) != 0) {
        x_socket_close(state.client);
        x_socket_close(state.server);
        x_event_loop_destroy(loop);
        return 1;
    }

    snprintf(port_text, sizeof(port_text), "%u", (unsigned int)port);
    if (x_address_resolve(&local_address, "127.0.0.1", port_text, X_SOCKET_TCP, 0) != 0
        || x_socket_connect(state.client, &local_address) != 0
        || x_socket_send(state.client, "ping", 4U, &actual) != 0
        || actual != 4U) {
        x_socket_close(state.client);
        x_socket_close(state.server);
        x_event_loop_destroy(loop);
        return 1;
    }

    if (x_event_loop_add_socket(loop, &server_source, state.server, X_EVENT_READ, server_callback, &state) != 0
        || x_event_loop_add_socket(loop, &client_source, state.client, X_EVENT_READ, client_callback, &state) != 0
        || x_event_loop_add_timer(loop, &timer_source, 1U, 0U, timer_callback, &state) != 0
        || x_event_loop_add_timer(loop, &repeat_source, 1U, 5U, repeat_timer_callback, &state) != 0
        || x_event_source_type(server_source) != X_EVENT_SOURCE_SOCKET
        || x_event_source_type(client_source) != X_EVENT_SOURCE_SOCKET
        || x_event_source_type(timer_source) != X_EVENT_SOURCE_TIMER
        || x_event_source_type(repeat_source) != X_EVENT_SOURCE_TIMER) {
        x_socket_close(state.client);
        x_socket_close(state.server);
        x_event_loop_destroy(loop);
        return 1;
    }

    if (x_event_loop_run(loop) != 0) {
        x_socket_close(state.client);
        x_socket_close(state.server);
        x_event_loop_destroy(loop);
        return 1;
    }

    x_socket_close(state.client);
    x_socket_close(state.server);
    x_event_loop_destroy(loop);

    if (!state.timer_fired || state.repeat_count < 3 || !state.server_done || !state.client_done) {
        return 1;
    }

    return 0;
}

int main(void)
{
    if (run_socket_and_timer_check() != 0
        || run_file_io_check() != 0
        || run_pipe_check() != 0
        || run_cancel_check() != 0) {
        return 1;
    }

    printf("Event example passed.\n");
    return 0;
}
