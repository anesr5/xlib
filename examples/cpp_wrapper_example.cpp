#include <xlib/xlib.hpp>

#include <cstdio>
#include <cstring>
#include <string>

typedef int (*plugin_add_fn)(int left, int right);

static int check_thread_and_mutex()
{
    xlib::mutex mutex;
    int counter = 0;

    xlib::thread worker([&]() -> int {
        for (int i = 0; i < 1000; ++i) {
            xlib::lock_guard lock(mutex);
            ++counter;
        }

        return 17;
    });

    if (worker.join() != 17 || counter != 1000) {
        return 1;
    }

    xlib::recursive_mutex recursive;
    recursive.lock();
    recursive.lock();
    recursive.unlock();
    recursive.unlock();

    return 0;
}

static int check_file()
{
    const char *path = "xlib_cpp_wrapper.txt";
    const char *message = "cpp wrapper";
    char buffer[64];

    {
        xlib::file file(path, X_FILE_READ | X_FILE_WRITE | X_FILE_CREATE | X_FILE_TRUNCATE);
        if (file.write(message, std::strlen(message)) != std::strlen(message)) {
            return 1;
        }

        if (file.size() != std::strlen(message)) {
            return 1;
        }

        file.seek(0, X_FILE_SEEK_SET);
        std::memset(buffer, 0, sizeof(buffer));
        if (file.read(buffer, sizeof(buffer) - 1U) != std::strlen(message)) {
            return 1;
        }
    }

    x_file_remove(path);
    return std::strcmp(buffer, message) == 0 ? 0 : 1;
}

static int check_dynamic_library(const char *plugin_path)
{
    xlib::dynamic_library library(plugin_path);
    plugin_add_fn add = library.symbol<plugin_add_fn>("xlib_dynamic_plugin_add");

    return add(19, 23) == 42 ? 0 : 1;
}

static int check_socket()
{
    xlib::socket server = xlib::socket::tcp();
    server.set_reuse_address(true);
    server.bind(xlib::socket_address::resolve("127.0.0.1", "0", X_SOCKET_TCP, true));
    server.listen(1);

    std::uint16_t port = server.local_address().port();
    std::string port_text = std::to_string(static_cast<unsigned int>(port));

    xlib::thread server_thread([&]() -> int {
        xlib::socket client = server.accept();
        char buffer[16];
        std::memset(buffer, 0, sizeof(buffer));

        if (client.receive(buffer, sizeof(buffer) - 1U) != 4U || std::strcmp(buffer, "ping") != 0) {
            return 1;
        }

        return client.send("pong", 4U) == 4U ? 0 : 1;
    });

    xlib::socket client = xlib::socket::tcp();
    client.set_tcp_no_delay(true);
    client.connect(xlib::socket_address::resolve("127.0.0.1", port_text.c_str(), X_SOCKET_TCP));

    if (client.send("ping", 4U) != 4U) {
        return 1;
    }

    char buffer[16];
    std::memset(buffer, 0, sizeof(buffer));
    if (client.receive(buffer, sizeof(buffer) - 1U) != 4U || std::strcmp(buffer, "pong") != 0) {
        return 1;
    }

    return server_thread.join();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 1;
    }

    try {
        if (check_thread_and_mutex() != 0
            || check_file() != 0
            || check_dynamic_library(argv[1]) != 0
            || check_socket() != 0) {
            return 1;
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }

    std::printf("C++ wrapper example passed.\n");
    return 0;
}
