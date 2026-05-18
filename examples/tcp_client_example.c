#include <xlib/network.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    const char *port = argc > 2 ? argv[2] : "8080";
    const char *message = argc > 3 ? argv[3] : "hello from xlib";
    x_socket_address_t address;
    x_socket_t *client;
    char buffer[512];
    size_t actual;

    if (x_socket_tcp(&client) != 0) {
        return 1;
    }

    if (x_address_resolve(&address, host, port, X_SOCKET_TCP, 0) != 0
        || x_socket_connect(client, &address) != 0) {
        x_socket_close(client);
        return 1;
    }

    if (x_socket_send(client, message, strlen(message), &actual) != 0
        || actual != strlen(message)) {
        x_socket_close(client);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (x_socket_receive(client, buffer, sizeof(buffer) - 1U, &actual) != 0) {
        x_socket_close(client);
        return 1;
    }

    printf("%s\n", buffer);

    x_socket_close(client);
    return 0;
}
