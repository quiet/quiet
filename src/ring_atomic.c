#include "quiet/ring_atomic.h"

ring *ring_create(size_t length) {
    ring *r = malloc(sizeof(ring));

    r->length = length;
    r->base = malloc(length);
    atomic_init(&r->reader, (uintptr_t)r->base);
    atomic_init(&r->writer, (uintptr_t)r->base);
    pthread_mutex_init(&r->reader_mutex, NULL);
    pthread_mutex_init(&r->writer_mutex, NULL);
    atomic_init(&r->is_closed, false);

    r->partial_write_in_progress = false;
    r->partial_write_length = 0;

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

ssize_t ring_write(ring *r, const void *vbuf, size_t len) {
    bool is_closed = (bool)atomic_load(&r->is_closed);
    if (is_closed) {
        return 0;
    }

    if (r->partial_write_in_progress) {
        return RingErrorPartialWriteInProgress;
    }

    const uint8_t *buf = (const uint8_t *)vbuf;

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
        return RingErrorWouldBlock;
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

ssize_t ring_read(ring *r, void *vdst, size_t len) {
    uint8_t *r_copy = (uint8_t*)atomic_load(&r->reader);
    uint8_t *w_copy = (uint8_t*)atomic_load(&r->writer);
    // n.b. regarding comments in ring_write
    // if r->reader == r->writer, then the reader is caught up, so we are done
    size_t avail = ring_calculate_distance(r, r_copy, w_copy);
    if (avail < len) {
        bool is_closed = (bool)atomic_load(&r->is_closed);
        if (is_closed) {
            return 0;
        }
        return RingErrorWouldBlock;
    }

    uint8_t *dst = (uint8_t *)vdst;

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

void ring_close(ring *r) {
    atomic_store(&r->is_closed, true);
}

bool ring_is_closed(ring *r) {
    return (bool)atomic_load(&r->is_closed);
}

void ring_advance_reader(ring *r, size_t len) {
    uint8_t *r_copy = (uint8_t*)atomic_load(&r->reader);
    r_copy = ring_calculate_advance(r, r_copy, len);
    atomic_store(&r->reader, (uintptr_t)r_copy);
}

ssize_t ring_write_partial_init(ring *r, size_t len) {
    bool is_closed = (bool)atomic_load(&r->is_closed);
    if (is_closed) {
        return 0;
    }

    if (r->partial_write_in_progress) {
        return RingErrorPartialWriteInProgress;
    }

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
        return RingErrorWouldBlock;
    }

    r->partial_write_length = len;
    r->partial_writer = w_copy;
    r->partial_write_in_progress = true;

    return len;
}

ssize_t ring_write_partial(ring *r, const void *vbuf, size_t len) {
    if (r->is_closed) {
        return 0;
    }

    if (len > r->partial_write_length) {
        return RingErrorPartialWriteLengthMismatch;
    }

    const uint8_t *buf = (const uint8_t *)vbuf;

    // how far do we write before the end of the ring?
    size_t prewrap = ring_calculate_distance(r, r->partial_writer, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;
    memcpy(r->partial_writer, buf, prewrap);

    if (prewrap < len) {
        memcpy(r->base, buf + prewrap, len - prewrap);
    }

    r->partial_writer = ring_calculate_advance(r, r->partial_writer, len);
    r->partial_write_length -= len;
    return len;
}

ssize_t ring_write_partial_commit(ring *r) {
    if (r->is_closed) {
        return 0;
    }

    if (!r->partial_write_in_progress) {
        return RingErrorPartialWriteLengthMismatch;
    }

    if (r->partial_write_length) {
        return RingErrorPartialWriteLengthMismatch;
    }

    atomic_thread_fence(memory_order_release);

    atomic_store(&r->writer, (uintptr_t)r->partial_writer);
    r->partial_write_in_progress = false;

    return 0;
}

void ring_reader_set_blocking(ring *r, time_t sec, long nano) {
    assert(false && "blocking mode not supported by this version. please recompile with pthread support");
}

void ring_reader_set_nonblocking(ring *r) {
    return;
}

void ring_writer_set_blocking(ring *r, time_t sec, long nano) {
    assert(false && "blocking mode not supported by this version. please recompile with pthread support");
}

void ring_writer_set_nonblocking(ring *r) {
    return;
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
