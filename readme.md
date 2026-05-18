# xlib

**xlib** is a lightweight MIT-licensed cross-platform low-level systems library for C and C++.

Current version: `1.9.0`

It provides a small portable abstraction layer over operating system APIs, allowing developers to write low-level code once and compile it across Linux, Windows, and macOS.

Instead of directly using platform-specific APIs such as `pthread`, Win32 threads, `mmap`, `dlopen`, or `LoadLibrary`, xlib exposes a unified API and maps it internally to the correct native backend.

---

## Features

Current features include:

- Cross-platform thread creation and joining
- Mutexes and recursive mutexes
- Condition variables
- Counting semaphores
- Thread-local storage
- Monotonic and system clocks
- High-resolution elapsed timers
- Virtual memory allocation and protection
- Page size detection
- Read-write memory-mapped files
- Read-only memory-mapped files
- Mapped file open with configurable protection
- Named shared memory regions
- Named counting semaphores
- Pluggable global allocator
- Allocation dispatch helpers (`x_alloc`, `x_realloc`, `x_free`)
- File I/O
- Directory creation and iteration
- Path utilities
- UTF-8 path handling on Windows
- File watcher API
- File watcher event sources
- Async whole-file read helper
- Dynamic library loading
- Dynamic symbol lookup
- Dynamic library error reporting
- TCP and UDP sockets
- IPv6 TCP and UDP sockets
- Address resolution (IPv4 and IPv6)
- Address formatting and parsing (`inet_ntop` / `inet_pton` wrappers)
- Address family detection
- Non-blocking sockets
- Socket options
- Socket send and receive timeouts
- IPv4 multicast join and leave helpers
- UDP convenience bind helper
- TLS integration hook API (pluggable backend)
- Process creation, waiting, and termination
- Non-blocking process polling
- Exit code retrieval
- Anonymous pipe handles
- Two-stage process pipeline helper
- Environment variable helpers
- Per-child environment blocks
- Standard input/output/error redirection for child processes
- Event loop support
- Timer events
- Socket readiness events
- Process exit events
- File change events
- Event-loop wake and cancellation APIs
- Repeating timer drift correction
- Linux `epoll`, macOS/BSD `kqueue`, and Windows polling backends
- Header-only C++ RAII wrappers
- C++ RAII process wrapper
- C++ RAII event-loop wrapper
- C++ RAII memory mapping wrapper
- C++ RAII condition and semaphore wrappers
- C++ range-for directory iterator
- `std::chrono` integration for time and sleep APIs
- `[[nodiscard]]` annotations on all value-returning C++ methods
- C++ error utilities based on `std::system_error`
- Cross-platform terminal/TTY helpers
- Millisecond sleep utilities
- Microsecond sleep utilities
- CMake build support
- Example programs
- CI coverage for Linux, Windows, and macOS
- Package install validation
- Optional sanitizer builds on supported GCC/Clang platforms
- Structured logging hooks
- Runtime diagnostics callbacks
- Optional tracing hooks
- Debug assertions for invalid API usage
- C++ error category support (`std::error_category` subclass)

Planned features include:

- Full networking overhaul (IPv6, address families)

---

## Goals

xlib aims to be:

- Lightweight
- Portable
- Easy to build
- Dependency-free where possible
- Friendly to both C and C++
- Suitable for systems programming
- Simple enough to embed into other projects

---

## Why xlib?

Writing portable low-level C/C++ code is difficult because every operating system has different APIs.

For example:

| Feature | Linux/macOS | Windows |
|---|---|---|
| Threads | `pthread` | Win32 threads |
| Dynamic libraries | `dlopen` | `LoadLibrary` |
| Virtual memory | `mmap` | `VirtualAlloc` |
| File descriptors | POSIX file descriptors | Windows handles |
| Event polling | `epoll` / `kqueue` | IOCP / Win32 APIs |

xlib provides a common API over these platform-specific systems.

---

## Example

```c
#include <xlib/thread.h>
#include <stdio.h>

int worker(void *data) {
    printf("Hello from xlib thread!\n");
    return 0;
}

int main(void) {
    x_thread_t *thread;

    if (x_thread_create(&thread, worker, NULL) != 0) {
        return 1;
    }

    x_thread_join(thread, NULL);
    x_thread_destroy(thread);

    return 0;
}
```

---

## Roadmap

### v1.1 - Documentation and Packaging

- [x] Full API reference
- [x] More examples for each module
- [x] Contribution guide
- [x] Versioning and compatibility policy
- [x] CMake package config files
- [x] Install/export validation
- [x] Package manager integration planning

### v1.2 - CI and Platform Hardening

- [x] CI for Linux, Windows, and macOS
- [x] Compiler matrix for GCC, Clang, MSVC, and MinGW
- [x] Sanitizer builds where supported
- [x] Cross-platform test coverage expansion
- [x] POSIX backend validation on Linux and macOS
- [x] Windows API edge-case hardening

### v1.3 - Event System Expansion

- [x] Stable timer and socket event-source abstractions
- [x] Event-loop wake APIs
- [x] Event-loop cancellation APIs
- [x] Repeating timer drift correction
- [x] Timer precision and drift smoke tests
- [x] Backend-specific CI event smoke tests

### v1.4 - Filesystem and Process Extensions

- [x] File event sources
- [x] Process event sources
- [x] File watcher API
- [x] Async filesystem operations
- [x] Advanced process pipelines
- [x] Pipe handle support
- [x] Environment block support for child processes
- [x] Cross-platform terminal/TTY utilities

### v1.5 - IPC and Memory Extensions

- [x] Shared memory primitives
- [x] Inter-process synchronization
- [x] Named mutexes or semaphores
- [x] Memory allocation hooks
- [x] Pluggable allocator support
- [x] More memory mapping modes

### v1.6 - Networking Extensions

- [x] Expanded IPv6 support
- [x] TLS socket integration hooks
- [x] Socket timeout helpers
- [x] Multicast helpers
- [x] Better address formatting and parsing
- [x] UDP convenience APIs

### v1.7 - C++ API Growth

- [x] More complete C++20 wrappers
- [x] RAII process wrapper
- [x] RAII event-loop wrapper
- [x] RAII memory mapping wrapper
- [x] C++ iterator support for directories
- [x] `std::chrono` integration

### v1.8 - Diagnostics and Observability

- [x] Structured logging hooks
- [x] Error category support for C++
- [x] Runtime diagnostics callbacks
- [x] Optional tracing hooks
- [x] Debug assertions for invalid API usage

### v1.9 - ABI and Release Hardening

- [x] ABI compatibility policy and tooling
- [x] Symbol visibility controls
- [x] Public header audit
- [x] Long-running stress tests
- [x] Release checklist automation
- [x] Extended platform support evaluation

### v2.0 - IPv6 and Full Networking Overhaul

- [ ] Full dual-stack IPv6 support across all socket APIs
- [ ] Address family abstraction (`AF_INET` / `AF_INET6` unified)
- [ ] `x_address_t` redesign to carry both v4 and v6 natively
- [ ] Hostname resolution returning multiple results with iteration
- [ ] Peer address retrieval on accepted/connected sockets
- [ ] Socket `shutdown()` support (read, write, both)
- [ ] Platform-consistent `EWOULDBLOCK` / `EAGAIN` unification
- [ ] Network interface enumeration API

### v2.1 - Complete C++ Wrapper Surface

- [ ] `xlib::process` RAII wrapper with wait/terminate/exit-code
- [ ] `xlib::event_loop` RAII wrapper
- [ ] `xlib::mapped_file` RAII wrapper
- [ ] `xlib::directory` / `xlib::directory_iterator` (range-for compatible)
- [ ] `xlib::condition` RAII wrapper
- [ ] `xlib::semaphore` RAII wrapper
- [ ] `xlib::tls_key<T>` typed thread-local storage wrapper
- [ ] `std::chrono` integration for all time and sleep APIs
- [ ] C++17 `[[nodiscard]]` annotations on error-returning wrappers

### v2.2 - Async I/O Foundation

- [ ] Non-blocking file read/write event sources for the event loop
- [ ] Native non-polling process exit backends where available
- [ ] Native filesystem change watcher backends where available
- [ ] Signal event sources on POSIX (`signalfd` / `kqueue EVFILT_SIGNAL`)
- [ ] Pipe read/write event sources
- [ ] Child process stdout/stderr streaming via event loop
- [ ] Unified `x_event_source` subtype for all new sources

### v2.3 - IPC and Shared Memory

- [ ] Named shared memory segments
- [ ] Named mutexes and semaphores (cross-process synchronization)
- [ ] Cross-platform UNIX domain / named pipe support
- [ ] Inter-process message queue primitives
- [ ] Shared memory with configurable protection flags
- [ ] Memory-mapped file resize API

### v2.4 - TLS and Secure Networking

- [ ] TLS stream wrapper over `x_socket_t` (pluggable backend)
- [ ] Certificate and key loading helpers
- [ ] Hostname verification support
- [ ] Non-blocking TLS handshake integration with event loop
- [ ] Optional bundled backend (e.g. mbedTLS or BearSSL)
- [ ] SNI support

### v2.5 - Advanced Threading

- [ ] Read-write lock (`x_rwlock_t`) API
- [ ] Barrier synchronization primitive
- [ ] Thread pool with configurable worker count
- [ ] Thread affinity / CPU pinning helpers
- [ ] `once` primitive (`x_once_t`) for one-time initialization
- [ ] Per-thread stack size configuration in `x_thread_create`

### v2.6 - Pluggable Allocator and Memory Diagnostics

- [ ] Global pluggable allocator (`x_allocator_t` vtable)
- [ ] Per-arena allocation scopes
- [ ] Allocation tracking and high-watermark reporting
- [ ] Guard-page allocator option for debugging
- [ ] Stack allocator primitive for temporary allocations
- [ ] Integration with sanitizer allocators in debug builds

### v2.7 - Observability and Structured Diagnostics

- [ ] Structured error type carrying operation name, code, and context
- [ ] C++ `std::error_category` subclass for all xlib error codes
- [ ] Optional callback-based logging sink (`x_log_hook_t`)
- [ ] Per-module log verbosity control
- [ ] Debug-build API misuse assertions (double-free, use-after-free guards)
- [ ] Compile-time feature detection macros (`XLIB_HAS_EPOLL` etc.)
- [ ] Optional perf-counter hooks for profiling integrations

### v2.8 - Security and Hardening

- [ ] Mandatory `O_CLOEXEC` / `SOCK_CLOEXEC` enforcement audit (all platforms)
- [ ] Privilege-drop helper (`setuid`/`setgid` wrappers)
- [ ] Secure memory zeroing (`x_memory_secure_zero`) resistant to optimisation
- [ ] `MAP_ANONYMOUS` guard pages around sensitive allocations
- [ ] Capability-based fd passing (`SCM_RIGHTS`) on POSIX
- [ ] Stack-smashing / buffer overflow hardening in all path utilities
- [ ] Fuzz-testing harnesses for filesystem, network, and process APIs

### v2.9 - Stable ABI and Long-Term Support

- [ ] Frozen, versioned ABI with compatibility guarantees across minor versions
- [ ] `XLIB_API` visibility macros applied to every public symbol
- [ ] Static and shared library both fully validated
- [ ] Symbol versioning scripts (`.map` / `.def`) for all platforms
- [ ] Stable C ABI test suite (binary compatibility regression tests)
- [ ] Official package manager presence (vcpkg, Conan, Homebrew)
- [ ] Long-term support commitment and security patch policy
- [ ] Complete API reference generated from source (Doxygen / Sphinx)
- [ ] Migration guide from v1.x to v3.0


## v3.0 - Stable Platform Layer Release

- [ ] Official stable C ABI baseline
- [ ] Finalized v3 public API surface
- [ ] Removal or deprecation of legacy v1/v2 transitional APIs
- [ ] Complete migration guide from v2.x to v3.0
- [ ] Strict semantic versioning policy enforcement
- [ ] ABI compatibility regression suite enabled in CI
- [ ] Full Linux, macOS, Windows release validation
- [ ] Official source and binary release artifacts
- [ ] Package manager publishing: vcpkg, Conan, Homebrew
- [ ] Long-term support branch created for v3.x

## v3.1 - Platform and Hardware Introspection

- [ ] OS detection API
- [ ] OS version detection API
- [ ] CPU architecture detection
- [ ] CPU feature detection: SSE, AVX, NEON, etc.
- [ ] CPU core and hardware thread count API
- [ ] Cache line size detection
- [ ] Endianness detection
- [ ] Page size and allocation granularity API refinement
- [ ] Runtime feature query API: `x_feature_available`
- [ ] Compile-time feature macros: `XLIB_PLATFORM_WINDOWS`, `XLIB_ARCH_X64`, etc.

## v3.2 - Atomics and Lock-Free Primitives

- [ ] Portable atomic integer API
- [ ] Portable atomic pointer API
- [ ] Memory-order abstraction
- [ ] Spinlock primitive
- [ ] Once-cell / lazy initialization helper
- [ ] Single-producer single-consumer queue
- [ ] Multi-producer single-consumer queue
- [ ] Atomic reference counter primitive
- [ ] Lock-free ring buffer
- [ ] C++ wrappers around atomic and lock-free primitives


## v3.3 - Advanced Filesystem Layer

- [ ] Path normalization API
- [ ] Canonical path resolution
- [ ] Relative path computation
- [ ] Parent path, extension, stem, and filename helpers
- [ ] Symlink creation, reading, and resolution
- [ ] Hard link support
- [ ] File copy, move, and recursive remove helpers
- [ ] Temporary file and temporary directory APIs
- [ ] Atomic file replacement API
- [ ] Filesystem permissions abstraction
- [ ] Cross-platform file locking API


## v3.4 - Native Async I/O Backends

- [ ] Linux `io_uring` backend evaluation
- [ ] Windows IOCP backend
- [ ] macOS native async I/O backend evaluation
- [ ] Async file read/write operations
- [ ] Async socket operations
- [ ] Async pipe operations
- [ ] Async cancellation support
- [ ] Async timeout support
- [ ] Event loop integration for native async backends
- [ ] Portable fallback backend


## v3.5 - Runtime and Scheduler Layer

- [ ] Generic task scheduler
- [ ] Work-stealing thread pool
- [ ] Future/promise-style task handles
- [ ] Task cancellation tokens
- [ ] Task priorities
- [ ] Delayed task scheduling
- [ ] Periodic task scheduling
- [ ] Event-loop-backed task executor
- [ ] Blocking and non-blocking task wait APIs
- [ ] C++ task wrapper integration


## v3.6 - Advanced Memory Management

- [ ] Arena allocator
- [ ] Pool allocator
- [ ] Stack allocator
- [ ] Slab allocator
- [ ] Aligned allocation API
- [ ] NUMA-aware allocation investigation
- [ ] Allocation tagging
- [ ] Memory usage snapshots
- [ ] Leak reporting utilities
- [ ] Debug allocator with red zones
- [ ] Optional guard-page allocator integration


## v3.7 - Binary Data and Serialization Primitives

- [ ] Byte buffer API
- [ ] Dynamic buffer API
- [ ] Binary reader/writer helpers
- [ ] Endian-aware integer encoding and decoding
- [ ] Varint encoding helpers
- [ ] Length-prefixed message framing
- [ ] Checksum helpers
- [ ] Stream abstraction over files, sockets, memory, and pipes
- [ ] Zero-copy buffer slices
- [ ] C++ span/string-view integration where available

## v3.8 - System Service and Daemon Support

- [ ] POSIX daemonization helper
- [ ] Windows service wrapper
- [ ] Service install/uninstall helpers on Windows
- [ ] Service start/stop/status helpers
- [ ] Signal handling integration for services
- [ ] Graceful shutdown coordination
- [ ] PID file helper on POSIX
- [ ] Logging integration for services
- [ ] Crash restart policy helpers
- [ ] Process supervision example application

## v3.9 - Production Hardening and Certification

- [ ] Extended fuzz-testing suite
- [ ] Long-running networking stress tests
- [ ] Long-running filesystem stress tests
- [ ] Long-running process and IPC stress tests
- [ ] Thread sanitizer test profile
- [ ] Address sanitizer test profile
- [ ] Undefined behavior sanitizer test profile
- [ ] Static analysis integration
- [ ] Public security policy
- [ ] CVE/reporting process
- [ ] Reproducible builds investigation
- [ ] Signed release artifacts

---

## Supported Platforms

Planned support:

- Linux
- Windows
- macOS

Future possible support:

- FreeBSD
- Android
- iOS
- WebAssembly

---

## Build

xlib builds with CMake:

```bash
cmake -B build
cmake --build build
```

Common local configurations are also available through CMake presets:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

To install xlib and use it from another CMake project:

```bash
cmake -B build -DXLIB_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=/path/to/prefix
cmake --build build
cmake --install build
```

```cmake
find_package(xlib CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE xlib::xlib)
```

To run the example:

```bash
./build/xlib_thread_example
./build/xlib_threading_primitives_example
./build/xlib_time_example
./build/xlib_memory_example
./build/xlib_filesystem_example
./build/xlib_dynamic_library_example path/to/xlib_dynamic_library_plugin
./build/xlib_network_example
./build/xlib_tcp_server_example 8080
./build/xlib_tcp_client_example 127.0.0.1 8080 "hello from xlib"
./build/xlib_process_example ./build/xlib_process_helper
./build/xlib_event_example
./build/xlib_filesystem_process_extensions_example ./build/xlib_process_helper
./build/xlib_cpp_wrapper_example path/to/xlib_dynamic_library_plugin
./build/xlib_diagnostics_example
./build/xlib_stress_test
```

---

## Design Philosophy

xlib is not intended to wrap every operating system feature.

Instead, it focuses on a small set of useful low-level primitives that can be implemented cleanly across platforms.

The library should remain:

- Small
- Fast
- Predictable
- Easy to read
- Easy to modify
- Easy to embed

---

## C and C++ Support

The core API is written in C for maximum compatibility.

C++ projects can include `xlib/xlib.hpp` for header-only RAII wrappers and exception-based error handling.

---

## Documentation

- [API Reference](docs/API.md)
- [Event System](docs/EVENTS.md)
- [Examples Guide](docs/EXAMPLES.md)
- [CI and Platform Validation](docs/CI.md)

---

## Contributing

Contributions are welcome once the base structure is ready.

Useful contributions may include:

- Platform-specific implementations
- Tests
- Documentation
- Examples
- Bug fixes
- API design improvements

---

## License

xlib is licensed under the MIT License.

See [LICENSE](LICENSE) for details.
