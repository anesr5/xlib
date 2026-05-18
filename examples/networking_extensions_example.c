#include <xlib/network.h>
#include <xlib/tls.h>

#include <stdio.h>
#include <string.h>

/* --- Address formatting and parsing --------------------------------------- */

static int address_formatting_example(void)
{
    x_socket_address_t address;
    char buffer[XLIB_ADDRESS_STRING_MAX];
    int error;

    printf("Address formatting and parsing...\n");

    error = x_address_from_string(&address, "127.0.0.1", 8080, X_ADDRESS_FAMILY_IPV4);
    if (error != 0) {
        fprintf(stderr, "x_address_from_string (IPv4) failed: %d\n", error);
        return 1;
    }

    error = x_address_to_string(&address, buffer, sizeof(buffer));
    if (error != 0) {
        fprintf(stderr, "x_address_to_string (IPv4) failed: %d\n", error);
        return 1;
    }

    printf("  IPv4 round-trip: %s\n", buffer);

    error = x_address_from_string(&address, "::1", 8080, X_ADDRESS_FAMILY_IPV6);
    if (error != 0) {
        fprintf(stderr, "x_address_from_string (IPv6) failed: %d\n", error);
        return 1;
    }

    error = x_address_to_string(&address, buffer, sizeof(buffer));
    if (error != 0) {
        fprintf(stderr, "x_address_to_string (IPv6) failed: %d\n", error);
        return 1;
    }

    printf("  IPv6 round-trip: %s\n", buffer);
    printf("  address formatting OK\n\n");
    return 0;
}

/* --- Address resolve and family query ------------------------------------- */

static int address_family_example(void)
{
    x_socket_address_t address;
    int family;
    int error;

    printf("Address family detection...\n");

    error = x_address_resolve(&address, "127.0.0.1", "8080", X_SOCKET_TCP, 0);
    if (error != 0) {
        fprintf(stderr, "x_address_resolve failed: %d\n", error);
        return 1;
    }

    family = x_address_family(&address);
    printf("  resolved family: %s\n",
           family == X_ADDRESS_FAMILY_IPV6 ? "IPv6" : "IPv4");

    printf("  address family OK\n\n");
    return 0;
}

/* --- Socket timeout ------------------------------------------------------- */

static int socket_timeout_example(void)
{
    x_socket_t *socket = NULL;
    int error;

    printf("Socket timeouts...\n");

    error = x_socket_tcp(&socket);
    if (error != 0) {
        fprintf(stderr, "x_socket_tcp failed: %d\n", error);
        return 1;
    }

    error = x_socket_set_receive_timeout(socket, 100);
    if (error != 0) {
        fprintf(stderr, "x_socket_set_receive_timeout failed: %d\n", error);
        x_socket_close(socket);
        return 1;
    }

    error = x_socket_set_send_timeout(socket, 100);
    if (error != 0) {
        fprintf(stderr, "x_socket_set_send_timeout failed: %d\n", error);
        x_socket_close(socket);
        return 1;
    }

    x_socket_close(socket);
    printf("  socket timeout OK\n\n");
    return 0;
}

/* --- IPv6 socket factory -------------------------------------------------- */

static int ipv6_socket_example(void)
{
    x_socket_t *tcp6 = NULL;
    x_socket_t *udp6 = NULL;
    int error;

    printf("IPv6 socket factories...\n");

    error = x_socket_tcp6(&tcp6);
    if (error != 0) {
        fprintf(stderr, "x_socket_tcp6 failed: %d\n", error);
        return 1;
    }

    error = x_socket_udp6(&udp6);
    if (error != 0) {
        fprintf(stderr, "x_socket_udp6 failed: %d\n", error);
        x_socket_close(tcp6);
        return 1;
    }

    x_socket_close(tcp6);
    x_socket_close(udp6);
    printf("  IPv6 sockets OK\n\n");
    return 0;
}

/* --- UDP bound convenience ------------------------------------------------ */

static int udp_bound_example(void)
{
    x_socket_t *socket = NULL;
    x_socket_address_t local;
    char addr_string[XLIB_ADDRESS_STRING_MAX];
    uint16_t port;
    int error;

    printf("UDP convenience bind...\n");

    error = x_socket_udp_bound(&socket, NULL, "0");
    if (error != 0) {
        fprintf(stderr, "x_socket_udp_bound failed: %d\n", error);
        return 1;
    }

    error = x_socket_local_address(socket, &local);
    if (error == 0) {
        x_address_to_string(&local, addr_string, sizeof(addr_string));
        x_address_port(&local, &port);
        printf("  bound to %s:%u\n", addr_string, (unsigned int)port);
    }

    x_socket_close(socket);
    printf("  UDP bound OK\n\n");
    return 0;
}

/* --- TLS hooks (no-op stub) ----------------------------------------------- */

static int stub_tls_connect(x_tls_context_t *ctx, x_socket_t *socket,
                             const char *hostname, void *user_data)
{
    (void)ctx; (void)socket; (void)user_data;
    printf("  TLS connect called for host: %s\n", hostname);
    return 0;
}

static int stub_tls_accept(x_tls_context_t *ctx, x_socket_t *socket, void *user_data)
{
    (void)ctx; (void)socket; (void)user_data;
    return 0;
}

static int stub_tls_read(x_tls_context_t *ctx, void *buffer, size_t size,
                          size_t *bytes_read, void *user_data)
{
    (void)ctx; (void)buffer; (void)size; (void)user_data;
    if (bytes_read != NULL) {
        *bytes_read = 0U;
    }
    return 0;
}

static int stub_tls_write(x_tls_context_t *ctx, const void *buffer, size_t size,
                           size_t *bytes_written, void *user_data)
{
    (void)ctx; (void)buffer; (void)size; (void)user_data;
    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }
    return 0;
}

static void stub_tls_close(x_tls_context_t *ctx, void *user_data)
{
    (void)ctx; (void)user_data;
}

static int tls_hooks_example(void)
{
    x_tls_hooks_t hooks;
    x_tls_context_t *ctx = NULL;
    x_socket_t *socket   = NULL;
    int error;

    printf("TLS hooks...\n");

    hooks.connect   = stub_tls_connect;
    hooks.accept    = stub_tls_accept;
    hooks.read      = stub_tls_read;
    hooks.write     = stub_tls_write;
    hooks.close     = stub_tls_close;
    hooks.user_data = NULL;

    error = x_tls_context_create(&ctx, &hooks);
    if (error != 0) {
        fprintf(stderr, "x_tls_context_create failed: %d\n", error);
        return 1;
    }

    error = x_socket_tcp(&socket);
    if (error != 0) {
        fprintf(stderr, "x_socket_tcp failed: %d\n", error);
        x_tls_context_destroy(ctx);
        return 1;
    }

    error = x_tls_connect(ctx, socket, "example.com");
    if (error != 0) {
        fprintf(stderr, "x_tls_connect failed: %d\n", error);
        x_socket_close(socket);
        x_tls_context_destroy(ctx);
        return 1;
    }

    x_tls_close(ctx);
    x_socket_close(socket);
    x_tls_context_destroy(ctx);

    printf("  TLS hooks OK\n\n");
    return 0;
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    int result = 0;

    result |= address_formatting_example();
    result |= address_family_example();
    result |= socket_timeout_example();
    result |= ipv6_socket_example();
    result |= udp_bound_example();
    result |= tls_hooks_example();

    if (result == 0) {
        printf("All networking extension examples passed.\n");
    }

    return result;
}
