#include <xlib/diagnostics.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- log hook state ------------------------------------------------ */

static x_log_hook_t  x_log_current_hook;
static void         *x_log_current_user_data;

void x_log_set_hook(x_log_hook_t hook, void *user_data)
{
    x_log_current_hook      = hook;
    x_log_current_user_data = user_data;
}

x_log_hook_t x_log_get_hook(void)
{
    return x_log_current_hook;
}

void x_log_emit(int level, const char *module, const char *format, ...)
{
    x_log_hook_t hook = x_log_current_hook;

    if (hook == NULL || format == NULL) {
        return;
    }

    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        hook(level, module != NULL ? module : "xlib", buffer,
             x_log_current_user_data);
    }
}

/* ---------- diagnostics hook state ---------------------------------------- */

static x_diag_hook_t  x_diag_current_hook;
static void          *x_diag_current_user_data;

void x_diag_set_hook(x_diag_hook_t hook, void *user_data)
{
    x_diag_current_hook      = hook;
    x_diag_current_user_data = user_data;
}

x_diag_hook_t x_diag_get_hook(void)
{
    return x_diag_current_hook;
}

void x_diag_emit(const char *operation, int error_code, const char *context)
{
    x_diag_hook_t hook = x_diag_current_hook;

    if (hook == NULL || operation == NULL || error_code == 0) {
        return;
    }

    {
        x_diag_event_t event;
        event.operation  = operation;
        event.error_code = error_code;
        event.context    = context;

        hook(&event, x_diag_current_user_data);
    }
}

/* ---------- tracing hook state -------------------------------------------- */

static x_trace_hook_t  x_trace_current_hook;
static void           *x_trace_current_user_data;

void x_trace_set_hook(x_trace_hook_t hook, void *user_data)
{
    x_trace_current_hook      = hook;
    x_trace_current_user_data = user_data;
}

x_trace_hook_t x_trace_get_hook(void)
{
    return x_trace_current_hook;
}

void x_trace_emit(int phase, const char *function)
{
    x_trace_hook_t hook = x_trace_current_hook;
    uint64_t timestamp = 0U;

    if (hook == NULL || function == NULL) {
        return;
    }

    /*
     * Best-effort monotonic timestamp.  x_time_monotonic_ns lives in
     * x_time.c; we call it here to avoid duplicating clock logic.  If it
     * fails we still fire the trace with timestamp 0.
     */
    {
        /* Forward-declare to avoid circular header include. */
        extern int x_time_monotonic_ns(uint64_t *nanoseconds);
        (void)x_time_monotonic_ns(&timestamp);
    }

    hook(phase, function, timestamp, x_trace_current_user_data);
}

/* ---------- debug assertions ---------------------------------------------- */

#ifndef NDEBUG
void x_assert_fail(const char *expression, const char *message,
                   const char *file, int line)
{
    fprintf(stderr, "xlib assertion failed: %s\n", message != NULL ? message : "(no message)");
    fprintf(stderr, "  expression: %s\n", expression != NULL ? expression : "(unknown)");
    fprintf(stderr, "  location:   %s:%d\n", file != NULL ? file : "(unknown)", line);
    fflush(stderr);
    abort();
}
#endif
