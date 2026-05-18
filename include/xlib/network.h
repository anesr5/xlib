#ifndef XLIB_NETWORK_H
#define XLIB_NETWORK_H

#include <stddef.h>
#include <stdint.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLIB_SOCKET_ADDRESS_SIZE 128

typedef struct x_socket x_socket_t;

typedef enum x_socket_type {
    X_SOCKET_TCP = 1,
    X_SOCKET_UDP = 2
} x_socket_type_t;

typedef enum x_address_family {
    X_ADDRESS_FAMILY_IPV4 = 4,
    X_ADDRESS_FAMILY_IPV6 = 6
} x_address_family_t;

typedef struct x_socket_address {
    unsigned char storage[XLIB_SOCKET_ADDRESS_SIZE];
    size_t length;
} x_socket_address_t;

/*
 * Resolves host and service into a socket address for the given socket type.
 * host may be NULL when passive is non-zero to request a wildcard address.
 * The address family of the result depends on what the system resolves; use
 * x_address_family to query it.
 */
XLIB_API int x_address_resolve(
    x_socket_address_t *address,
    const char *host,
    const char *service,
    int type,
    int passive);

XLIB_API int x_address_port(const x_socket_address_t *address, uint16_t *port);

/*
 * Returns X_ADDRESS_FAMILY_IPV4 or X_ADDRESS_FAMILY_IPV6 for the given
 * address, or a negative errno-style error code on failure.
 */
XLIB_API int x_address_family(const x_socket_address_t *address);

/*
 * Formats address as a human-readable string (e.g. "192.0.2.1" or
 * "2001:db8::1").  buffer must be at least XLIB_ADDRESS_STRING_MAX bytes.
 */
#define XLIB_ADDRESS_STRING_MAX 64
XLIB_API int x_address_to_string(
    const x_socket_address_t *address,
    char *buffer,
    size_t buffer_size);

/*
 * Parses a numeric host string and port into a socket address.
 * family must be X_ADDRESS_FAMILY_IPV4 or X_ADDRESS_FAMILY_IPV6.
 */
XLIB_API int x_address_from_string(
    x_socket_address_t *address,
    const char *host,
    uint16_t port,
    int family);

/* IPv4 socket factories */
XLIB_API int x_socket_tcp(x_socket_t **socket);
XLIB_API int x_socket_udp(x_socket_t **socket);

/* IPv6 socket factories */
XLIB_API int x_socket_tcp6(x_socket_t **socket);
XLIB_API int x_socket_udp6(x_socket_t **socket);

XLIB_API int x_socket_bind(x_socket_t *socket, const x_socket_address_t *address);
XLIB_API int x_socket_listen(x_socket_t *socket, int backlog);
XLIB_API int x_socket_accept(x_socket_t *socket, x_socket_t **client);
XLIB_API int x_socket_connect(x_socket_t *socket, const x_socket_address_t *address);

XLIB_API int x_socket_send(x_socket_t *socket, const void *buffer, size_t size, size_t *bytes_sent);
XLIB_API int x_socket_receive(x_socket_t *socket, void *buffer, size_t size, size_t *bytes_received);
XLIB_API int x_socket_send_to(
    x_socket_t *socket,
    const x_socket_address_t *address,
    const void *buffer,
    size_t size,
    size_t *bytes_sent);
XLIB_API int x_socket_receive_from(
    x_socket_t *socket,
    x_socket_address_t *address,
    void *buffer,
    size_t size,
    size_t *bytes_received);

XLIB_API int x_socket_local_address(x_socket_t *socket, x_socket_address_t *address);

XLIB_API int x_socket_set_nonblocking(x_socket_t *socket, int enabled);
XLIB_API int x_socket_set_reuse_address(x_socket_t *socket, int enabled);
XLIB_API int x_socket_set_tcp_no_delay(x_socket_t *socket, int enabled);

/*
 * Sets the send or receive timeout on a socket.  A value of 0 disables the
 * timeout.  Returns ETIMEDOUT from send/receive calls that exceed the limit.
 */
XLIB_API int x_socket_set_send_timeout(x_socket_t *socket, uint32_t milliseconds);
XLIB_API int x_socket_set_receive_timeout(x_socket_t *socket, uint32_t milliseconds);

/*
 * Joins or leaves an IPv4 multicast group on the given interface address.
 * Pass NULL for interface_address to use the default interface.
 * The socket must be a UDP socket.
 */
XLIB_API int x_socket_join_multicast_group(
    x_socket_t *socket,
    const char *group_address,
    const char *interface_address);
XLIB_API int x_socket_leave_multicast_group(
    x_socket_t *socket,
    const char *group_address,
    const char *interface_address);

/*
 * Creates a UDP socket, sets SO_REUSEADDR, and binds it to host:service in a
 * single call.  Equivalent to resolving, creating, and binding manually.
 */
XLIB_API int x_socket_udp_bound(
    x_socket_t **socket,
    const char *host,
    const char *service);

XLIB_API uintptr_t x_socket_native_handle(x_socket_t *socket);

XLIB_API void x_socket_close(x_socket_t *socket);

#ifdef __cplusplus
}
#endif

#endif
