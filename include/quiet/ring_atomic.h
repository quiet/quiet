#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <stdatomic.h>
#include <pthread.h>

// stdatomic approach to ring buffer
typedef struct {
    size_t length;
    uint8_t *base;
    _Atomic uintptr_t reader; // reader points to next block to be read
    _Atomic uintptr_t writer; // writer points to next block to be written
    pthread_mutex_t reader_mutex;
    pthread_mutex_t writer_mutex;
} ring;

ring *ring_create(size_t length);
void ring_destroy(ring *r);
size_t ring_calculate_distance(const ring *r, const uint8_t *src, const uint8_t *dst);
uint8_t *ring_calculate_advance(const ring *r, uint8_t *p, size_t adv);
ssize_t ring_write(ring *r, const uint8_t *buf, size_t len);
ssize_t ring_read(ring *r, uint8_t *dst, size_t len);
void ring_advance_reader(ring *r, size_t len);

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
