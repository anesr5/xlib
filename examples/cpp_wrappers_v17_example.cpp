#include <xlib/xlib.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

/* --- condition + mutex + semaphore ---------------------------------------- */

static int synchronization_example()
{
    printf("Condition and semaphore wrappers...\n");

    xlib::mutex mtx;
    xlib::condition cond;
    xlib::semaphore sem(0);

    {
        xlib::lock_guard guard(mtx);
        cond.signal();
    }

    sem.post();
    sem.wait();

    printf("  condition and semaphore OK\n\n");
    return 0;
}

/* --- mapped_file ---------------------------------------------------------- */

static int mapped_file_example()
{
    printf("Mapped file wrapper...\n");

    const char *path = "xlib_v17_example_mapped.bin";
    const char *payload = "v1.7 mapped file test";

    {
        xlib::mapped_file mf(path, static_cast<std::size_t>(128));
        std::memcpy(mf.data(), payload, std::strlen(payload) + 1U);
        mf.flush();
        printf("  wrote: %s (%zu bytes)\n", payload, mf.size());
    }

    {
        xlib::mapped_file mf(path, X_MEMORY_PROTECT_READ);
        printf("  read:  %s\n", static_cast<const char *>(mf.data()));
    }

    printf("  mapped_file OK\n\n");
    return 0;
}

/* --- process -------------------------------------------------------------- */

static int process_example(const char *helper_path)
{
    printf("Process wrapper...\n");

    char *args[] = { const_cast<char *>(helper_path), nullptr };
    xlib::process proc(helper_path, args);
    int exit_code = proc.wait();
    printf("  helper exited with code %d\n", exit_code);
    printf("  process OK\n\n");
    return 0;
}

/* --- directory_iterator --------------------------------------------------- */

static int directory_example()
{
    printf("Directory iterator (range-for)...\n");

    int count = 0;
    for (const xlib::directory_entry &entry : xlib::directory(".")) {
        if (entry.name()[0] == '.') {
            continue;
        }
        printf("  %s%s\n", entry.name(), entry.is_directory() ? "/" : "");
        ++count;
    }

    printf("  listed %d entries, directory_iterator OK\n\n", count);
    return 0;
}

/* --- event_loop ----------------------------------------------------------- */

static int event_loop_example()
{
    printf("Event loop wrapper...\n");

    xlib::event_loop loop;
    loop.cancel();

    printf("  event_loop OK\n\n");
    return 0;
}

/* --- std::chrono integration ---------------------------------------------- */

static int chrono_example()
{
    printf("std::chrono integration...\n");

    auto t0 = xlib::chrono::monotonic_now();

    xlib::chrono::sleep(std::chrono::milliseconds(5));

    auto t1 = xlib::chrono::monotonic_now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    printf("  elapsed: %lld ms\n", static_cast<long long>(elapsed));
    printf("  chrono OK\n\n");
    return 0;
}

/* --- socket_address new helpers ------------------------------------------- */

static int socket_address_example()
{
    printf("socket_address helpers (from_string / to_string / family)...\n");

    auto addr4 = xlib::socket_address::from_string("127.0.0.1", 9000, X_ADDRESS_FAMILY_IPV4);
    printf("  IPv4 to_string: %s port %u\n",
           addr4.to_string().c_str(),
           static_cast<unsigned>(addr4.port()));
    printf("  family: %s\n",
           addr4.family() == X_ADDRESS_FAMILY_IPV6 ? "IPv6" : "IPv4");

    auto addr6 = xlib::socket_address::from_string("::1", 9000, X_ADDRESS_FAMILY_IPV6);
    printf("  IPv6 to_string: %s\n", addr6.to_string().c_str());

    printf("  socket_address OK\n\n");
    return 0;
}

/* -------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int result = 0;

    result |= synchronization_example();
    result |= mapped_file_example();
    result |= directory_example();
    result |= event_loop_example();
    result |= chrono_example();
    result |= socket_address_example();

    if (argc >= 2) {
        result |= process_example(argv[1]);
    } else {
        printf("(skipping process example: no helper path provided)\n\n");
    }

    if (result == 0) {
        printf("All v1.7 C++ wrapper examples passed.\n");
    }

    return result;
}
