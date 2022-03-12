#include "Threading.h"

Semaphore::~Semaphore() {
    if (os_data.handle)
        CloseHandle(os_data.handle);
}

void Semaphore::init() {
    os_data.handle = CreateSemaphore(0, 0, LONG_MAX, 0);
}

void Semaphore::signal() {
    ReleaseSemaphore(os_data.handle, 1, 0);
}

void Semaphore::wait_for() {
    WaitForSingleObject(os_data.handle, INFINITE);
}



Mutex::~Mutex() {
    DeleteCriticalSection(&os_data.section);
}

void Mutex::init() {
    InitializeCriticalSection(&os_data.section);
}

void Mutex::take() {
    EnterCriticalSection(&os_data.section);
}

void Mutex::release() {
    LeaveCriticalSection(&os_data.section);
}



int32_t atomic_read(volatile int32_t *r) {
    return InterlockedAdd((volatile long *)r, 0);
}

int64_t atomic_read(volatile int64_t *r) {
    return InterlockedAdd64((volatile long long *)r, 0);
}

uint32_t atomic_read(volatile uint32_t *r) {
    return InterlockedAdd((volatile long *)r, 0);
}

uint64_t atomic_read(volatile uint64_t *r) {
    return InterlockedAdd64((volatile long long *)r, 0);
}


int32_t atomic_increment(volatile int32_t *r) {
    return InterlockedIncrement((volatile long *)r);
}

int64_t atomic_increment(volatile int64_t *r) {
    return InterlockedIncrement64((volatile long long *)r);
}

uint32_t atomic_increment(volatile uint32_t *r) {
    return InterlockedIncrement((volatile long *)r);
}

uint64_t atomic_increment(volatile uint64_t *r) {
    return InterlockedIncrement64((volatile long long *)r);
}


int32_t atomic_add(volatile int32_t *r, int32_t v) {
    return InterlockedAdd((volatile long *)r, v);
}

int64_t atomic_add(volatile int64_t *r, int64_t v) {
    return InterlockedAdd64((volatile long long *)r, v);
}

uint32_t atomic_add(volatile uint32_t *r, uint32_t v) {
    return InterlockedAdd((volatile long *)r, v);
}

uint64_t atomic_add(volatile uint64_t *r, uint64_t v) {
    return InterlockedAdd64((volatile long long *)r, v);
}