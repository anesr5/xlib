# xlib

---

## What is xlib?

**xlib** is a small MIT-licensed systems programming library for C and C++.

It provides a portable abstraction layer over common operating system APIs such as threads, mutexes, files, memory mapping, dynamic libraries, sockets, processes, IPC, timers, and event loops.

Instead of writing platform-specific code like this:

```c
#ifdef _WIN32
    /* Win32 API */
#else
    /* POSIX API */
#endif
```

xlib gives you one consistent API that maps internally to the native backend on each platform.

```c
#include <xlib/thread.h>
#include <stdio.h>

static int worker(void *data) {
    (void)data;
    puts("hello from xlib");
    return 0;
}

int main(void) {
    x_thread_t *thread = NULL;

    if (x_thread_create(&thread, worker, NULL) != 0) {
        return 1;
    }

    x_thread_join(thread, NULL);
    x_thread_destroy(thread);
    return 0;
}
```

---

## Why xlib?

Portable systems programming in C and C++ is still painful.

| Problem           | Linux / macOS       | Windows                               | xlib                  |
| ----------------- | ------------------- | ------------------------------------- | --------------------- |
| Threads           | `pthread`           | Win32 threads                         | `x_thread_*`          |
| Dynamic libraries | `dlopen` / `dlsym`  | `LoadLibrary` / `GetProcAddress`      | `x_dynamic_library_*` |
| Memory mapping    | `mmap`              | `CreateFileMapping` / `MapViewOfFile` | `x_mapped_file_*`     |
| Virtual memory    | `mmap` / `mprotect` | `VirtualAlloc` / `VirtualProtect`     | `x_virtual_memory_*`  |
| Sockets           | POSIX sockets       | Winsock                               | `x_socket_*`          |
| Event polling     | `epoll` / `kqueue`  | Windows polling backend               | `x_event_loop_*`      |

xlib focuses on being:

* **Small** — useful primitives, not a giant framework.
* **Portable** — Linux, Windows, and macOS are first-class targets.
* **C-first** — easy to embed in C and C++ projects.
* **C++ friendly** — optional header-only RAII wrappers.
* **Build-system friendly** — CMake support, install rules, and package config files.
* **Dependency-light** — native OS APIs where possible.

---

## Current status

| Item                | Status                                     |
| ------------------- | ------------------------------------------ |
| Current version     | `2.5.0`                                    |
| License             | MIT                                        |
| Language            | C with optional C++ wrappers               |
| Build system        | CMake                                      |
| Supported platforms | Linux, Windows, macOS                      |
| ABI stability       | Not guaranteed before the stable v3.x line |
| Project status      | Active development                         |

> xlib is usable for experiments, tools, learning, and small systems projects. For large production systems, review the API surface, tests, and platform behavior before adopting it.

---

## Features

### Core systems primitives

* Thread creation, joining, sleeping, and destruction
* Thread options, including custom stack size
* Mutexes and recursive mutexes
* Condition variables
* Counting semaphores
* Thread-local storage
* Read-write locks
* Barrier synchronization
* One-time initialization with `x_once`
* Thread pool with configurable worker count
* Thread affinity helpers

### Time

* Monotonic clock
* System clock
* Millisecond and microsecond sleep helpers
* High-resolution elapsed timers

### Memory

* Page size detection
* Virtual memory allocation
* Virtual memory protection changes
* Read-only and read-write memory-mapped files
* Mapped file resizing
* Shared memory primitives
* Allocation dispatch helpers

### Filesystem

* File open, read, write, seek, size, close, and remove
* Directory creation, removal, opening, and iteration
* Path helpers
* UTF-8 public path handling on Windows
* File watcher API
* Async whole-file read helper

### Dynamic libraries

* Runtime library loading
* Symbol lookup
* Cross-platform error reporting

### Networking

* TCP and UDP sockets
* IPv4 and IPv6 support
* Dual-stack socket helpers
* Address resolution
* Multi-result hostname resolution
* Address formatting and parsing
* Peer and local address retrieval
* Non-blocking sockets
* Socket shutdown helpers
* Socket options
* Send and receive timeouts
* Multicast helpers
* Network interface enumeration
* Platform-consistent would-block checks

### TLS hooks

* Pluggable TLS backend API
* TLS stream wrapper over sockets
* Certificate and private-key loading hooks
* SNI and hostname verification helpers
* Event-loop TLS handshake source

> TLS support is designed as a pluggable integration layer. A real TLS backend must be provided or enabled for secure networking use.

### Processes and IPC

* Process creation
* Waiting, polling, termination, and exit code retrieval
* Standard input, output, and error redirection
* Anonymous pipes
* Child environment blocks
* Environment variable helpers
* Two-stage process pipeline helper
* Named shared memory
* Named semaphores
* Named cross-process mutexes
* Named byte-stream pipes
* Fixed-size inter-process message queues

### Event loop

* Timer events
* Socket readiness events
* Process exit events
* File watcher events
* File read/write event sources
* Pipe read/write event sources
* POSIX signal event sources
* TLS handshake event sources
* Wake and cancellation APIs
* Repeating timer drift correction
* Linux `epoll` backend
* macOS / BSD `kqueue` backend
* Windows polling backend

### C++ wrappers

* Header-only RAII wrappers through `xlib/xlib.hpp`
* Thread, mutex, condition, semaphore, and TLS wrappers
* Timer wrappers with `std::chrono` support
* Process wrapper
* Event loop wrapper
* Memory mapping wrapper
* Directory range-for iterator
* Networking wrappers
* `[[nodiscard]]` annotations on value-returning methods
* Error helpers based on `std::system_error`

---

## Installation

### Requirements

* CMake 3.16+
* A C compiler
* Optional: a C++ compiler for the C++ wrappers and examples

### Build from source

```bash
git clone https://github.com/anesr5/xlib.git
cd xlib


cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Install

```bash
cmake -B build -DXLIB_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=/your/install/prefix
cmake --build build
cmake --install build
```

### Use from CMake

```cmake
find_package(xlib CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE xlib::xlib)
```

### Use in C

```c
#include <xlib/xlib.h>
```

### Use in C++

```cpp
#include <xlib/xlib.hpp>
```

---

## CMake presets

For local development:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

For sanitizer builds on supported GCC/Clang platforms:

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers --output-on-failure
```

---

## Examples

The `examples/` directory contains focused examples for the public modules.

| Example                          | What it shows                                        |
| -------------------------------- | ---------------------------------------------------- |
| `thread_example.c`               | Basic thread creation and joining                    |
| `threading_primitives_example.c` | Mutexes, conditions, semaphores, TLS, worker threads |
| `time_example.c`                 | Clocks, timers, and sleep helpers                    |
| `memory_example.c`               | Virtual memory, protection flags, mapped files       |
| `filesystem_example.c`           | File I/O, directories, paths, UTF-8 filenames        |
| `dynamic_library_example.c`      | Loading a plugin and resolving symbols               |
| `network_example.c`              | Loopback TCP and UDP checks                          |
| `tcp_server_example.c`           | Minimal TCP echo server                              |
| `tcp_client_example.c`           | Minimal TCP client                                   |
| `process_example.c`              | Process creation, stdio redirection, exit codes      |
| `event_example.c`                | Timers and socket readiness in an event loop         |
| `ipc_example.c`                  | Shared memory, named semaphores, mapped files        |
| `diagnostics_example.c`          | Logging, diagnostics, tracing, assertions            |
| `cpp_wrapper_example.cpp`        | C++ RAII wrappers                                    |

Run most examples through CTest:

```bash
ctest --test-dir build --output-on-failure
```

Run the TCP examples manually:

```bash
./build/xlib_tcp_server_example 8080
./build/xlib_tcp_client_example 127.0.0.1 8080 "hello from xlib"
```

---

## Documentation

* [API Reference](docs/API.md)
* [Event System](docs/EVENTS.md)
* [Examples Guide](docs/EXAMPLES.md)
* [CI and Platform Validation](docs/CI.md)

---

## When should you use xlib?

xlib is a good fit for:

* cross-platform command-line tools
* systems utilities
* small servers and agents
* runtime prototypes
* game engine tooling
* educational systems programming projects
* C/C++ projects that want portable OS primitives without adopting a large framework

xlib may not be the best fit if you need:

* a fully mature production async runtime
* a high-level networking framework
* a guaranteed stable ABI today
* a batteries-included TLS implementation without configuring a backend
* a replacement for large frameworks such as Boost.Asio, libuv, APR, or GLib

---

## Roadmap

### Near term

* Improve release packaging
* Publish tagged GitHub releases
* Expand real-world examples
* Improve documentation for each module
* Add more platform edge-case tests

### Later

* Stable ABI policy for the v3.x line
* Package manager support
* Stronger fuzzing and stress testing
* More complete native async I/O backends
* Extended service / daemon support

See the documentation directory for more detailed development notes.

---

## License

xlib is licensed under the MIT License.

See [LICENSE](LICENSE) for details.
