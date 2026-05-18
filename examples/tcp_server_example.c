#include <xlib/network.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *port = argc > 1 ? argv[1] : "8080";
    x_socket_address_t address;
    x_socket_t *server;
    x_socket_t *client;
    char buffer[512];
    size_t actual;

    if (x_socket_tcp(&server) != 0) {
        return 1;
    }

    x_socket_set_reuse_address(server, 1);

    if (x_address_resolve(&address, NULL, port, X_SOCKET_TCP, 1) != 0
        || x_socket_bind(server, &address) != 0
        || x_socket_listen(server, 16) != 0) {
        x_socket_close(server);
        return 1;
    }

    printf("Listening on port %s\n", port);

    if (x_socket_accept(server, &client) != 0) {
        x_socket_close(server);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive(client, buffer, sizeof(buffer) - 1U, &actual) == 0 && actual > 0U) {
        x_socket_send(client, buffer, actual, NULL);
    }

    x_socket_close(client);
    x_socket_close(server);
    return 0;
}
