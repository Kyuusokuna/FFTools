#pragma once
#include "Windows.h"

struct Semaphore_OS_Data {
    HANDLE handle = 0;
};

struct Mutex_OS_Data {
    CRITICAL_SECTION section;
};