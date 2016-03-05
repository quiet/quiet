#include "quiet/ring.h"

ring *ring_create(size_t length) {
    ring *r = malloc(sizeof(ring));

    r->length = length;
    r->base = malloc(length);
    r->reader = r->base;
    r->writer = r->base;

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

ssize_t ring_write(ring *r, const uint8_t *buf, size_t len) {
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
        return -1;
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

ssize_t ring_read(ring *r, uint8_t *dst, size_t len) {
    // n.b. regarding comments in ring_write
    // if r->reader == r->writer, then the reader is caught up, so we are done
    size_t avail = ring_calculate_distance(r, r->reader, r->writer);
    if (avail < len) {
        return -1;
    }

    size_t prewrap = ring_calculate_distance(r, r->reader, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;
    memcpy(dst, r->reader, prewrap);

    if (prewrap < len) {
        memcpy(dst + prewrap, r->base, len - prewrap);
    }

    r->reader = ring_calculate_advance(r, r->reader, len);
    return len;
}
