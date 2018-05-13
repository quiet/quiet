#include "quiet/common.h"

typedef struct {
    size_t length;
    uint8_t *base;
    uint8_t *reader; // reader points to next block to be read
    uint8_t *writer; // writer points to next block to be written
    bool is_closed;
    size_t partial_write_length;
    uint8_t *partial_writer; // partial_writer points to the next block to be partial written
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

// these are nops so that this can easily be used in place of the threadsafe rings
void ring_writer_lock(ring *r);
void ring_writer_unlock(ring *r);
void ring_reader_lock(ring *r);
void ring_reader_unlock(ring *r);
