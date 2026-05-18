#include <xlib/network.h>
#include <xlib/thread.h>

#include <stdio.h>
#include <string.h>

struct server_state {
    x_mutex_t *mutex;
    x_condition_t *condition;
    int ready;
    int result;
    uint16_t port;
};

static void publish_server_state(struct server_state *state, int result, uint16_t port)
{
    x_mutex_lock(state->mutex);
    state->result = result;
    state->port = port;
    state->ready = 1;
    x_condition_signal(state->condition);
    x_mutex_unlock(state->mutex);
}

static int tcp_server_thread(void *data)
{
    struct server_state *state = (struct server_state *)data;
    x_socket_address_t address;
    x_socket_address_t local_address;
    x_socket_t *server = NULL;
    x_socket_t *client = NULL;
    char buffer[16];
    size_t actual;
    uint16_t port;

    if (x_socket_tcp(&server) != 0) {
        publish_server_state(state, 1, 0);
        return 1;
    }

    x_socket_set_reuse_address(server, 1);

    if (x_address_resolve(&address, "127.0.0.1", "0", X_SOCKET_TCP, 1) != 0
        || x_socket_bind(server, &address) != 0
        || x_socket_listen(server, 1) != 0
        || x_socket_local_address(server, &local_address) != 0
        || x_address_port(&local_address, &port) != 0) {
        x_socket_close(server);
        publish_server_state(state, 1, 0);
        return 1;
    }

    publish_server_state(state, 0, port);

    if (x_socket_accept(server, &client) != 0) {
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

static int run_tcp_check(void)
{
    struct server_state state;
    x_socket_address_t address;
    x_socket_t *client;
    x_thread_t *thread;
    char port_text[16];
    char buffer[16];
    size_t actual;
    int server_result = 1;

    memset(&state, 0, sizeof(state));

    if (x_mutex_create(&state.mutex) != 0) {
        return 1;
    }

    if (x_condition_create(&state.condition) != 0) {
        x_mutex_destroy(state.mutex);
        return 1;
    }

    if (x_thread_create(&thread, tcp_server_thread, &state) != 0) {
        x_condition_destroy(state.condition);
        x_mutex_destroy(state.mutex);
        return 1;
    }

    x_mutex_lock(state.mutex);
    while (!state.ready) {
        x_condition_wait(state.condition, state.mutex);
    }
    x_mutex_unlock(state.mutex);

    if (state.result != 0) {
        x_thread_join(thread, NULL);
        x_thread_destroy(thread);
        x_condition_destroy(state.condition);
        x_mutex_destroy(state.mutex);
        return 1;
    }

    snprintf(port_text, sizeof(port_text), "%u", (unsigned int)state.port);

    if (x_socket_tcp(&client) != 0) {
        return 1;
    }

    if (x_socket_set_nonblocking(client, 1) != 0
        || x_socket_set_nonblocking(client, 0) != 0
        || x_socket_set_tcp_no_delay(client, 1) != 0
        || x_address_resolve(&address, "127.0.0.1", port_text, X_SOCKET_TCP, 0) != 0
        || x_socket_connect(client, &address) != 0) {
        x_socket_close(client);
        return 1;
    }

    if (x_socket_send(client, "ping", 4U, &actual) != 0 || actual != 4U) {
        x_socket_close(client);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive(client, buffer, sizeof(buffer) - 1U, &actual) != 0
        || actual != 4U
        || strcmp(buffer, "pong") != 0) {
        x_socket_close(client);
        return 1;
    }

    x_socket_close(client);

    if (x_thread_join(thread, &server_result) != 0 || server_result != 0) {
        x_thread_destroy(thread);
        return 1;
    }

    x_thread_destroy(thread);
    x_condition_destroy(state.condition);
    x_mutex_destroy(state.mutex);
    return 0;
}

static int run_udp_check(void)
{
    x_socket_address_t bind_address;
    x_socket_address_t receiver_address;
    x_socket_address_t sender_address;
    x_socket_t *sender;
    x_socket_t *receiver;
    char buffer[32];
    size_t actual;

    if (x_socket_udp(&sender) != 0) {
        return 1;
    }

    if (x_socket_udp(&receiver) != 0) {
        x_socket_close(sender);
        return 1;
    }

    if (x_address_resolve(&bind_address, "127.0.0.1", "0", X_SOCKET_UDP, 1) != 0
        || x_socket_bind(receiver, &bind_address) != 0
        || x_socket_local_address(receiver, &receiver_address) != 0
        || x_socket_send_to(sender, &receiver_address, "datagram", 8U, &actual) != 0
        || actual != 8U) {
        x_socket_close(receiver);
        x_socket_close(sender);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive_from(receiver, &sender_address, buffer, sizeof(buffer) - 1U, &actual) != 0
        || actual != 8U
        || strcmp(buffer, "datagram") != 0) {
        x_socket_close(receiver);
        x_socket_close(sender);
        return 1;
    }

    x_socket_close(receiver);
    x_socket_close(sender);
    return 0;
}

int main(void)
{
    if (run_tcp_check() != 0 || run_udp_check() != 0) {
        return 1;
    }

    printf("Network example passed.\n");
    return 0;
}
