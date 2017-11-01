#include "clock.h"

#ifdef _WIN32

#include <Windows.h>

uint64_t millis(void)
{
    LARGE_INTEGER freq, time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&time);

    return time.QuadPart * 1000 / freq.QuadPart;
}

#else // ifdef _WIN32

#include <time.h>

uint64_t millis(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    return (time.tv_sec * 1000) + (time.tv_nsec / 1000000);
}

#endif
