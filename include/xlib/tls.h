#ifndef XLIB_TLS_H
#define XLIB_TLS_H

#include <stddef.h>

#include <xlib/network.h>
#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_tls_context x_tls_context_t;

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
