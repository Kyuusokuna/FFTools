#pragma once
#include <stdint.h>

#ifdef _WIN32
#include "Threading_win32.h"
#else
#error "Unsupported OS"
#endif

struct Semaphore {
    ~Semaphore();

    void init();
    void signal();
    void wait_for();

    Semaphore_OS_Data os_data;
};

struct Mutex {
    ~Mutex();

    void init();
    void take();
    void release();

    Mutex_OS_Data os_data;
};

struct Scoped_Lock {
    Scoped_Lock(Mutex &mutex) {
        m = &mutex;
        m->take();
    }

    ~Scoped_Lock() {
        m->release();
    }

    Mutex *m;
};

int32_t atomic_read(volatile int32_t *r);
int64_t atomic_read(volatile int64_t *r);

uint32_t atomic_read(volatile uint32_t *r);
uint64_t atomic_read(volatile uint64_t *r);


int32_t atomic_increment(volatile int32_t *r);
int64_t atomic_increment(volatile int64_t *r);

uint32_t atomic_increment(volatile uint32_t *r);
uint64_t atomic_increment(volatile uint64_t *r);


int32_t atomic_add(volatile int32_t *r, int32_t v);
int64_t atomic_add(volatile int64_t *r, int64_t v);

uint32_t atomic_add(volatile uint32_t *r, uint32_t v);
uint64_t atomic_add(volatile uint64_t *r, uint64_t v);