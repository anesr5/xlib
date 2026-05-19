#include <xlib/tls.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef ENOSYS
#define ENOSYS ENOTSUP
#endif

struct x_tls_context {
    x_tls_hooks_t hooks;
    x_socket_t *socket;
    int verify_hostname;
    char *server_name;
    char *certificate_file;
    char *private_key_file;
};

struct x_tls_stream {
    x_tls_context_t *ctx;
    x_socket_t *socket;
    int mode;
    int connected;
};

static char *x_tls_strdup(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value);
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, value, length + 1U);
    }
    return copy;
}

int x_tls_context_create(x_tls_context_t **ctx, const x_tls_hooks_t *hooks)
{
    x_tls_context_t *created;

    if (ctx == NULL || hooks == NULL) {
        return EINVAL;
    }

    if (hooks->connect == NULL || hooks->accept == NULL
        || hooks->read == NULL || hooks->write == NULL
        || hooks->close == NULL) {
        return EINVAL;
    }

    *ctx = NULL;

    created = (x_tls_context_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->hooks = *hooks;
    created->verify_hostname = 1;

    *ctx = created;
    return 0;
}

int x_tls_connect(x_tls_context_t *ctx, x_socket_t *socket, const char *hostname)
{
    int error;

    if (ctx == NULL || socket == NULL || hostname == NULL) {
        return EINVAL;
    }

    ctx->socket = socket;
    error = x_tls_context_set_server_name(ctx, hostname);
    if (error != 0) {
        return error;
    }
    return ctx->hooks.connect(ctx, socket, hostname, ctx->hooks.user_data);
}

int x_tls_accept(x_tls_context_t *ctx, x_socket_t *socket)
{
    if (ctx == NULL || socket == NULL) {
        return EINVAL;
    }

    ctx->socket = socket;
    return ctx->hooks.accept(ctx, socket, ctx->hooks.user_data);
}

int x_tls_read(x_tls_context_t *ctx, void *buffer, size_t size, size_t *bytes_read)
{
    if (ctx == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_read != NULL) {
        *bytes_read = 0U;
    }

    return ctx->hooks.read(ctx, buffer, size, bytes_read, ctx->hooks.user_data);
}

int x_tls_write(x_tls_context_t *ctx, const void *buffer, size_t size, size_t *bytes_written)
{
    if (ctx == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }

    return ctx->hooks.write(ctx, buffer, size, bytes_written, ctx->hooks.user_data);
}

int x_tls_handshake(x_tls_context_t *ctx)
{
    if (ctx == NULL) {
        return EINVAL;
    }

    if (ctx->hooks.handshake == NULL) {
        return 0;
    }

    return ctx->hooks.handshake(ctx, ctx->hooks.user_data);
}

int x_tls_context_load_certificate_file(x_tls_context_t *ctx, const char *path)
{
    char *copy;
    if (ctx == NULL || path == NULL) {
        return EINVAL;
    }
    if (ctx->hooks.load_certificate_file != NULL) {
        return ctx->hooks.load_certificate_file(ctx, path, ctx->hooks.user_data);
    }
    copy = x_tls_strdup(path);
    if (copy == NULL) {
        return ENOMEM;
    }
    free(ctx->certificate_file);
    ctx->certificate_file = copy;
    return 0;
}

int x_tls_context_load_private_key_file(x_tls_context_t *ctx, const char *path)
{
    char *copy;
    if (ctx == NULL || path == NULL) {
        return EINVAL;
    }
    if (ctx->hooks.load_private_key_file != NULL) {
        return ctx->hooks.load_private_key_file(ctx, path, ctx->hooks.user_data);
    }
    copy = x_tls_strdup(path);
    if (copy == NULL) {
        return ENOMEM;
    }
    free(ctx->private_key_file);
    ctx->private_key_file = copy;
    return 0;
}

int x_tls_context_set_server_name(x_tls_context_t *ctx, const char *server_name)
{
    char *copy;
    if (ctx == NULL || server_name == NULL) {
        return EINVAL;
    }
    if (ctx->hooks.set_server_name != NULL) {
        return ctx->hooks.set_server_name(ctx, server_name, ctx->hooks.user_data);
    }
    copy = x_tls_strdup(server_name);
    if (copy == NULL) {
        return ENOMEM;
    }
    free(ctx->server_name);
    ctx->server_name = copy;
    return 0;
}

int x_tls_context_set_verify_hostname(x_tls_context_t *ctx, int enabled)
{
    if (ctx == NULL) {
        return EINVAL;
    }
    ctx->verify_hostname = enabled ? 1 : 0;
    if (ctx->hooks.set_verify_hostname != NULL) {
        return ctx->hooks.set_verify_hostname(ctx, ctx->verify_hostname, ctx->hooks.user_data);
    }
    return 0;
}

static int x_tls_ascii_equal_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return a == b;
}

static int x_tls_string_equal_ci(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (!x_tls_ascii_equal_ci(*left, *right)) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

int x_tls_hostname_matches(const char *pattern, const char *hostname)
{
    const char *suffix;
    const char *host_suffix;
    size_t suffix_length;
    size_t hostname_length;

    if (pattern == NULL || hostname == NULL || pattern[0] == '\0' || hostname[0] == '\0') {
        return 0;
    }

    if (pattern[0] != '*') {
        return x_tls_string_equal_ci(pattern, hostname);
    }

    if (pattern[1] != '.') {
        return 0;
    }

    suffix = pattern + 1;
    suffix_length = strlen(suffix);
    hostname_length = strlen(hostname);
    if (hostname_length <= suffix_length) {
        return 0;
    }

    host_suffix = hostname + hostname_length - suffix_length;
    if (!x_tls_string_equal_ci(suffix, host_suffix)) {
        return 0;
    }

    return strchr(hostname, '.') == host_suffix;
}

int x_tls_builtin_backend_available(void)
{
    return 0;
}

int x_tls_context_create_builtin(x_tls_context_t **ctx)
{
    if (ctx == NULL) {
        return EINVAL;
    }
    *ctx = NULL;
    return ENOSYS;
}

int x_tls_stream_create(x_tls_stream_t **stream, x_tls_context_t *ctx, x_socket_t *socket, int mode)
{
    x_tls_stream_t *created;
    if (stream == NULL || ctx == NULL || socket == NULL
        || (mode != X_TLS_MODE_CLIENT && mode != X_TLS_MODE_SERVER)) {
        return EINVAL;
    }
    *stream = NULL;
    created = (x_tls_stream_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }
    created->ctx = ctx;
    created->socket = socket;
    created->mode = mode;
    *stream = created;
    return 0;
}

int x_tls_stream_connect(x_tls_stream_t *stream, const char *hostname)
{
    int error;
    if (stream == NULL || hostname == NULL || stream->mode != X_TLS_MODE_CLIENT) {
        return EINVAL;
    }
    error = x_tls_connect(stream->ctx, stream->socket, hostname);
    if (error == 0) {
        stream->connected = 1;
    }
    return error;
}

int x_tls_stream_accept(x_tls_stream_t *stream)
{
    int error;
    if (stream == NULL || stream->mode != X_TLS_MODE_SERVER) {
        return EINVAL;
    }
    error = x_tls_accept(stream->ctx, stream->socket);
    if (error == 0) {
        stream->connected = 1;
    }
    return error;
}

int x_tls_stream_handshake(x_tls_stream_t *stream)
{
    int error;
    if (stream == NULL) {
        return EINVAL;
    }
    error = x_tls_handshake(stream->ctx);
    if (error == 0) {
        stream->connected = 1;
    }
    return error;
}

int x_tls_stream_read(x_tls_stream_t *stream, void *buffer, size_t size, size_t *bytes_read)
{
    if (stream == NULL) {
        return EINVAL;
    }
    return x_tls_read(stream->ctx, buffer, size, bytes_read);
}

int x_tls_stream_write(x_tls_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written)
{
    if (stream == NULL) {
        return EINVAL;
    }
    return x_tls_write(stream->ctx, buffer, size, bytes_written);
}

void x_tls_stream_close(x_tls_stream_t *stream)
{
    if (stream != NULL) {
        x_tls_close(stream->ctx);
        stream->connected = 0;
    }
}

void x_tls_stream_destroy(x_tls_stream_t *stream)
{
    free(stream);
}

void x_tls_close(x_tls_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->hooks.close(ctx, ctx->hooks.user_data);
    ctx->socket = NULL;
}

void x_tls_context_destroy(x_tls_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    free(ctx->server_name);
    free(ctx->certificate_file);
    free(ctx->private_key_file);
    free(ctx);
}
