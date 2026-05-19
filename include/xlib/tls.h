#ifndef XLIB_TLS_H
#define XLIB_TLS_H

#include <stddef.h>

#include <xlib/network.h>
#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_tls_context x_tls_context_t;
typedef struct x_tls_stream x_tls_stream_t;

typedef enum x_tls_mode {
    X_TLS_MODE_CLIENT = 1,
    X_TLS_MODE_SERVER = 2
} x_tls_mode_t;

/*
 * Pluggable TLS hooks vtable.  xlib does not ship a TLS implementation; it
 * provides this abstraction so that callers can slot in any backend (e.g.
 * mbedTLS, BearSSL, OpenSSL) without changing the rest of the code.
 *
 * All function pointers must be non-NULL when passed to x_tls_context_create.
 * user_data is forwarded unchanged to every call.
 */
typedef struct x_tls_hooks {
    /*
     * Perform a TLS client handshake over socket, verifying hostname.
     * Returns 0 on success or an errno-style error code on failure.
     */
    int (*connect)(x_tls_context_t *ctx, x_socket_t *socket,
                   const char *hostname, void *user_data);

    /*
     * Perform a TLS server handshake over socket.
     * Returns 0 on success or an errno-style error code on failure.
     */
    int (*accept)(x_tls_context_t *ctx, x_socket_t *socket, void *user_data);

    /*
     * Read up to size decrypted bytes into buffer.
     * Sets *bytes_read to the number of bytes read.
     * Returns 0 on success or an errno-style error code on failure.
     */
    int (*read)(x_tls_context_t *ctx, void *buffer, size_t size,
                size_t *bytes_read, void *user_data);

    /*
     * Write size bytes from buffer.
     * Sets *bytes_written to the number of bytes consumed.
     * Returns 0 on success or an errno-style error code on failure.
     */
    int (*write)(x_tls_context_t *ctx, const void *buffer, size_t size,
                 size_t *bytes_written, void *user_data);

    /*
     * Optional non-blocking handshake step. Return 0 when complete,
     * EAGAIN/EWOULDBLOCK while pending, or another errno-style error.
     */
    int (*handshake)(x_tls_context_t *ctx, void *user_data);

    int (*load_certificate_file)(x_tls_context_t *ctx, const char *path, void *user_data);
    int (*load_private_key_file)(x_tls_context_t *ctx, const char *path, void *user_data);
    int (*set_server_name)(x_tls_context_t *ctx, const char *server_name, void *user_data);
    int (*set_verify_hostname)(x_tls_context_t *ctx, int enabled, void *user_data);

    /*
     * Perform a graceful TLS shutdown and release any backend state.
     * The underlying socket is NOT closed by this call.
     */
    void (*close)(x_tls_context_t *ctx, void *user_data);

    void *user_data;
} x_tls_hooks_t;

/*
 * Creates a TLS context backed by hooks.  The x_tls_hooks_t struct is copied
 * internally; the caller does not need to keep it alive.
 * Destroy with x_tls_context_destroy.
 */
XLIB_API int x_tls_context_create(x_tls_context_t **ctx, const x_tls_hooks_t *hooks);

/*
 * Perform a TLS client handshake over socket, verifying hostname.
 */
XLIB_API int x_tls_connect(x_tls_context_t *ctx, x_socket_t *socket, const char *hostname);

/*
 * Perform a TLS server handshake over socket.
 */
XLIB_API int x_tls_accept(x_tls_context_t *ctx, x_socket_t *socket);

XLIB_API int x_tls_read(x_tls_context_t *ctx, void *buffer, size_t size, size_t *bytes_read);
XLIB_API int x_tls_write(x_tls_context_t *ctx, const void *buffer, size_t size, size_t *bytes_written);
XLIB_API int x_tls_handshake(x_tls_context_t *ctx);
XLIB_API int x_tls_context_load_certificate_file(x_tls_context_t *ctx, const char *path);
XLIB_API int x_tls_context_load_private_key_file(x_tls_context_t *ctx, const char *path);
XLIB_API int x_tls_context_set_server_name(x_tls_context_t *ctx, const char *server_name);
XLIB_API int x_tls_context_set_verify_hostname(x_tls_context_t *ctx, int enabled);
XLIB_API int x_tls_hostname_matches(const char *pattern, const char *hostname);
XLIB_API int x_tls_builtin_backend_available(void);
XLIB_API int x_tls_context_create_builtin(x_tls_context_t **ctx);

XLIB_API int x_tls_stream_create(x_tls_stream_t **stream, x_tls_context_t *ctx, x_socket_t *socket, int mode);
XLIB_API int x_tls_stream_connect(x_tls_stream_t *stream, const char *hostname);
XLIB_API int x_tls_stream_accept(x_tls_stream_t *stream);
XLIB_API int x_tls_stream_handshake(x_tls_stream_t *stream);
XLIB_API int x_tls_stream_read(x_tls_stream_t *stream, void *buffer, size_t size, size_t *bytes_read);
XLIB_API int x_tls_stream_write(x_tls_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written);
XLIB_API void x_tls_stream_close(x_tls_stream_t *stream);
XLIB_API void x_tls_stream_destroy(x_tls_stream_t *stream);

/*
 * Performs a graceful TLS shutdown.  Does not close the underlying socket.
 */
XLIB_API void x_tls_close(x_tls_context_t *ctx);

/*
 * Releases the x_tls_context_t allocation.  x_tls_close must be called
 * first if a handshake was completed.
 */
XLIB_API void x_tls_context_destroy(x_tls_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
