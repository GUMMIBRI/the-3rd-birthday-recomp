/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_PLATFORM_CLOCK_H
#define SR_PLATFORM_CLOCK_H
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
static inline void sr_clock_setup(void) { timeBeginPeriod(1); }
static inline int64_t sr_clock_frequency(void) {
    LARGE_INTEGER value;
    if (!QueryPerformanceFrequency(&value)) abort();
    return value.QuadPart;
}
static inline int64_t sr_clock_ticks(void) {
    LARGE_INTEGER value;
    if (!QueryPerformanceCounter(&value)) abort();
    return value.QuadPart;
}
static inline void sr_clock_sleep_ms(uint32_t ms) { Sleep(ms); }
#else
#include <time.h>
#include <errno.h>
static inline void sr_clock_setup(void) { }
static inline int64_t sr_clock_frequency(void) { return INT64_C(1000000000); }
static inline int64_t sr_clock_ticks(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) abort();
    return (int64_t)value.tv_sec * INT64_C(1000000000) + value.tv_nsec;
}
static inline void sr_clock_sleep_ms(uint32_t ms) {
    struct timespec delay = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    while (nanosleep(&delay, &delay) != 0) { if (errno != EINTR) abort(); }
}
#endif
#endif
