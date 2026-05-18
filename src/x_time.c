#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/time.h>

#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifdef _WIN32
static int x_time_filetime_ns(uint64_t *nanoseconds)
{
    FILETIME filetime;
    ULARGE_INTEGER value;
    uint64_t intervals_since_unix_epoch;

    GetSystemTimeAsFileTime(&filetime);

    value.LowPart = filetime.dwLowDateTime;
    value.HighPart = filetime.dwHighDateTime;

    if (value.QuadPart < 116444736000000000ULL) {
        return EINVAL;
    }

    intervals_since_unix_epoch = value.QuadPart - 116444736000000000ULL;
    *nanoseconds = intervals_since_unix_epoch * 100ULL;
    return 0;
}
#else
static int x_time_clock_ns(clockid_t clock_id, uint64_t *nanoseconds)
{
    struct timespec time_value;

    if (clock_gettime(clock_id, &time_value) != 0) {
        return errno;
    }

    *nanoseconds = ((uint64_t)time_value.tv_sec * 1000000000ULL)
        + (uint64_t)time_value.tv_nsec;
    return 0;
}
#endif

int x_time_monotonic_ns(uint64_t *nanoseconds)
{
    if (nanoseconds == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        LARGE_INTEGER counter;
        LARGE_INTEGER frequency;
        uint64_t cnt;
        uint64_t freq;

        if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
            return EINVAL;
        }

        cnt  = (uint64_t)counter.QuadPart;
        freq = (uint64_t)frequency.QuadPart;

        /* Integer arithmetic avoids the precision loss of long double on MSVC
         * (where long double == double, 64-bit).  Split into whole seconds and
         * the sub-second remainder to keep intermediate values in range. */
        *nanoseconds = (cnt / freq) * 1000000000ULL
                     + (cnt % freq) * 1000000000ULL / freq;
        return 0;
    }
#else
    return x_time_clock_ns(CLOCK_MONOTONIC, nanoseconds);
#endif
}

int x_time_system_ns(uint64_t *nanoseconds)
{
    if (nanoseconds == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    return x_time_filetime_ns(nanoseconds);
#else
    return x_time_clock_ns(CLOCK_REALTIME, nanoseconds);
#endif
}

int x_time_sleep_ms(uint64_t milliseconds)
{
    if (milliseconds > UINT64_MAX / 1000ULL) {
        return EOVERFLOW;
    }

    return x_time_sleep_us(milliseconds * 1000ULL);
}

int x_time_sleep_us(uint64_t microseconds)
{
    if (microseconds == 0U) {
        return 0;
    }

#ifdef _WIN32
    {
        HANDLE timer;
        LARGE_INTEGER due_time;

        if (microseconds > (uint64_t)LLONG_MAX / 10ULL) {
            return EOVERFLOW;
        }

        timer = CreateWaitableTimerA(NULL, TRUE, NULL);
        if (timer == NULL) {
            return EAGAIN;
        }

        due_time.QuadPart = -((LONGLONG)microseconds * 10LL);
        if (!SetWaitableTimer(timer, &due_time, 0, NULL, NULL, FALSE)) {
            CloseHandle(timer);
            return EINVAL;
        }

        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
            CloseHandle(timer);
            return EINVAL;
        }

        CloseHandle(timer);
        return 0;
    }
#else
    {
        struct timespec request;
        struct timespec remaining;

        request.tv_sec = (time_t)(microseconds / 1000000ULL);
        request.tv_nsec = (long)(microseconds % 1000000ULL) * 1000L;

        while (nanosleep(&request, &remaining) != 0) {
            if (errno != EINTR) {
                return errno;
            }

            request = remaining;
        }

        return 0;
    }
#endif
}

int x_timer_start(x_timer_t *timer)
{
    if (timer == NULL) {
        return EINVAL;
    }

    return x_time_monotonic_ns(&timer->started_ns);
}

int x_timer_elapsed_ns(const x_timer_t *timer, uint64_t *nanoseconds)
{
    uint64_t now = 0U;
    int error;

    if (timer == NULL || nanoseconds == NULL) {
        return EINVAL;
    }

    error = x_time_monotonic_ns(&now);
    if (error != 0) {
        return error;
    }

    *nanoseconds = now >= timer->started_ns ? now - timer->started_ns : 0U;
    return 0;
}
