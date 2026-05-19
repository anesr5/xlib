#include <xlib/network.h>
#include <xlib/thread.h>
#include <xlib/tls.h>

#include <stdio.h>
#include <string.h>

struct tcp_v2_state {
    x_mutex_t *mutex;
    x_condition_t *condition;
    int ready;
    int result;
    uint16_t port;
};

static void tcp_v2_publish(struct tcp_v2_state *state, int result, uint16_t port)
{
    x_mutex_lock(state->mutex);
    state->result = result;
    state->port = port;
    state->ready = 1;
    x_condition_signal(state->condition);
    x_mutex_unlock(state->mutex);
}

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
    x_address_t *addresses = NULL;
    size_t count = 0U;
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

    error = x_address_resolve_all(&addresses, &count, "localhost", "80", X_SOCKET_TCP, 0);
    if (error != 0 || count == 0U) {
        fprintf(stderr, "x_address_resolve_all failed: %d\n", error);
        return 1;
    }
    printf("  resolved %u localhost address(es)\n", (unsigned int)count);
    x_address_list_free(addresses);

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
    x_socket_t *dual_tcp = NULL;
    x_socket_t *dual_udp = NULL;
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

    error = x_socket_tcp_dual_stack(&dual_tcp);
    if (error != 0) {
        x_socket_close(tcp6);
        x_socket_close(udp6);
        printf("  dual-stack TCP unavailable on this platform: %d\n", error);
        printf("  IPv6 sockets OK\n\n");
        return 0;
    }

    error = x_socket_udp_dual_stack(&dual_udp);
    if (error != 0) {
        x_socket_close(tcp6);
        x_socket_close(udp6);
        x_socket_close(dual_tcp);
        printf("  dual-stack UDP unavailable on this platform: %d\n", error);
        printf("  IPv6 sockets OK\n\n");
        return 0;
    }

    x_socket_close(tcp6);
    x_socket_close(udp6);
    x_socket_close(dual_tcp);
    x_socket_close(dual_udp);
    printf("  IPv6 sockets OK\n\n");
    return 0;
}

static int tcp_v2_server(void *data)
{
    struct tcp_v2_state *state = (struct tcp_v2_state *)data;
    x_socket_t *server = NULL;
    x_socket_t *client = NULL;
    x_socket_address_t bind_address;
    x_socket_address_t local_address;
    x_socket_address_t peer_address;
    uint16_t port = 0;
    char buffer[16];
    size_t actual = 0U;

    if (x_socket_tcp(&server) != 0) {
        tcp_v2_publish(state, 1, 0);
        return 1;
    }

    if (x_socket_set_reuse_address(server, 1) != 0
        || x_address_resolve(&bind_address, "127.0.0.1", "0", X_SOCKET_TCP, 1) != 0
        || x_socket_bind(server, &bind_address) != 0
        || x_socket_listen(server, 1) != 0
        || x_socket_local_address(server, &local_address) != 0
        || x_address_port(&local_address, &port) != 0) {
        x_socket_close(server);
        tcp_v2_publish(state, 1, 0);
        return 1;
    }

    tcp_v2_publish(state, 0, port);

    if (x_socket_accept(server, &client) != 0
        || x_socket_peer_address(client, &peer_address) != 0
        || x_address_family(&peer_address) != X_ADDRESS_FAMILY_IPV4) {
        if (client != NULL) {
            x_socket_close(client);
        }
        x_socket_close(server);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive(client, buffer, sizeof(buffer) - 1U, &actual) != 0
        || actual != 4U
        || strcmp(buffer, "ping") != 0) {
        x_socket_close(client);
        x_socket_close(server);
        return 1;
    }

    if (x_socket_send(client, "pong", 4U, &actual) != 0 || actual != 4U) {
        x_socket_close(client);
        x_socket_close(server);
        return 1;
    }

    x_socket_close(client);
    x_socket_close(server);
    return 0;
}

static int tcp_peer_shutdown_example(void)
{
    struct tcp_v2_state state;
    x_socket_t *client = NULL;
    x_socket_address_t address;
    x_socket_address_t local_address;
    x_thread_t *thread = NULL;
    char port_text[16];
    char buffer[16];
    size_t actual = 0U;
    int server_result = 1;
    int ok = 0;

    printf("Peer address and socket shutdown...\n");
    memset(&state, 0, sizeof(state));

    if (x_mutex_create(&state.mutex) != 0) {
        return 1;
    }
    if (x_condition_create(&state.condition) != 0) {
        x_mutex_destroy(state.mutex);
        return 1;
    }
    if (x_thread_create(&thread, tcp_v2_server, &state) != 0) {
        x_condition_destroy(state.condition);
        x_mutex_destroy(state.mutex);
        return 1;
    }

    x_mutex_lock(state.mutex);
    while (!state.ready) {
        x_condition_wait(state.condition, state.mutex);
    }
    x_mutex_unlock(state.mutex);

    snprintf(port_text, sizeof(port_text), "%u", (unsigned int)state.port);
    if (state.result == 0
        && x_socket_tcp(&client) == 0
        && x_address_resolve(&address, "127.0.0.1", port_text, X_SOCKET_TCP, 0) == 0
        && x_socket_connect(client, &address) == 0
        && x_socket_local_address(client, &local_address) == 0
        && x_address_family(&local_address) == X_ADDRESS_FAMILY_IPV4
        && x_socket_send(client, "ping", 4U, &actual) == 0
        && actual == 4U
        && x_socket_shutdown(client, X_SOCKET_SHUTDOWN_WRITE) == 0
        && x_socket_receive(client, buffer, sizeof(buffer) - 1U, &actual) == 0
        && actual == 4U) {
        buffer[actual] = '\0';
        ok = strcmp(buffer, "pong") == 0;
    }

    if (client != NULL) {
        x_socket_close(client);
    }

    if (x_thread_join(thread, &server_result) != 0 || server_result != 0) {
        ok = 0;
    }
    x_thread_destroy(thread);
    x_condition_destroy(state.condition);
    x_mutex_destroy(state.mutex);

    if (!ok) {
        fprintf(stderr, "peer/shutdown check failed\n");
        return 1;
    }

    printf("  peer/shutdown OK\n\n");
    return 0;
}

static int interface_enumeration_example(void)
{
    x_network_interface_t *interfaces = NULL;
    size_t count = 0U;
    int error;

    printf("Network interface enumeration...\n");
    error = x_network_interfaces(&interfaces, &count);
    if (error != 0) {
        fprintf(stderr, "x_network_interfaces failed: %d\n", error);
        return 1;
    }

    printf("  found %u interface address(es)\n", (unsigned int)count);
    x_network_interfaces_free(interfaces);
    printf("  interface enumeration OK\n\n");
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
    result |= tcp_peer_shutdown_example();
    result |= interface_enumeration_example();
    result |= udp_bound_example();
    result |= tls_hooks_example();

    if (result == 0) {
        printf("All networking extension examples passed.\n");
    }

    return result;
}
