# Event System

xlib's event system provides a small source-based event loop for timers, socket readiness, process exits, and file watcher polling.

## Event Sources

Event sources are represented by opaque `x_event_source_t` handles. A source can remove itself from inside its callback with `x_event_source_remove`.

Supported source types:

- Timers through `x_event_loop_add_timer`
- Socket readiness through `x_event_loop_add_socket`
- Process exits through `x_event_loop_add_process`
- File watcher changes through `x_event_loop_add_file_watcher`
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

## Backends

- Linux uses `epoll`.
- macOS and BSD platforms use `kqueue`.
- Windows currently uses a `select`-based polling backend.

The CI workflow includes explicit `xlib_event_example` smoke runs on each supported runner family.

## Current Scope

v1.4 covers timer, socket, process, and snapshot-based file watcher sources. The process and file watcher sources are portable polling sources, while socket readiness uses the native backend where available.
