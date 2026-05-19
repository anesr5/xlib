# Event System

xlib's event system provides a small source-based event loop for timers, socket readiness, process exits, and file watcher polling.

## Event Sources

Event sources are represented by opaque `x_event_source_t` handles. A source can remove itself from inside its callback with `x_event_source_remove`. Call `x_event_source_type` to inspect the source subtype in generic dispatch code.

Supported source types:

- Timers through `x_event_loop_add_timer`
- Socket readiness through `x_event_loop_add_socket`
- Process exits through `x_event_loop_add_process`
- File watcher changes through `x_event_loop_add_file_watcher`
- File read/write readiness through `x_event_loop_add_file`
- Pipe readiness through `x_event_loop_add_pipe`
- POSIX signals through `x_event_loop_add_signal`
- TLS handshake completion through `x_event_loop_add_tls_handshake`
- Internal wake notifications used by `x_event_loop_wake` and `x_event_loop_cancel`

## Wake and Cancel

`x_event_loop_wake` interrupts a blocked event loop without stopping it. This is useful when another thread has changed state that the event-loop thread should observe.

`x_event_loop_cancel` requests the loop to stop and wakes it if it is blocked. `x_event_loop_stop` also requests stop and performs a best-effort wake.

The wake mechanism is implemented with an internal loopback UDP socket so it works with the same readiness backend as normal socket events.

## Timers

One-shot timers use an interval of `0`.

Repeating timers advance from their previous due time rather than from the callback dispatch time. This reduces long-term drift when callbacks run slightly late.

## Process and File Sources

Process sources poll `x_process_poll` on the requested interval and dispatch `X_EVENT_PROCESS` once the child exits.

File watcher sources poll an existing `x_file_watcher_t` and dispatch `X_EVENT_FILE` together with `X_EVENT_FILE_CREATED`, `X_EVENT_FILE_MODIFIED`, or `X_EVENT_FILE_DELETED`.

File I/O sources dispatch read/write readiness for regular files through `x_event_loop_add_file`. Regular files are considered ready on each requested interval; callbacks perform the actual `x_file_read` / `x_file_write` work and remove the source when complete.

Pipe sources put the pipe into non-blocking mode, poll `x_pipe_poll` on the requested interval, and dispatch `X_EVENT_READ` and/or `X_EVENT_WRITE`. This supports event-loop streaming of child stdout/stderr pipes created through `x_process_options_t`.

Signal sources are available on POSIX platforms through `x_event_loop_add_signal`; they install a minimal signal handler and dispatch `X_EVENT_SIGNAL` from the event loop. On Windows this API returns `ENOSYS`.

TLS handshake sources call `x_tls_stream_handshake` on the requested interval and dispatch `X_EVENT_TLS` when the backend reports completion; `EAGAIN`/`EWOULDBLOCK` keeps the source pending.

## Backends

- Linux uses `epoll`, with `pidfd_open` for process exits and `inotify` for filesystem watcher sources when available.
- macOS and BSD platforms use `kqueue`.
- Windows currently uses a `select`-based polling backend.

The CI workflow includes explicit `xlib_event_example` smoke runs on each supported runner family.

## Current Scope

v2.2 covers timer, socket, process, file watcher, file I/O, pipe, and POSIX signal sources. Process and filesystem watcher sources use native non-polling backends where available and fall back to portable polling otherwise.
