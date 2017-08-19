#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _MSC_VER
#include <time.h>
#include <errno.h>
#include <windows.h>
typedef CRITICAL_SECTION pthread_mutex_t;

static inline void pthread_mutex_init(pthread_mutex_t *mutex, void *attr) { InitializeCriticalSection(mutex); }
static inline void pthread_mutex_destroy(pthread_mutex_t *mutex) { DeleteCriticalSection(mutex); }
static inline void pthread_mutex_lock(pthread_mutex_t *mutex) { EnterCriticalSection(mutex); }
static inline void pthread_mutex_unlock(pthread_mutex_t *mutex) { LeaveCriticalSection(mutex); }

int gettimeofday(struct timeval *tp, void *_arg) {
    tp->tv_sec = 0;
    tp->tv_usec = 0;
    return 0;
}

typedef CONDITION_VARIABLE pthread_cond_t;

static inline void pthread_cond_init(pthread_cond_t *cv, void *attr) { InitializeConditionVariable(cv); }
static inline void pthread_cond_destroy(pthread_cond_t *cv) { }
static inline int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *mutex) {
    int res = SleepConditionVariableCS(cv, mutex, INFINITE);
    if (res) {
        return 0;
    }
    if (GetLastError() == ERROR_TIMEOUT) {
        return ETIMEDOUT;
    }
    return EINVAL;
}
static inline int pthread_cond_timedwait(pthread_cond_t *cv, pthread_mutex_t *mutex, struct timespec *deadline) {
    int res = SleepConditionVariableCS(cv, mutex, deadline->tv_sec * 1000 + (deadline->tv_nsec / 1000000));
    if (res) {
        return 0;
    }
    if (GetLastError() == ERROR_TIMEOUT) {
        return ETIMEDOUT;
    }
    return EINVAL;
}
static inline void pthread_cond_signal(pthread_cond_t *cv) { WakeConditionVariable(cv); }
static inline void pthread_cond_broadcast(pthread_cond_t *cv) { WakeAllConditionVariable(cv); }
#else
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#endif

#include "quiet/common.h"

typedef struct {
    bool is_blocking;
    struct timespec timeout;
    pthread_cond_t cond;
} ring_wait_t;

// pthread condvar approach to ring buffer
// support blocking and nonblocking modes
typedef struct {
    size_t length;
    uint8_t *base;
    uint8_t *reader;
    uint8_t *writer;
    pthread_mutex_t mutex;

    size_t partial_write_length;
    uint8_t *partial_writer;
    bool partial_write_in_progress;

    ring_wait_t *read_wait;
    ring_wait_t *write_wait;

    bool is_closed;
} ring;

ring *ring_create(size_t length);
void ring_destroy(ring *r);
size_t ring_calculate_distance(const ring *r, const uint8_t *src, const uint8_t *dst);
uint8_t *ring_calculate_advance(const ring *r, uint8_t *p, size_t adv);
// must be called with lock held
ssize_t ring_write(ring *r, const void *buf, size_t len);
// must be called with lock held
ssize_t ring_read(ring *r, void *dst, size_t len);
// must be called with lock held
void ring_close(ring *r);
// must be called with lock held
void ring_advance_reader(ring *r, size_t len);

ssize_t ring_write_partial_init(ring *r, size_t len);
ssize_t ring_write_partial(ring *r, const void *buf, size_t len);
ssize_t ring_write_partial_commit(ring *r);

// must be called with lock held
void ring_set_reader_blocking(ring *r, time_t sec, long nano);
// must be called with lock held
void ring_set_reader_nonblocking(ring *r);
// must be called with lock held
void ring_set_writer_blocking(ring *r, time_t sec, long nano);
// must be called with lock held
void ring_set_writer_nonblocking(ring *r);

// there's only one mutex here, but this api allows us to mimic atomic ring
void ring_reader_lock(ring *r);
void ring_reader_unlock(ring *r);
void ring_writer_lock(ring *r);
void ring_writer_unlock(ring *r);
