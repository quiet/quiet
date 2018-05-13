#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <stdatomic.h>
#include <pthread.h>

#include "quiet/common.h"

// stdatomic approach to ring buffer
typedef struct {
    size_t length;
    uint8_t *base;
    _Atomic uintptr_t reader; // reader points to next block to be read
    _Atomic uintptr_t writer; // writer points to next block to be written
    pthread_mutex_t reader_mutex;
    pthread_mutex_t writer_mutex;
    _Atomic bool is_closed;
    size_t partial_write_length;
    uint8_t *partial_writer;
    bool partial_write_in_progress;
} ring;

ring *ring_create(size_t length);
void ring_destroy(ring *r);
size_t ring_calculate_distance(const ring *r, const uint8_t *src, const uint8_t *dst);
uint8_t *ring_calculate_advance(const ring *r, uint8_t *p, size_t adv);
ssize_t ring_write(ring *r, const void *buf, size_t len);
ssize_t ring_read(ring *r, void *dst, size_t len);
void ring_close(ring *r);
bool ring_is_closed(ring *r);
void ring_advance_reader(ring *r, size_t len);

ssize_t ring_write_partial_init(ring *r, size_t len);
ssize_t ring_write_partial(ring *r, const void *buf, size_t len);
ssize_t ring_write_partial_commit(ring *r);

// stubs for unusable feature
void ring_set_reader_blocking(ring *r, time_t sec, long nano);
void ring_set_reader_nonblocking(ring *r);
void ring_set_writer_blocking(ring *r, time_t sec, long nano);
void ring_set_writer_nonblocking(ring *r);

// lock functions
// ring_atomic supports one reader and one writer simultaneously
//    without any locking
// if there will be more than 1 simultaneous writers, then
//    every writer must use ring_writer_lock/ring_writer_unlock
// if there will be more than 1 simultaneous readers, then
//    every reader must use ring_reader_lock/ring_reader_unlock
void ring_writer_lock(ring *r);
void ring_writer_unlock(ring *r);
void ring_reader_lock(ring *r);
void ring_reader_unlock(ring *r);
