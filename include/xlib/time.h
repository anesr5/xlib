#ifndef XLIB_TIME_H
#define XLIB_TIME_H

#include <stdint.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_timer {
    uint64_t started_ns;
} x_timer_t;

/*
 * Returns nanoseconds from a monotonic clock suitable for elapsed-time
 * measurement.
 */
XLIB_API int x_time_monotonic_ns(uint64_t *nanoseconds);

/*
 * Returns nanoseconds from the Unix epoch according to the system clock.
 */
XLIB_API int x_time_system_ns(uint64_t *nanoseconds);

XLIB_API int x_time_sleep_ms(uint64_t milliseconds);
XLIB_API int x_time_sleep_us(uint64_t microseconds);

/*
 * Convenience helpers for measuring elapsed monotonic time.
 */
XLIB_API int x_timer_start(x_timer_t *timer);
XLIB_API int x_timer_elapsed_ns(const x_timer_t *timer, uint64_t *nanoseconds);

#ifdef __cplusplus
}
#endif

#endif
