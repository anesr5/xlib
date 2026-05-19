#include <xlib/event.h>
#include <xlib/network.h>
#include <xlib/tls.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct tls_state {
    int handshake_calls;
    int event_done;
    char buffer[64];
    size_t size;
};

static int fake_connect(x_tls_context_t *ctx, x_socket_t *socket, const char *hostname, void *user_data)
{
    (void)ctx;
    (void)socket;
    (void)hostname;
    (void)user_data;
    return 0;
}

static int fake_accept(x_tls_context_t *ctx, x_socket_t *socket, void *user_data)
{
    (void)ctx;
    (void)socket;
    (void)user_data;
    return 0;
}

static int fake_handshake(x_tls_context_t *ctx, void *user_data)
{
    struct tls_state *state = (struct tls_state *)user_data;
    (void)ctx;
    ++state->handshake_calls;
    return state->handshake_calls == 1 ? EAGAIN : 0;
}

static int fake_read(x_tls_context_t *ctx, void *buffer, size_t size, size_t *bytes_read, void *user_data)
{
    struct tls_state *state = (struct tls_state *)user_data;
    size_t copied = state->size < size ? state->size : size;
    (void)ctx;
    memcpy(buffer, state->buffer, copied);
    if (bytes_read != NULL) {
        *bytes_read = copied;
    }
    return 0;
}

static int fake_write(x_tls_context_t *ctx, const void *buffer, size_t size, size_t *bytes_written, void *user_data)
{
    struct tls_state *state = (struct tls_state *)user_data;
    (void)ctx;
    if (size > sizeof(state->buffer)) {
        return EMSGSIZE;
    }
    memcpy(state->buffer, buffer, size);
    state->size = size;
    if (bytes_written != NULL) {
        *bytes_written = size;
    }
    return 0;
}

static void fake_close(x_tls_context_t *ctx, void *user_data)
{
    (void)ctx;
    (void)user_data;
}

static int tls_event_callback(x_event_loop_t *loop, x_event_source_t *source, int events, void *user_data)
{
    struct tls_state *state = (struct tls_state *)user_data;
    if ((events & X_EVENT_TLS) == 0 || x_event_source_type(source) != X_EVENT_SOURCE_TLS_HANDSHAKE) {
        return 1;
    }
    state->event_done = 1;
    x_event_loop_stop(loop);
    return 0;
}

int main(void)
{
    struct tls_state state;
    x_tls_hooks_t hooks;
    x_tls_context_t *ctx = NULL;
    x_tls_stream_t *stream = NULL;
    x_socket_t *socket = NULL;
    x_event_loop_t *loop = NULL;
    x_event_source_t *source = NULL;
    char buffer[16];
    size_t actual = 0U;

    memset(&state, 0, sizeof(state));
    memset(&hooks, 0, sizeof(hooks));
    hooks.connect = fake_connect;
    hooks.accept = fake_accept;
    hooks.read = fake_read;
    hooks.write = fake_write;
    hooks.handshake = fake_handshake;
    hooks.close = fake_close;
    hooks.user_data = &state;

    if (!x_tls_hostname_matches("*.example.com", "www.example.com")
        || x_tls_hostname_matches("*.example.com", "deep.www.example.com")) {
        return 1;
    }

    if (x_tls_builtin_backend_available() != 0) {
        return 1;
    }

    if (x_tls_context_create(&ctx, &hooks) != 0
        || x_tls_context_load_certificate_file(ctx, "cert.pem") != 0
        || x_tls_context_load_private_key_file(ctx, "key.pem") != 0
        || x_tls_context_set_verify_hostname(ctx, 1) != 0
        || x_tls_context_set_server_name(ctx, "www.example.com") != 0
        || x_socket_udp(&socket) != 0
        || x_tls_stream_create(&stream, ctx, socket, X_TLS_MODE_CLIENT) != 0
        || x_tls_stream_connect(stream, "www.example.com") != 0
        || x_tls_stream_write(stream, "tls", 3U, &actual) != 0
        || actual != 3U
        || x_tls_stream_read(stream, buffer, sizeof(buffer), &actual) != 0
        || actual != 3U
        || memcmp(buffer, "tls", 3U) != 0
        || x_event_loop_create(&loop) != 0
        || x_event_loop_add_tls_handshake(loop, &source, stream, 1U, tls_event_callback, &state) != 0
        || x_event_loop_run(loop) != 0
        || !state.event_done) {
        if (loop != NULL) x_event_loop_destroy(loop);
        if (stream != NULL) x_tls_stream_destroy(stream);
        if (ctx != NULL) x_tls_context_destroy(ctx);
        if (socket != NULL) x_socket_close(socket);
        return 1;
    }

    x_event_loop_destroy(loop);
    x_tls_stream_close(stream);
    x_tls_stream_destroy(stream);
    x_tls_context_destroy(ctx);
    x_socket_close(socket);
    printf("TLS example passed.\n");
    return 0;
}
