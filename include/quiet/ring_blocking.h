#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <errno.h>

#include <pthread.h>

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
bool ring_is_closed(ring *r);
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
