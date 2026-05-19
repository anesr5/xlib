#ifndef XLIB_XLIB_HPP
#define XLIB_XLIB_HPP

#include <xlib/xlib.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace xlib {

inline std::error_code make_error_code(int error)
{
    return std::error_code(error, std::generic_category());
}

inline void check(int error, const char *operation)
{
    if (error != 0) {
        throw std::system_error(make_error_code(error), operation);
    }
}

class thread {
public:
    thread() = default;

    explicit thread(std::function<int()> function)
    {
        std::unique_ptr<std::function<int()> > owned(new std::function<int()>(std::move(function)));
        int error = x_thread_create(&thread_, &thread::entry, owned.get());
        if (error != 0) {
            throw std::system_error(make_error_code(error), "x_thread_create");
        }

        owned.release();
    }

    thread(const thread &) = delete;
    thread &operator=(const thread &) = delete;

    thread(thread &&other) noexcept
        : thread_(other.thread_), joined_(other.joined_)
    {
        other.thread_ = nullptr;
        other.joined_ = true;
    }

    thread &operator=(thread &&other) noexcept
    {
        if (this != &other) {
            reset();
            thread_ = other.thread_;
            joined_ = other.joined_;
            other.thread_ = nullptr;
            other.joined_ = true;
        }

        return *this;
    }

    ~thread()
    {
        reset();
    }

    [[nodiscard]] int join()
    {
        int result = 0;
        check(x_thread_join(thread_, &result), "x_thread_join");
        joined_ = true;
        return result;
    }

    [[nodiscard]] bool joinable() const noexcept
    {
        return thread_ != nullptr && !joined_;
    }

private:
    static int entry(void *data) noexcept
    {
        std::unique_ptr<std::function<int()> > function(static_cast<std::function<int()> *>(data));
        try {
            return (*function)();
        } catch (...) {
            return -1;
        }
    }

    void reset() noexcept
    {
        if (thread_ == nullptr) {
            return;
        }

        if (!joined_) {
            // join() error is intentionally discarded here: we are in a
            // noexcept context (destructor / move-assign).  We still call
            // x_thread_destroy to release the x_thread_t allocation.  If
            // the join actually failed the underlying OS thread may still
            // be running, but there is no safe alternative from noexcept.
            (void)x_thread_join(thread_, nullptr);
            joined_ = true;
        }

        x_thread_destroy(thread_);
        thread_ = nullptr;
    }

    x_thread_t *thread_ = nullptr;
    bool joined_ = false;
};

class mutex {
public:
    mutex()
    {
        check(x_mutex_create(&mutex_), "x_mutex_create");
    }

    mutex(const mutex &) = delete;
    mutex &operator=(const mutex &) = delete;

    ~mutex()
    {
        x_mutex_destroy(mutex_);
    }

    void lock()
    {
        check(x_mutex_lock(mutex_), "x_mutex_lock");
    }

    [[nodiscard]] bool try_lock()
    {
        int error = x_mutex_try_lock(mutex_);
        if (error == 0) {
            return true;
        }

        if (error == EBUSY) {
            return false;
        }

        throw std::system_error(make_error_code(error), "x_mutex_try_lock");
    }

    void unlock()
    {
        check(x_mutex_unlock(mutex_), "x_mutex_unlock");
    }

    x_mutex_t *native_handle() noexcept
    {
        return mutex_;
    }

private:
    x_mutex_t *mutex_ = nullptr;
};

class recursive_mutex {
public:
    recursive_mutex()
    {
        check(x_recursive_mutex_create(&mutex_), "x_recursive_mutex_create");
    }

    recursive_mutex(const recursive_mutex &) = delete;
    recursive_mutex &operator=(const recursive_mutex &) = delete;

    ~recursive_mutex()
    {
        x_mutex_destroy(mutex_);
    }

    void lock()
    {
        check(x_mutex_lock(mutex_), "x_mutex_lock");
    }

    void unlock()
    {
        check(x_mutex_unlock(mutex_), "x_mutex_unlock");
    }

private:
    x_mutex_t *mutex_ = nullptr;
};

template <typename Mutex>
class basic_lock_guard {
public:
    explicit basic_lock_guard(Mutex &value)
        : mutex_(value)
    {
        mutex_.lock();
    }

    basic_lock_guard(const basic_lock_guard &) = delete;
    basic_lock_guard &operator=(const basic_lock_guard &) = delete;

    ~basic_lock_guard()
    {
        mutex_.unlock();
    }

private:
    Mutex &mutex_;
};

// Convenience aliases — mirrors std::lock_guard usage
using lock_guard = basic_lock_guard<mutex>;
using recursive_lock_guard = basic_lock_guard<recursive_mutex>;

class file {
public:
    file() = default;

    file(const char *path, int mode)
    {
        open(path, mode);
    }

    file(const file &) = delete;
    file &operator=(const file &) = delete;

    file(file &&other) noexcept
        : file_(other.file_)
    {
        other.file_ = nullptr;
    }

    file &operator=(file &&other) noexcept
    {
        if (this != &other) {
            close();
            file_ = other.file_;
            other.file_ = nullptr;
        }

        return *this;
    }

    ~file()
    {
        close();
    }

    void open(const char *path, int mode)
    {
        close();
        check(x_file_open(&file_, path, mode), "x_file_open");
    }

    [[nodiscard]] std::size_t read(void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_file_read(file_, buffer, size, &actual), "x_file_read");
        return actual;
    }

    [[nodiscard]] std::size_t write(const void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_file_write(file_, buffer, size, &actual), "x_file_write");
        return actual;
    }

    void seek(std::int64_t offset, int origin)
    {
        check(x_file_seek(file_, offset, origin), "x_file_seek");
    }

    [[nodiscard]] std::uint64_t size()
    {
        std::uint64_t value = 0;
        check(x_file_size(file_, &value), "x_file_size");
        return value;
    }

    void close() noexcept
    {
        if (file_ != nullptr) {
            x_file_close(file_);
            file_ = nullptr;
        }
    }

private:
    x_file_t *file_ = nullptr;
};

class dynamic_library {
public:
    dynamic_library() = default;

    explicit dynamic_library(const char *path)
    {
        open(path);
    }

    dynamic_library(const dynamic_library &) = delete;
    dynamic_library &operator=(const dynamic_library &) = delete;

    dynamic_library(dynamic_library &&other) noexcept
        : library_(other.library_)
    {
        other.library_ = nullptr;
    }

    dynamic_library &operator=(dynamic_library &&other) noexcept
    {
        if (this != &other) {
            close();
            library_ = other.library_;
            other.library_ = nullptr;
        }

        return *this;
    }

    ~dynamic_library()
    {
        close();
    }

    void open(const char *path)
    {
        close();
        check(x_dynamic_library_open(&library_, path), "x_dynamic_library_open");
    }

    template <typename T>
    T symbol(const char *name)
    {
        // Casting void* to a non-pointer type via a union is UB in C++ and
        // produces garbage when sizeof(T) != sizeof(void*).  Restrict T to
        // pointer types only (typically function-pointer typedefs).
        static_assert(std::is_pointer<T>::value,
            "T must be a pointer type (e.g. a function pointer typedef)");

        void *raw = nullptr;
        union converter {
            void *object;
            T value;
        } converted;

        check(x_dynamic_library_symbol(library_, name, &raw), "x_dynamic_library_symbol");
        converted.object = raw;
        return converted.value;
    }

    void close() noexcept
    {
        if (library_ != nullptr) {
            x_dynamic_library_close(library_);
            library_ = nullptr;
        }
    }

private:
    x_dynamic_library_t *library_ = nullptr;
};

class socket_address {
public:
    socket_address() = default;

    explicit socket_address(const x_socket_address_t &address)
        : address_(address)
    {
    }

    [[nodiscard]] static socket_address resolve(
        const char *host, const char *service, int type, bool passive = false)
    {
        socket_address result;
        check(x_address_resolve(&result.address_, host, service, type, passive ? 1 : 0), "x_address_resolve");
        return result;
    }

    [[nodiscard]] static std::vector<socket_address> resolve_all(
        const char *host, const char *service, int type, bool passive = false)
    {
        x_address_t *addresses = nullptr;
        std::size_t count = 0;
        check(x_address_resolve_all(&addresses, &count, host, service, type, passive ? 1 : 0),
            "x_address_resolve_all");

        std::vector<socket_address> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            result.push_back(socket_address(addresses[i]));
        }

        x_address_list_free(addresses);
        return result;
    }

    [[nodiscard]] static socket_address from_string(
        const char *host, std::uint16_t port, int family)
    {
        socket_address result;
        check(x_address_from_string(&result.address_, host, port, family), "x_address_from_string");
        return result;
    }

    [[nodiscard]] std::uint16_t port() const
    {
        std::uint16_t value = 0;
        check(x_address_port(&address_, &value), "x_address_port");
        return value;
    }

    [[nodiscard]] int family() const noexcept
    {
        return x_address_family(&address_);
    }

    [[nodiscard]] std::string to_string() const
    {
        char buffer[XLIB_ADDRESS_STRING_MAX];
        check(x_address_to_string(&address_, buffer, sizeof(buffer)), "x_address_to_string");
        return std::string(buffer);
    }

    const x_socket_address_t *native_handle() const noexcept
    {
        return &address_;
    }

    x_socket_address_t *native_handle() noexcept
    {
        return &address_;
    }

private:
    x_socket_address_t address_{};
};

class socket {
public:
    socket() = default;

    [[nodiscard]] static socket tcp()
    {
        x_socket_t *handle = nullptr;
        check(x_socket_tcp(&handle), "x_socket_tcp");
        return socket(handle);
    }

    [[nodiscard]] static socket udp()
    {
        x_socket_t *handle = nullptr;
        check(x_socket_udp(&handle), "x_socket_udp");
        return socket(handle);
    }

    [[nodiscard]] static socket tcp6()
    {
        x_socket_t *handle = nullptr;
        check(x_socket_tcp6(&handle), "x_socket_tcp6");
        return socket(handle);
    }

    [[nodiscard]] static socket udp6()
    {
        x_socket_t *handle = nullptr;
        check(x_socket_udp6(&handle), "x_socket_udp6");
        return socket(handle);
    }

    [[nodiscard]] static socket tcp_dual_stack()
    {
        x_socket_t *handle = nullptr;
        check(x_socket_tcp_dual_stack(&handle), "x_socket_tcp_dual_stack");
        return socket(handle);
    }

    [[nodiscard]] static socket udp_dual_stack()
    {
        x_socket_t *handle = nullptr;
        check(x_socket_udp_dual_stack(&handle), "x_socket_udp_dual_stack");
        return socket(handle);
    }

    socket(const socket &) = delete;
    socket &operator=(const socket &) = delete;

    socket(socket &&other) noexcept
        : socket_(other.socket_)
    {
        other.socket_ = nullptr;
    }

    socket &operator=(socket &&other) noexcept
    {
        if (this != &other) {
            close();
            socket_ = other.socket_;
            other.socket_ = nullptr;
        }

        return *this;
    }

    ~socket()
    {
        close();
    }

    void bind(const socket_address &address)
    {
        check(x_socket_bind(socket_, address.native_handle()), "x_socket_bind");
    }

    void listen(int backlog)
    {
        check(x_socket_listen(socket_, backlog), "x_socket_listen");
    }

    socket accept()
    {
        x_socket_t *client = nullptr;
        check(x_socket_accept(socket_, &client), "x_socket_accept");
        return socket(client);
    }

    void connect(const socket_address &address)
    {
        check(x_socket_connect(socket_, address.native_handle()), "x_socket_connect");
    }

    void shutdown(int how)
    {
        check(x_socket_shutdown(socket_, how), "x_socket_shutdown");
    }

    [[nodiscard]] std::size_t send(const void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_socket_send(socket_, buffer, size, &actual), "x_socket_send");
        return actual;
    }

    [[nodiscard]] std::size_t receive(void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_socket_receive(socket_, buffer, size, &actual), "x_socket_receive");
        return actual;
    }

    [[nodiscard]] std::size_t send_to(const socket_address &address, const void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_socket_send_to(socket_, address.native_handle(), buffer, size, &actual), "x_socket_send_to");
        return actual;
    }

    [[nodiscard]] std::size_t receive_from(socket_address *address, void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        x_socket_address_t native_address;
        x_socket_address_t *native_pointer = address != nullptr ? &native_address : nullptr;
        check(x_socket_receive_from(socket_, native_pointer, buffer, size, &actual), "x_socket_receive_from");
        if (address != nullptr) {
            *address = socket_address(native_address);
        }
        return actual;
    }

    [[nodiscard]] socket_address local_address()
    {
        socket_address address;
        check(x_socket_local_address(socket_, address.native_handle()), "x_socket_local_address");
        return address;
    }

    [[nodiscard]] socket_address peer_address()
    {
        socket_address address;
        check(x_socket_peer_address(socket_, address.native_handle()), "x_socket_peer_address");
        return address;
    }

    void set_nonblocking(bool enabled)
    {
        check(x_socket_set_nonblocking(socket_, enabled ? 1 : 0), "x_socket_set_nonblocking");
    }

    void set_reuse_address(bool enabled)
    {
        check(x_socket_set_reuse_address(socket_, enabled ? 1 : 0), "x_socket_set_reuse_address");
    }

    void set_tcp_no_delay(bool enabled)
    {
        check(x_socket_set_tcp_no_delay(socket_, enabled ? 1 : 0), "x_socket_set_tcp_no_delay");
    }

    void set_send_timeout(std::uint32_t milliseconds)
    {
        check(x_socket_set_send_timeout(socket_, milliseconds), "x_socket_set_send_timeout");
    }

    void set_receive_timeout(std::uint32_t milliseconds)
    {
        check(x_socket_set_receive_timeout(socket_, milliseconds), "x_socket_set_receive_timeout");
    }

    void join_multicast_group(const char *group_address, const char *interface_address = nullptr)
    {
        check(x_socket_join_multicast_group(socket_, group_address, interface_address),
            "x_socket_join_multicast_group");
    }

    void leave_multicast_group(const char *group_address, const char *interface_address = nullptr)
    {
        check(x_socket_leave_multicast_group(socket_, group_address, interface_address),
            "x_socket_leave_multicast_group");
    }

    [[nodiscard]] static bool would_block(int error) noexcept
    {
        return x_socket_would_block(error) != 0;
    }

    x_socket_t *native_handle() noexcept
    {
        return socket_;
    }

    void close() noexcept
    {
        if (socket_ != nullptr) {
            x_socket_close(socket_);
            socket_ = nullptr;
        }
    }

private:
    explicit socket(x_socket_t *handle)
        : socket_(handle)
    {
    }

    x_socket_t *socket_ = nullptr;
};

class network_interfaces {
public:
    network_interfaces()
    {
        check(x_network_interfaces(&interfaces_, &count_), "x_network_interfaces");
    }

    network_interfaces(const network_interfaces &) = delete;
    network_interfaces &operator=(const network_interfaces &) = delete;

    network_interfaces(network_interfaces &&other) noexcept
        : interfaces_(other.interfaces_), count_(other.count_)
    {
        other.interfaces_ = nullptr;
        other.count_ = 0;
    }

    network_interfaces &operator=(network_interfaces &&other) noexcept
    {
        if (this != &other) {
            x_network_interfaces_free(interfaces_);
            interfaces_ = other.interfaces_;
            count_ = other.count_;
            other.interfaces_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }

    ~network_interfaces()
    {
        x_network_interfaces_free(interfaces_);
    }

    [[nodiscard]] const x_network_interface_t *begin() const noexcept { return interfaces_; }
    [[nodiscard]] const x_network_interface_t *end() const noexcept { return interfaces_ + count_; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

private:
    x_network_interface_t *interfaces_ = nullptr;
    std::size_t count_ = 0;
};

// ---- condition -------------------------------------------------------------

class condition {
public:
    condition()
    {
        check(x_condition_create(&condition_), "x_condition_create");
    }

    condition(const condition &) = delete;
    condition &operator=(const condition &) = delete;

    ~condition()
    {
        x_condition_destroy(condition_);
    }

    void wait(mutex &m)
    {
        check(x_condition_wait(condition_, m.native_handle()), "x_condition_wait");
    }

    void signal()
    {
        check(x_condition_signal(condition_), "x_condition_signal");
    }

    void broadcast()
    {
        check(x_condition_broadcast(condition_), "x_condition_broadcast");
    }

    x_condition_t *native_handle() noexcept
    {
        return condition_;
    }

private:
    x_condition_t *condition_ = nullptr;
};

// ---- semaphore -------------------------------------------------------------

class semaphore {
public:
    explicit semaphore(unsigned int initial_count = 0)
    {
        check(x_semaphore_create(&semaphore_, initial_count), "x_semaphore_create");
    }

    semaphore(const semaphore &) = delete;
    semaphore &operator=(const semaphore &) = delete;

    ~semaphore()
    {
        x_semaphore_destroy(semaphore_);
    }

    void wait()
    {
        check(x_semaphore_wait(semaphore_), "x_semaphore_wait");
    }

    void post()
    {
        check(x_semaphore_post(semaphore_), "x_semaphore_post");
    }

    x_semaphore_t *native_handle() noexcept
    {
        return semaphore_;
    }

private:
    x_semaphore_t *semaphore_ = nullptr;
};

template <typename T>
class tls_key {
public:
    tls_key()
    {
        check(x_tls_key_create(&key_), "x_tls_key_create");
    }

    tls_key(const tls_key &) = delete;
    tls_key &operator=(const tls_key &) = delete;

    tls_key(tls_key &&other) noexcept
        : key_(other.key_)
    {
        other.key_ = nullptr;
    }

    tls_key &operator=(tls_key &&other) noexcept
    {
        if (this != &other) {
            reset();
            key_ = other.key_;
            other.key_ = nullptr;
        }
        return *this;
    }

    ~tls_key()
    {
        reset();
    }

    void set(T *value)
    {
        check(x_tls_set(key_, value), "x_tls_set");
    }

    [[nodiscard]] T *get() const noexcept
    {
        return static_cast<T *>(x_tls_get(key_));
    }

    x_tls_key_t *native_handle() noexcept
    {
        return key_;
    }

private:
    void reset() noexcept
    {
        if (key_ != nullptr) {
            x_tls_key_destroy(key_);
            key_ = nullptr;
        }
    }

    x_tls_key_t *key_ = nullptr;
};

class pipe {
public:
    pipe() = default;

    pipe(const pipe &) = delete;
    pipe &operator=(const pipe &) = delete;

    pipe(pipe &&other) noexcept
        : pipe_(other.pipe_)
    {
        other.pipe_ = nullptr;
    }

    pipe &operator=(pipe &&other) noexcept
    {
        if (this != &other) {
            close();
            pipe_ = other.pipe_;
            other.pipe_ = nullptr;
        }
        return *this;
    }

    ~pipe()
    {
        close();
    }

    [[nodiscard]] static std::pair<pipe, pipe> create()
    {
        x_pipe_t *read_pipe = nullptr;
        x_pipe_t *write_pipe = nullptr;
        check(x_pipe_create(&read_pipe, &write_pipe), "x_pipe_create");
        return std::make_pair(pipe(read_pipe), pipe(write_pipe));
    }

    [[nodiscard]] std::size_t read(void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_pipe_read(pipe_, buffer, size, &actual), "x_pipe_read");
        return actual;
    }

    [[nodiscard]] std::size_t write(const void *buffer, std::size_t size)
    {
        std::size_t actual = 0;
        check(x_pipe_write(pipe_, buffer, size, &actual), "x_pipe_write");
        return actual;
    }

    void close() noexcept
    {
        if (pipe_ != nullptr) {
            x_pipe_close(pipe_);
            pipe_ = nullptr;
        }
    }

    x_pipe_t *native_handle() noexcept
    {
        return pipe_;
    }

private:
    explicit pipe(x_pipe_t *handle)
        : pipe_(handle)
    {
    }

    x_pipe_t *pipe_ = nullptr;
};

// ---- mapped_file -----------------------------------------------------------

class mapped_file {
public:
    mapped_file() = default;

    mapped_file(const char *path, std::size_t size)
    {
        create(path, size);
    }

    mapped_file(const char *path, int protection)
    {
        open(path, protection);
    }

    mapped_file(const mapped_file &) = delete;
    mapped_file &operator=(const mapped_file &) = delete;

    mapped_file(mapped_file &&other) noexcept
        : mapping_(other.mapping_)
    {
        other.mapping_ = nullptr;
    }

    mapped_file &operator=(mapped_file &&other) noexcept
    {
        if (this != &other) {
            destroy();
            mapping_ = other.mapping_;
            other.mapping_ = nullptr;
        }

        return *this;
    }

    ~mapped_file()
    {
        destroy();
    }

    void create(const char *path, std::size_t size)
    {
        destroy();
        check(x_mapped_file_create(&mapping_, path, size), "x_mapped_file_create");
    }

    void open(const char *path, int protection)
    {
        destroy();
        check(x_mapped_file_open(&mapping_, path, protection), "x_mapped_file_open");
    }

    [[nodiscard]] void *data() noexcept
    {
        return x_mapped_file_data(mapping_);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return x_mapped_file_size(mapping_);
    }

    void flush()
    {
        check(x_mapped_file_flush(mapping_), "x_mapped_file_flush");
    }

    void destroy() noexcept
    {
        if (mapping_ != nullptr) {
            x_mapped_file_destroy(mapping_);
            mapping_ = nullptr;
        }
    }

    x_mapped_file_t *native_handle() noexcept
    {
        return mapping_;
    }

private:
    x_mapped_file_t *mapping_ = nullptr;
};

// ---- process ---------------------------------------------------------------

class process {
public:
    process() = default;

    process(const char *path, char *const arguments[],
            const x_process_options_t *options = nullptr)
    {
        start(path, arguments, options);
    }

    process(const process &) = delete;
    process &operator=(const process &) = delete;

    process(process &&other) noexcept
        : process_(other.process_)
    {
        other.process_ = nullptr;
    }

    process &operator=(process &&other) noexcept
    {
        if (this != &other) {
            destroy();
            process_ = other.process_;
            other.process_ = nullptr;
        }

        return *this;
    }

    ~process()
    {
        destroy();
    }

    void start(const char *path, char *const arguments[],
               const x_process_options_t *options = nullptr)
    {
        destroy();
        check(x_process_start(&process_, path, arguments, options), "x_process_start");
    }

    [[nodiscard]] int wait()
    {
        int exit_code = 0;
        check(x_process_wait(process_, &exit_code), "x_process_wait");
        return exit_code;
    }

    [[nodiscard]] bool poll(int *exit_code = nullptr)
    {
        int completed = 0;
        check(x_process_poll(process_, &completed, exit_code), "x_process_poll");
        return completed != 0;
    }

    void terminate()
    {
        check(x_process_terminate(process_), "x_process_terminate");
    }

    void destroy() noexcept
    {
        if (process_ != nullptr) {
            x_process_destroy(process_);
            process_ = nullptr;
        }
    }

    x_process_t *native_handle() noexcept
    {
        return process_;
    }

private:
    x_process_t *process_ = nullptr;
};

class file_watcher {
public:
    explicit file_watcher(const char *path)
    {
        check(x_file_watcher_create(&watcher_, path), "x_file_watcher_create");
    }

    explicit file_watcher(const std::string &path)
        : file_watcher(path.c_str())
    {
    }

    file_watcher(const file_watcher &) = delete;
    file_watcher &operator=(const file_watcher &) = delete;

    file_watcher(file_watcher &&other) noexcept
        : watcher_(other.watcher_)
    {
        other.watcher_ = nullptr;
    }

    file_watcher &operator=(file_watcher &&other) noexcept
    {
        if (this != &other) {
            destroy();
            watcher_ = other.watcher_;
            other.watcher_ = nullptr;
        }
        return *this;
    }

    ~file_watcher()
    {
        destroy();
    }

    void poll(x_file_watch_callback callback, void *user_data = nullptr)
    {
        check(x_file_watcher_poll(watcher_, callback, user_data), "x_file_watcher_poll");
    }

    void destroy() noexcept
    {
        if (watcher_ != nullptr) {
            x_file_watcher_destroy(watcher_);
            watcher_ = nullptr;
        }
    }

    x_file_watcher_t *native_handle() noexcept
    {
        return watcher_;
    }

private:
    x_file_watcher_t *watcher_ = nullptr;
};

// ---- directory_entry / directory_iterator / directory ----------------------

class directory_entry {
public:
    [[nodiscard]] const char *name() const noexcept { return entry_.name; }
    [[nodiscard]] bool is_directory() const noexcept { return entry_.is_directory != 0; }
    [[nodiscard]] std::uint64_t size() const noexcept { return entry_.size; }
    const x_directory_entry_t &native() const noexcept { return entry_; }

private:
    friend class directory_iterator;
    x_directory_entry_t entry_{};
};

class directory_iterator {
public:
    directory_iterator() = default;

    explicit directory_iterator(const char *path)
    {
        check(x_directory_open(&directory_, path), "x_directory_open");
        advance();
    }

    directory_iterator(const directory_iterator &) = delete;
    directory_iterator &operator=(const directory_iterator &) = delete;

    directory_iterator(directory_iterator &&other) noexcept
        : directory_(other.directory_), current_(other.current_)
    {
        other.directory_ = nullptr;
    }

    directory_iterator &operator=(directory_iterator &&other) noexcept
    {
        if (this != &other) {
            if (directory_ != nullptr) {
                x_directory_close(directory_);
            }
            directory_ = other.directory_;
            current_ = other.current_;
            other.directory_ = nullptr;
        }
        return *this;
    }

    ~directory_iterator()
    {
        if (directory_ != nullptr) {
            x_directory_close(directory_);
        }
    }

    const directory_entry &operator*() const noexcept { return current_; }
    const directory_entry *operator->() const noexcept { return &current_; }

    directory_iterator &operator++()
    {
        advance();
        return *this;
    }

    bool operator==(const directory_iterator &other) const noexcept
    {
        return (directory_ == nullptr) == (other.directory_ == nullptr);
    }

    bool operator!=(const directory_iterator &other) const noexcept
    {
        return !(*this == other);
    }

private:
    void advance()
    {
        int has_entry = 0;
        check(x_directory_next(directory_, &current_.entry_, &has_entry), "x_directory_next");
        if (!has_entry) {
            x_directory_close(directory_);
            directory_ = nullptr;
        }
    }

    x_directory_t *directory_ = nullptr;
    directory_entry current_;
};

class directory {
public:
    explicit directory(const char *path)
        : path_(path)
    {
    }

    explicit directory(const std::string &path)
        : path_(path)
    {
    }

    [[nodiscard]] directory_iterator begin() const { return directory_iterator(path_.c_str()); }
    [[nodiscard]] directory_iterator end() const { return directory_iterator(); }

private:
    std::string path_;
};

// ---- event_loop ------------------------------------------------------------

[[nodiscard]] inline x_event_source_type_t source_type(const x_event_source_t *source) noexcept
{
    return static_cast<x_event_source_type_t>(x_event_source_type(source));
}

class event_loop {
public:
    event_loop()
    {
        check(x_event_loop_create(&loop_), "x_event_loop_create");
    }

    event_loop(const event_loop &) = delete;
    event_loop &operator=(const event_loop &) = delete;

    ~event_loop()
    {
        if (loop_ != nullptr) {
            x_event_loop_destroy(loop_);
        }
    }

    void run()
    {
        check(x_event_loop_run(loop_), "x_event_loop_run");
    }

    void wake()
    {
        check(x_event_loop_wake(loop_), "x_event_loop_wake");
    }

    void cancel()
    {
        check(x_event_loop_cancel(loop_), "x_event_loop_cancel");
    }

    void stop() noexcept
    {
        x_event_loop_stop(loop_);
    }

    x_event_source_t *add_timer(
        std::chrono::milliseconds delay,
        std::chrono::milliseconds interval,
        x_event_callback callback,
        void *user_data = nullptr)
    {
        x_event_source_t *source = nullptr;
        check(x_event_loop_add_timer(
            loop_,
            &source,
            static_cast<std::uint64_t>(delay.count()),
            static_cast<std::uint64_t>(interval.count()),
            callback,
            user_data),
            "x_event_loop_add_timer");
        return source;
    }

    x_event_source_t *add_process(
        process &proc,
        std::chrono::milliseconds interval,
        x_event_callback callback,
        void *user_data = nullptr)
    {
        x_event_source_t *source = nullptr;
        check(x_event_loop_add_process(
            loop_,
            &source,
            proc.native_handle(),
            static_cast<std::uint64_t>(interval.count()),
            callback,
            user_data),
            "x_event_loop_add_process");
        return source;
    }

    x_event_source_t *add_file_watcher(
        file_watcher &watcher,
        std::chrono::milliseconds interval,
        x_event_callback callback,
        void *user_data = nullptr)
    {
        x_event_source_t *source = nullptr;
        check(x_event_loop_add_file_watcher(
            loop_,
            &source,
            watcher.native_handle(),
            static_cast<std::uint64_t>(interval.count()),
            callback,
            user_data),
            "x_event_loop_add_file_watcher");
        return source;
    }

    x_event_loop_t *native_handle() noexcept
    {
        return loop_;
    }

private:
    x_event_loop_t *loop_ = nullptr;
};

// ---- chrono helpers --------------------------------------------------------
//
// These wrap xlib time and sleep functions in terms of std::chrono so that
// callers can write duration-typed code without manual unit conversion.

namespace chrono {

[[nodiscard]] inline std::chrono::nanoseconds monotonic_now()
{
    std::uint64_t ns = 0;
    check(x_time_monotonic_ns(&ns), "x_time_monotonic_ns");
    return std::chrono::nanoseconds(static_cast<std::int64_t>(ns));
}

[[nodiscard]] inline std::chrono::nanoseconds system_now()
{
    std::uint64_t ns = 0;
    check(x_time_system_ns(&ns), "x_time_system_ns");
    return std::chrono::nanoseconds(static_cast<std::int64_t>(ns));
}

template <typename Rep, typename Period>
inline void sleep(std::chrono::duration<Rep, Period> duration)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    if (ms > 0) {
        check(x_time_sleep_ms(static_cast<std::uint64_t>(ms)), "x_time_sleep_ms");
    }
}

template <typename Rep, typename Period>
inline void sleep_precise(std::chrono::duration<Rep, Period> duration)
{
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    if (us > 0) {
        check(x_time_sleep_us(static_cast<std::uint64_t>(us)), "x_time_sleep_us");
    }
}

} // namespace chrono

class timer {
public:
    timer()
    {
        restart();
    }

    void restart()
    {
        check(x_timer_start(&timer_), "x_timer_start");
    }

    [[nodiscard]] std::chrono::nanoseconds elapsed() const
    {
        std::uint64_t ns = 0;
        check(x_timer_elapsed_ns(&timer_, &ns), "x_timer_elapsed_ns");
        return std::chrono::nanoseconds(static_cast<std::int64_t>(ns));
    }

    [[nodiscard]] const x_timer_t *native_handle() const noexcept
    {
        return &timer_;
    }

private:
    x_timer_t timer_{};
};

// ---- error category -------------------------------------------------------
//
// std::error_category subclass that maps xlib errno-style error codes to
// human-readable descriptions.  Use xlib::xlib_category() to obtain the
// singleton.  This fulfils the v1.8 "Error category support for C++" item.

class xlib_error_category : public std::error_category {
public:
    [[nodiscard]] const char *name() const noexcept override
    {
        return "xlib";
    }

    [[nodiscard]] std::string message(int code) const override
    {
        // Delegate to the generic (POSIX) category for the description,
        // since xlib error codes are errno values.  Prefix with "xlib: "
        // so the origin is clear in logs and exception messages.
        return std::string("xlib: ")
            + std::generic_category().message(code);
    }

    [[nodiscard]] bool equivalent(
        const std::error_code &code, int condition) const noexcept override
    {
        // Allow comparison against generic_category codes (EINVAL, etc.)
        return code.category() == *this && code.value() == condition;
    }
};

[[nodiscard]] inline const std::error_category &xlib_category() noexcept
{
    static const xlib_error_category instance;
    return instance;
}

inline std::error_code make_xlib_error_code(int error)
{
    return std::error_code(error, xlib_category());
}

// ---- log_hook_guard -------------------------------------------------------
//
// RAII helper that installs a log hook on construction and removes it on
// destruction.

class log_hook_guard {
public:
    explicit log_hook_guard(x_log_hook_t hook, void *user_data = nullptr)
    {
        x_log_set_hook(hook, user_data);
    }

    log_hook_guard(const log_hook_guard &) = delete;
    log_hook_guard &operator=(const log_hook_guard &) = delete;

    ~log_hook_guard()
    {
        x_log_set_hook(nullptr, nullptr);
    }
};

// ---- diag_hook_guard ------------------------------------------------------

class diag_hook_guard {
public:
    explicit diag_hook_guard(x_diag_hook_t hook, void *user_data = nullptr)
    {
        x_diag_set_hook(hook, user_data);
    }

    diag_hook_guard(const diag_hook_guard &) = delete;
    diag_hook_guard &operator=(const diag_hook_guard &) = delete;

    ~diag_hook_guard()
    {
        x_diag_set_hook(nullptr, nullptr);
    }
};

// ---- trace_hook_guard -----------------------------------------------------

class trace_hook_guard {
public:
    explicit trace_hook_guard(x_trace_hook_t hook, void *user_data = nullptr)
    {
        x_trace_set_hook(hook, user_data);
    }

    trace_hook_guard(const trace_hook_guard &) = delete;
    trace_hook_guard &operator=(const trace_hook_guard &) = delete;

    ~trace_hook_guard()
    {
        x_trace_set_hook(nullptr, nullptr);
    }
};

} // namespace xlib

#endif
