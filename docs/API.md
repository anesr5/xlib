# xlib API Reference

This document summarizes the public API exposed by xlib. Functions return `0` on success unless otherwise noted. Failures use errno-style integer error codes such as `EINVAL`, `ENOMEM`, `ENOENT`, or platform-mapped equivalents.

All public symbols are exported using the `XLIB_API` macro to ensure correct visibility across platforms and shared/static builds.

## Umbrella Headers

- `#include <xlib/xlib.h>` includes every C module.
- `#include <xlib/xlib.hpp>` includes the C API and header-only C++ RAII wrappers.

## Threading

Header: `xlib/thread.h`

- `x_thread_create`, `x_thread_join`, `x_thread_destroy`
- `x_thread_sleep_ms`
- `x_mutex_create`, `x_recursive_mutex_create`, `x_mutex_lock`, `x_mutex_try_lock`, `x_mutex_unlock`, `x_mutex_destroy`
- `x_condition_create`, `x_condition_wait`, `x_condition_signal`, `x_condition_broadcast`, `x_condition_destroy`
- `x_semaphore_create`, `x_semaphore_wait`, `x_semaphore_post`, `x_semaphore_destroy`
- `x_tls_key_create`, `x_tls_set`, `x_tls_get`, `x_tls_key_destroy`

## Time

Header: `xlib/time.h`

- `x_time_monotonic_ns` returns monotonic nanoseconds for elapsed-time measurement.
- `x_time_system_ns` returns Unix epoch nanoseconds from the system clock.
- `x_time_sleep_ms` and `x_time_sleep_us` sleep for millisecond or microsecond durations.
- `x_timer_start` and `x_timer_elapsed_ns` provide a small elapsed timer helper.

## Memory

Header: `xlib/memory.h`

- `x_memory_page_size`
- `x_virtual_memory_alloc`, `x_virtual_memory_protect`, `x_virtual_memory_free`
- `x_mapped_file_create`, `x_mapped_file_data`, `x_mapped_file_size`, `x_mapped_file_flush`, `x_mapped_file_destroy`

Protection flags:

- `X_MEMORY_PROTECT_NONE`
- `X_MEMORY_PROTECT_READ`
- `X_MEMORY_PROTECT_WRITE`
- `X_MEMORY_PROTECT_EXECUTE`

## Filesystem

Header: `xlib/filesystem.h`

- `x_file_open`, `x_file_read`, `x_file_write`, `x_file_seek`, `x_file_size`, `x_file_close`, `x_file_remove`
- `x_directory_create`, `x_directory_remove`, `x_directory_open`, `x_directory_next`, `x_directory_close`
- `x_path_join`, `x_path_basename`, `x_path_is_absolute`
- `x_file_watcher_create`, `x_file_watcher_poll`, `x_file_watcher_destroy`
- `x_async_file_read_all`, `x_async_file_read_wait`, `x_async_file_read_destroy`

Windows paths are UTF-8 at the public API boundary and are converted internally to wide Win32 paths.

File watchers are snapshot-based and report created, modified, and deleted changes through `X_FILE_WATCH_CREATED`, `X_FILE_WATCH_MODIFIED`, and `X_FILE_WATCH_DELETED`.

## Dynamic Libraries

Header: `xlib/dynamic_library.h`

- `x_dynamic_library_open`
- `x_dynamic_library_symbol`
- `x_dynamic_library_close`
- `x_dynamic_library_last_error`

The backend maps to `LoadLibraryW`/`GetProcAddress` on Windows and `dlopen`/`dlsym` on POSIX platforms.

## Networking

Header: `xlib/network.h`

- `x_address_resolve`, `x_address_resolve_all`, `x_address_list_free`, `x_address_port`
- `x_address_family`, `x_address_to_string`, `x_address_from_string`
- `x_socket_tcp`, `x_socket_udp`, `x_socket_tcp6`, `x_socket_udp6`
- `x_socket_tcp_dual_stack`, `x_socket_udp_dual_stack`
- `x_socket_bind`, `x_socket_listen`, `x_socket_accept`, `x_socket_connect`, `x_socket_shutdown`
- `x_socket_send`, `x_socket_receive`, `x_socket_send_to`, `x_socket_receive_from`
- `x_socket_local_address`, `x_socket_peer_address`
- `x_socket_set_nonblocking`, `x_socket_set_reuse_address`, `x_socket_set_tcp_no_delay`
- `x_socket_set_send_timeout`, `x_socket_set_receive_timeout`
- `x_socket_join_multicast_group`, `x_socket_leave_multicast_group`
- `x_socket_udp_bound`
- `x_network_interfaces`, `x_network_interfaces_free`
- `x_socket_would_block`
- `x_socket_native_handle`, `x_socket_close`

`x_address_t` is an alias for the existing socket-address storage and carries either IPv4 or IPv6 addresses. `x_address_resolve_all` returns every matching address so callers can iterate connection attempts.

## Processes

Header: `xlib/process.h`

- `x_process_start`
- `x_process_wait`
- `x_process_poll`
- `x_process_terminate`
- `x_process_destroy`
- `x_pipe_create`, `x_pipe_read`, `x_pipe_write`, `x_pipe_close`
- `x_process_run_pipeline`
- `x_environment_get`, `x_environment_set`, `x_environment_unset`

`x_process_options_t` supports working-directory selection, standard input/output/error redirection to files or pipes, and per-child environment blocks.

## Events

Header: `xlib/event.h`

- `x_event_loop_create`, `x_event_loop_run`, `x_event_loop_stop`, `x_event_loop_destroy`
- `x_event_loop_wake`, `x_event_loop_cancel`
- `x_event_loop_add_socket`
- `x_event_loop_add_timer`
- `x_event_loop_add_process`
- `x_event_loop_add_file_watcher`
- `x_event_source_remove`

Backends include Linux `epoll`, macOS/BSD `kqueue`, and a portable `select`-based fallback used on Windows. Repeating timers are scheduled from their previous due time to reduce drift. Process and file watcher sources are implemented as portable polling sources on top of the same loop scheduler.

## Terminal

Header: `xlib/terminal.h`

- `x_terminal_is_tty`
- `x_terminal_size`

Terminal helpers work with stream indexes `0`, `1`, and `2` for standard input, output, and error.

## IPC

Header: `xlib/ipc.h`

- `x_shared_memory_create`, `x_shared_memory_open`, `x_shared_memory_data`, `x_shared_memory_size`, `x_shared_memory_close`, `x_shared_memory_unlink`
- `x_named_semaphore_create`, `x_named_semaphore_open`, `x_named_semaphore_wait`, `x_named_semaphore_post`, `x_named_semaphore_close`, `x_named_semaphore_unlink`

## Allocator

Header: `xlib/allocator.h`

- `x_allocator_set`, `x_allocator_reset`, `x_allocator_get`
- `x_alloc`, `x_realloc`, `x_free`

Permits overriding the internal memory allocations of xlib by providing a custom `x_allocator_t` vtable.

## TLS

Header: `xlib/tls.h`

- `x_tls_context_create`, `x_tls_connect`, `x_tls_accept`, `x_tls_read`, `x_tls_write`, `x_tls_close`, `x_tls_context_destroy`

xlib doesn't ship a TLS implementation, but provides an abstraction to easily plug in backend hooks via `x_tls_hooks_t`.

## Diagnostics

Header: `xlib/diagnostics.h`

- `x_log_set_hook`, `x_log_get_hook`
- `x_diag_set_hook`, `x_diag_get_hook`
- `x_trace_set_hook`, `x_trace_get_hook`
- `X_ASSERT` macro

Hooks allow the consumer to intercept formatted log messages, receive structured callback events on API errors, and trace function entry/exit.

## C++ Wrappers

Header: `xlib/xlib.hpp`

The C++ layer is header-only and throws `std::system_error` for failed C API calls.

- `xlib::thread`
- `xlib::mutex`, `xlib::recursive_mutex`, `xlib::lock_guard`
- `xlib::file`
- `xlib::dynamic_library`
- `xlib::socket`, `xlib::socket_address`
- `xlib::directory_entry`, `xlib::directory_iterator`, `xlib::directory`
- `xlib::process`
- `xlib::event_loop`
- `xlib::xlib_error_category`, `xlib::make_xlib_error_code`
- `xlib::log_hook_guard`, `xlib::diag_hook_guard`, `xlib::trace_hook_guard`
- `xlib::check`, `xlib::make_error_code`
- `xlib::chrono::sleep`, `xlib::chrono::sleep_precise`, `xlib::chrono::monotonic_now`, `xlib::chrono::system_now`
