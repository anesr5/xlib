# xlib Examples

The `examples/` directory contains focused smoke examples for each public module.

## Core Examples

- `thread_example.c`: creates and joins one thread.
- `threading_primitives_example.c`: exercises mutexes, recursive mutexes, condition variables, semaphores, TLS, and multiple worker threads.
- `time_example.c`: checks monotonic/system clocks, timers, and sleep helpers.
- `memory_example.c`: checks virtual memory, memory protection, page size, and mapped files.

## System Examples

- `filesystem_example.c`: tests file I/O, directory iteration, path helpers, and UTF-8 filenames.
- `dynamic_library_example.c`: loads a generated plugin and resolves exported symbols.
- `network_example.c`: runs loopback TCP and UDP checks in one process.
- `process_example.c`: starts child processes, redirects stdio, reads environment variables, checks exit codes, and terminates a sleeping child.
- `event_example.c`: combines timer events with loopback TCP socket readiness.
- `filesystem_process_extensions_example.c`: exercises v1.4 file watchers, file watcher event sources, async file reads, process events, child environment blocks, pipes, pipelines, and terminal helpers.
- `ipc_example.c`: checks shared memory regions, named semaphores, pluggable allocator, and mapped files.
- `networking_extensions_example.c`: exercises v2.0 networking APIs including multi-result resolution, dual-stack sockets, peer address lookup, socket shutdown, interface enumeration, UDP convenience binding, multicast helpers, TLS hooks, string address parsing, and timeout APIs.
- `diagnostics_example.c`: demonstrates log hooks, diagnostic callbacks, trace hooks, and assertions.
- `stress_test.c`: a long-running multi-threaded program that continually allocates memory, files, and IPC constructs to validate concurrency and resource handling.

## Standalone Examples

- `tcp_server_example.c`: minimal TCP echo server.
- `tcp_client_example.c`: minimal TCP client.
- `cpp_wrapper_example.cpp`: exercises the header-only C++ wrappers for threads, mutexes, files, dynamic libraries, and sockets.
- `cpp_wrappers_v17_example.cpp`: exercises the v2.1 C++ wrapper surface including typed TLS keys, mapped files, process polling, event timers, range-for directories, chrono timers, resolve-all networking, interface enumeration, and socket helpers.

## Running Examples

Most examples are registered as CTest tests:

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The TCP client and server are intended for manual use:

```bash
./build/xlib_tcp_server_example 8080
./build/xlib_tcp_client_example 127.0.0.1 8080 "hello from xlib"
./build/xlib_stress_test
```
