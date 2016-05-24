#include "quiet/ring_atomic.h"

ring *ring_create(size_t length) {
    ring *r = malloc(sizeof(ring));

    r->length = length;
    r->base = malloc(length);
    atomic_init(&r->reader, (uintptr_t)r->base);
    atomic_init(&r->writer, (uintptr_t)r->base);
    pthread_mutex_init(&r->reader_mutex, NULL);
    pthread_mutex_init(&r->writer_mutex, NULL);

    return r;
}

// pointers equal -- can write all, read none
// write == read - 1 -- can read all, write none

void ring_destroy(ring *r) {
    pthread_mutex_destroy(&r->reader_mutex);
    pthread_mutex_destroy(&r->writer_mutex);
    free(r->base);
    free(r);
}

size_t ring_calculate_distance(const ring *r, const uint8_t *src, const uint8_t *dst) {
    dst = (dst < src) ? (dst + r->length) : dst;
    return dst - src;
}

uint8_t *ring_calculate_advance(const ring *r, uint8_t *p, size_t adv) {
    uint8_t *end = p + adv;
    return (end < (r->base + r->length)) ? end : (end - r->length);
}

ssize_t ring_write(ring *r, const uint8_t *buf, size_t len) {
    uint8_t *r_copy = (uint8_t*)atomic_load(&r->reader);
    uint8_t *w_copy = (uint8_t*)atomic_load(&r->writer);
    // first find if we have the available space
    size_t distance = ring_calculate_distance(r, w_copy, r_copy);

    // the writer pointer points to the next block to be written,
    //      and reader to next block to be read
    // if they have no distance between them, it tells us the reader
    //      has read everything written
    // otherwise, we have to make sure not to overwrite the next block
    //      that the reader will read, so we subtract 1
    // in other words, the reader may catch up to the writer,
    //      but the writer must not catch up to the reader
    size_t avail = distance ? (distance - 1) : (r->length - 1);

    if (avail < len) {
        return -1;
    }

    // atomic note: r may change (ring_read could also occur right now)
    // however, that's ok, because it can only advance reader ptr forward,
    // not backward, so we're always allowed to use at least as much space
    // as we calculated above

    // how far do we write before the end of the ring?
    size_t prewrap = ring_calculate_distance(r, w_copy, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;

    // this copy will be serialized by the atomic_store on this side and
    //   atomic_load on the other side
    memcpy(w_copy, buf, prewrap);

    if (prewrap < len) {
        memcpy(r->base, buf + prewrap, len - prewrap);
    }

    atomic_thread_fence(memory_order_release);

    w_copy = ring_calculate_advance(r, w_copy, len);
    atomic_store(&r->writer, (uintptr_t)w_copy);
    return len;
}

ssize_t ring_read(ring *r, uint8_t *dst, size_t len) {
    uint8_t *r_copy = (uint8_t*)atomic_load(&r->reader);
    uint8_t *w_copy = (uint8_t*)atomic_load(&r->writer);
    // n.b. regarding comments in ring_write
    // if r->reader == r->writer, then the reader is caught up, so we are done
    size_t avail = ring_calculate_distance(r, r_copy, w_copy);
    if (avail < len) {
        return -1;
    }

    atomic_thread_fence(memory_order_acquire);

    size_t prewrap = ring_calculate_distance(r, r_copy, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;
    memcpy(dst, r_copy, prewrap);

    if (prewrap < len) {
        memcpy(dst + prewrap, r->base, len - prewrap);
    }

    r_copy = ring_calculate_advance(r, r_copy, len);
    atomic_store(&r->reader, (uintptr_t)r_copy);
    return len;
}

void ring_advance_reader(ring *r, size_t len) {
    uint8_t *r_copy = (uint8_t*)atomic_load(&r->reader);
    r_copy = ring_calculate_advance(r, r_copy, len);
    atomic_store(&r->reader, (uintptr_t)r_copy);
}

void ring_writer_lock(ring *r) {
    pthread_mutex_lock(&r->writer_mutex);
}

void ring_writer_unlock(ring *r) {
    pthread_mutex_unlock(&r->writer_mutex);
}

void ring_reader_lock(ring *r) {
    pthread_mutex_lock(&r->reader_mutex);
}

void ring_reader_unlock(ring *r) {
    pthread_mutex_unlock(&r->reader_mutex);
}
