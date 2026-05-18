/*
 * diagnostics_example.c — demonstrates the xlib v1.8 diagnostics hooks.
 *
 * This example installs a log hook, a diagnostics hook, a tracing hook,
 * and exercises the X_ASSERT macro, then triggers each one by making
 * deliberate API calls that succeed and fail.
 */

#include <xlib/diagnostics.h>
#include <xlib/thread.h>
#include <xlib/network.h>

#include <stdio.h>
#include <string.h>

/* ---- log hook callback --------------------------------------------------- */

static const char *level_name(int level)
{
    switch (level) {
    case X_LOG_LEVEL_DEBUG: return "DEBUG";
    case X_LOG_LEVEL_INFO:  return "INFO";
    case X_LOG_LEVEL_WARN:  return "WARN";
    case X_LOG_LEVEL_ERROR: return "ERROR";
    default:                return "???";
    }
}

static void my_log_hook(int level, const char *module, const char *message,
                        void *user_data)
{
    (void)user_data;
    printf("[LOG %s/%s] %s\n", level_name(level), module, message);
}

/* ---- diagnostics hook callback ------------------------------------------- */

static void my_diag_hook(const x_diag_event_t *event, void *user_data)
{
    (void)user_data;
    printf("[DIAG] operation=%s  error=%d", event->operation, event->error_code);
    if (event->context != NULL) {
        printf("  context=%s", event->context);
    }
    printf("\n");
}

/* ---- tracing hook callback ----------------------------------------------- */

static void my_trace_hook(int phase, const char *function,
                          uint64_t timestamp, void *user_data)
{
    (void)user_data;
    printf("[TRACE] %s %s  ts=%llu\n",
           phase == X_TRACE_ENTER ? ">>>" : "<<<",
           function,
           (unsigned long long)timestamp);
}

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    x_mutex_t *mutex = NULL;
    int error;

    printf("=== xlib diagnostics example ===\n\n");

    /* Install hooks */
    x_log_set_hook(my_log_hook, NULL);
    x_diag_set_hook(my_diag_hook, NULL);
    x_trace_set_hook(my_trace_hook, NULL);

    printf("--- hooks installed ---\n\n");

    /* Emit a manual log message */
    x_log_emit(X_LOG_LEVEL_INFO, "example", "hooks are ready");

    /* Successful call — no diag event expected */
    error = x_mutex_create(&mutex);
    if (error != 0) {
        printf("x_mutex_create failed: %d\n", error);
        return 1;
    }

    /* Trigger a trace manually */
    x_trace_emit(X_TRACE_ENTER, "example_operation");
    x_trace_emit(X_TRACE_EXIT,  "example_operation");

    /* Deliberately trigger a diagnostics event by passing NULL */
    error = x_mutex_lock(NULL);
    if (error != 0) {
        x_diag_emit("x_mutex_lock", error, "deliberate NULL for demo");
        x_log_emit(X_LOG_LEVEL_WARN, "example",
                   "x_mutex_lock(NULL) returned %d as expected", error);
    }

    /* Clean up */
    x_mutex_destroy(mutex);

    /* Debug assertion (only fires in debug builds) */
    X_ASSERT(1 == 1, "basic sanity check");

    printf("\n--- log hook query: %s ---\n",
           x_log_get_hook() != NULL ? "installed" : "none");

    /* Remove hooks */
    x_log_set_hook(NULL, NULL);
    x_diag_set_hook(NULL, NULL);
    x_trace_set_hook(NULL, NULL);

    printf("\n=== done ===\n");
    return 0;
}
