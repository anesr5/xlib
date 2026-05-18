#include <xlib/tls.h>

#include <errno.h>
#include <stdlib.h>

struct x_tls_context {
    x_tls_hooks_t hooks;
    x_socket_t *socket;
};

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

    created->hooks  = *hooks;
    created->socket = NULL;

    *ctx = created;
    return 0;
}

int x_tls_connect(x_tls_context_t *ctx, x_socket_t *socket, const char *hostname)
{
    if (ctx == NULL || socket == NULL || hostname == NULL) {
        return EINVAL;
    }

    ctx->socket = socket;
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

int x_tls_write(
    x_tls_context_t *ctx,
    const void *buffer,
    size_t size,
    size_t *bytes_written)
{
    if (ctx == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }

    return ctx->hooks.write(ctx, buffer, size, bytes_written, ctx->hooks.user_data);
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

    free(ctx);
}
