#include "time.h"
#include <Windows.h>


uint64_t Time::get_frequency() {
    LARGE_INTEGER hz;
    QueryPerformanceFrequency(&hz);
    return hz.QuadPart;
}

uint64_t Time::get_time() {
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
}