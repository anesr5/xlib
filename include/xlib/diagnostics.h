#ifndef XLIB_DIAGNOSTICS_H
#define XLIB_DIAGNOSTICS_H

#include <stddef.h>
#include <stdint.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- log levels ---------------------------------------------------- */

typedef enum x_log_level {
    X_LOG_LEVEL_DEBUG = 0,
    X_LOG_LEVEL_INFO  = 1,
    X_LOG_LEVEL_WARN  = 2,
    X_LOG_LEVEL_ERROR = 3
} x_log_level_t;

/* ---------- log hook ------------------------------------------------------ */

/*
 * Callback invoked for every log message emitted by xlib.
 *
 * level      one of X_LOG_LEVEL_*.
 * module     short tag identifying the subsystem (e.g. "network", "thread").
 * message    human-readable, newline-terminated message.
 * user_data  the pointer registered with x_log_set_hook.
 */
typedef void (*x_log_hook_t)(
    int level,
    const char *module,
    const char *message,
    void *user_data);

/*
 * Installs a global log hook.  Passing NULL removes the current hook.
 * The hook is not called from signal handlers or async-cancel contexts.
 * Not thread-safe with respect to itself; callers must serialise hook
 * installation if multiple threads may call x_log_set_hook concurrently.
 */
XLIB_API void x_log_set_hook(x_log_hook_t hook, void *user_data);

/*
 * Returns the currently installed log hook, or NULL if none.
 */
XLIB_API x_log_hook_t x_log_get_hook(void);

/* ---------- runtime diagnostics callback ---------------------------------- */

/*
 * Structured event delivered to the diagnostics hook.
 */
typedef struct x_diag_event {
    const char *operation;   /* e.g. "x_socket_bind"                        */
    int         error_code;  /* errno-style code returned by the operation   */
    const char *context;     /* optional extra detail; may be NULL           */
} x_diag_event_t;

/*
 * Callback invoked after any xlib public function returns a non-zero error
 * code, when a hook is installed.
 */
typedef void (*x_diag_hook_t)(
    const x_diag_event_t *event,
    void *user_data);

/*
 * Installs a global diagnostics hook.  Passing NULL removes the current hook.
 */
XLIB_API void x_diag_set_hook(x_diag_hook_t hook, void *user_data);

/*
 * Returns the currently installed diagnostics hook, or NULL if none.
 */
XLIB_API x_diag_hook_t x_diag_get_hook(void);

/* ---------- tracing hook -------------------------------------------------- */

typedef enum x_trace_phase {
    X_TRACE_ENTER = 0,
    X_TRACE_EXIT  = 1
} x_trace_phase_t;

/*
 * Callback invoked at the entry and exit of public API functions when
 * tracing is enabled (XLIB_ENABLE_TRACING defined at build time).
 *
 * phase       X_TRACE_ENTER or X_TRACE_EXIT.
 * function    name of the function (e.g. "x_thread_create").
 * timestamp   monotonic timestamp in nanoseconds, or 0 if unavailable.
 * user_data   the pointer registered with x_trace_set_hook.
 */
typedef void (*x_trace_hook_t)(
    int phase,
    const char *function,
    uint64_t timestamp,
    void *user_data);

/*
 * Installs a global tracing hook.  Passing NULL removes the current hook.
 */
XLIB_API void x_trace_set_hook(x_trace_hook_t hook, void *user_data);

/*
 * Returns the currently installed tracing hook, or NULL if none.
 */
XLIB_API x_trace_hook_t x_trace_get_hook(void);

/* ---------- internal emit helpers ----------------------------------------- */

/*
 * These are used internally by other xlib modules.  They are part of the
 * public header so that inline macros and static-inline wrappers in other
 * headers can reach them, but they are not intended for direct use.
 */
XLIB_API void x_log_emit(int level, const char *module, const char *format, ...);
XLIB_API void x_diag_emit(const char *operation, int error_code, const char *context);
XLIB_API void x_trace_emit(int phase, const char *function);

/* ---------- debug assertions ---------------------------------------------- */

/*
 * X_ASSERT(condition, message)
 *
 * In debug builds (NDEBUG not defined), evaluates condition and aborts with
 * a diagnostic message when it is false.  In release builds (NDEBUG defined),
 * expands to nothing.
 */
#ifdef NDEBUG
#define X_ASSERT(condition, message) ((void)0)
#else
XLIB_API void x_assert_fail(const char *expression, const char *message,
                             const char *file, int line);

#define X_ASSERT(condition, message) \
    ((condition) ? (void)0 : x_assert_fail(#condition, (message), __FILE__, __LINE__))
#endif

#ifdef __cplusplus
}
#endif

#endif
