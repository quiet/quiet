#include "quiet/ring.h"

ring *ring_create(size_t length) {
    ring *r = malloc(sizeof(ring));

    r->length = length;
    r->base = malloc(length);
    r->reader = r->base;
    r->writer = r->base;

    r->is_closed = false;

    r->partial_write_length = 0;
    r->partial_write_in_progress = false;

    return r;
}

// pointers equal -- can write all, read none
// write == read - 1 -- can read all, write none

void ring_destroy(ring *r) {
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
    if (r->is_closed) {
        return 0;
    }

    if (r->partial_write_in_progress) {
        return RingErrorPartialWriteInProgress;
    }

    const uint8_t *buf = (const uint8_t *)vbuf;

    // first find if we have the available space
    size_t distance = ring_calculate_distance(r, r->writer, r->reader);

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

    // how far do we write before the end of the ring?
    size_t prewrap = ring_calculate_distance(r, r->writer, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;
    memcpy(r->writer, buf, prewrap);

    if (prewrap < len) {
        memcpy(r->base, buf + prewrap, len - prewrap);
    }

    r->writer = ring_calculate_advance(r, r->writer, len);
    return len;
}

ssize_t ring_read(ring *r, void *vdst, size_t len) {
    // n.b. regarding comments in ring_write
    // if r->reader == r->writer, then the reader is caught up, so we are done
    size_t avail = ring_calculate_distance(r, r->reader, r->writer);
    if (avail < len) {
        if (r->is_closed) {
            return 0;
        }
        return RingErrorWouldBlock;
    }

    const uint8_t *dst = (const uint8_t *)vdst;

    size_t prewrap = ring_calculate_distance(r, r->reader, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;
    memcpy(dst, r->reader, prewrap);

    if (prewrap < len) {
        memcpy(dst + prewrap, r->base, len - prewrap);
    }

    r->reader = ring_calculate_advance(r, r->reader, len);
    return len;
}

void ring_close(ring *r) {
    r->is_closed = true;
}

bool ring_close(ring *r) {
    return r->is_closed;
}

void ring_advance_reader(ring *r, size_t len) {
    r->reader = ring_calculate_advance(r, r->reader, len);
}

ssize_t ring_write_partial_init(ring *r, size_t len) {
    if (r->is_closed) {
        return 0;
    }

    if (r->partial_write_in_progress) {
        return RingErrorPartialWriteInProgress;
    }

    // first find if we have the available space
    size_t distance = ring_calculate_distance(r, r->writer, r->reader);

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
    r->partial_writer = r->writer;
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

    r->writer = r->partial_writer;
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

void ring_reader_lock(ring *r) {
    return;
}

void ring_reader_unlock(ring *r) {
    return;
}

void ring_writer_lock(ring *r) {
    return;
}

void ring_writer_unlock(ring *r) {
    return;
}
