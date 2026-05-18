#include <xlib/time.h>

#include <stdint.h>
#include <stdio.h>

int main(void)
{
    x_timer_t timer;
    uint64_t monotonic_before;
    uint64_t monotonic_after;
    uint64_t system_time;
    uint64_t elapsed;

    if (x_time_monotonic_ns(&monotonic_before) != 0) {
        return 1;
    }

    if (x_time_system_ns(&system_time) != 0 || system_time == 0U) {
        return 1;
    }

    if (x_timer_start(&timer) != 0) {
        return 1;
    }

    if (x_time_sleep_us(1000U) != 0) {
        return 1;
    }

    if (x_timer_elapsed_ns(&timer, &elapsed) != 0 || elapsed == 0U) {
        return 1;
    }

    if (x_time_sleep_ms(1U) != 0) {
        return 1;
    }

    if (x_time_monotonic_ns(&monotonic_after) != 0) {
        return 1;
    }

    if (monotonic_after < monotonic_before) {
        return 1;
    }

    printf("Time example passed.\n");
    return 0;
}
