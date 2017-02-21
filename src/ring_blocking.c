#include "quiet/ring_blocking.h"

static ring_wait_t *ring_wait_create() {
    ring_wait_t *w = malloc(sizeof(ring_wait_t));
    w->is_blocking = false;
    pthread_cond_init(&w->cond, NULL);
    return w;
}

static void ring_wait_destroy(ring_wait_t *w) {
    pthread_cond_destroy(&w->cond);
}

static void ring_wait_set_blocking(ring_wait_t *w, time_t sec, long nano) {
    w->is_blocking = true;
    w->timeout.tv_sec = sec;
    w->timeout.tv_nsec = nano;
}

static void ring_wait_set_nonblocking(ring_wait_t *w) {
    w->is_blocking = false;
}

static struct timespec ring_wait_calculate_deadline(ring_wait_t *w) {
    struct timespec deadline;
    deadline.tv_sec = 0;
    deadline.tv_nsec = 0;
    if (w->timeout.tv_sec == 0 && w->timeout.tv_nsec == 0) {
        return deadline;
    }
    struct timeval now;
    gettimeofday(&now, NULL);

    struct timespec timeout = w->timeout;
    deadline.tv_sec = now.tv_sec + timeout.tv_sec;
    deadline.tv_nsec = (now.tv_usec * 1000) + timeout.tv_nsec;
    if (deadline.tv_nsec > 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    return deadline;
}

static int ring_wait_wait(ring_wait_t *w, pthread_mutex_t *mu, struct timespec deadline) {
    if (deadline.tv_sec == 0 && deadline.tv_nsec == 0) {
        int res = EINTR;
        while (res == EINTR) {
            res = pthread_cond_wait(&w->cond, mu);
        }
        return res;
    }

    int res = EINTR;
    while (res == EINTR) {
        res = pthread_cond_timedwait(&w->cond, mu, &deadline);
    }
    return res;
}

static void ring_wait_broadcast(ring_wait_t *w) {
    pthread_cond_broadcast(&w->cond);
}

static void ring_wait_signal(ring_wait_t *w) {
    pthread_cond_signal(&w->cond);
}

ring *ring_create(size_t length) {
    ring *r = malloc(sizeof(ring));

    r->length = length;
    r->base = malloc(length);
    r->reader = r->base;
    r->writer = r->base;
    pthread_mutex_init(&r->mutex, NULL);
    r->read_wait = ring_wait_create();
    r->write_wait = ring_wait_create();
    r->is_closed = false;
    r->partial_write_in_progress = false;
    r->partial_write_length = 0;

    return r;
}

// pointers equal -- can write all, read none
// write == read - 1 -- can read all, write none

void ring_destroy(ring *r) {
    ring_wait_destroy(r->read_wait);
    ring_wait_destroy(r->write_wait);
    pthread_mutex_destroy(&r->mutex);
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

// must be called with lock held
void ring_set_reader_blocking(ring *r, time_t sec, long nano) {
    ring_wait_set_blocking(r->read_wait, sec, nano);
}

// must be called with lock held
void ring_set_reader_nonblocking(ring *r) {
    ring_wait_set_nonblocking(r->read_wait);
}

// must be called with lock held
void ring_set_writer_blocking(ring *r, time_t sec, long nano) {
    ring_wait_set_blocking(r->write_wait, sec, nano);
}

// must be called with lock held
void ring_set_writer_nonblocking(ring *r) {
    ring_wait_set_nonblocking(r->write_wait);
}

// must be called with lock held
ssize_t ring_write(ring *r, const void *vbuf, size_t len) {
    bool is_blocking = r->write_wait->is_blocking;
    struct timespec deadline;
    if (is_blocking) {
        deadline = ring_wait_calculate_deadline(r->write_wait);
    }
    if (r->partial_write_in_progress) {
        return RingErrorPartialWriteInProgress;
    }

    const uint8_t *buf = (const uint8_t *)vbuf;

    uint8_t *reader, *writer;
    while (true) {
        // if the ring is closed, then writing will always fail
        if (r->is_closed) {
            return 0;
        }
        reader = r->reader;
        writer = r->writer;
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

        if (avail >= len) {
            break;
        }

        if (!is_blocking) {
            return RingErrorWouldBlock;
        }

        int res = ring_wait_wait(r->write_wait, &r->mutex, deadline);
        if (res) {
            if (errno == ETIMEDOUT) {
                return RingErrorTimedout;
            }
            return RingErrorIO;
        }
    }

    // how far do we write before the end of the ring?
    size_t prewrap = ring_calculate_distance(r, writer, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;

    memcpy(writer, buf, prewrap);

    if (prewrap < len) {
        memcpy(r->base, buf + prewrap, len - prewrap);
    }

    r->writer = ring_calculate_advance(r, writer, len);
    ring_wait_signal(r->read_wait);
    return len;
}

// must be called with lock held
ssize_t ring_write_partial_init(ring *r, size_t len) {
    bool is_blocking = r->write_wait->is_blocking;
    struct timespec deadline;
    if (is_blocking) {
        deadline = ring_wait_calculate_deadline(r->write_wait);
    }
    if (r->partial_write_in_progress) {
        return RingErrorPartialWriteInProgress;
    }
    uint8_t *reader, *writer;
    while (true) {
        // if the ring is closed, then writing will always fail
        if (r->is_closed) {
            return 0;
        }
        reader = r->reader;
        writer = r->writer;
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

        if (avail >= len) {
            break;
        }

        if (!is_blocking) {
            return RingErrorWouldBlock;
        }

        int res = ring_wait_wait(r->write_wait, &r->mutex, deadline);
        if (res) {
            if (errno == ETIMEDOUT) {
                return RingErrorTimedout;
            }
            return RingErrorIO;
        }
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
    ring_wait_signal(r->read_wait);

    return 0;
}

// must be called with lock held
ssize_t ring_read(ring *r, void *vdst, size_t len) {
    bool is_blocking = r->read_wait->is_blocking;
    struct timespec deadline;
    if (is_blocking) {
        deadline = ring_wait_calculate_deadline(r->read_wait);
    }

    uint8_t *dst = (uint8_t *)vdst;
    uint8_t *reader, *writer;
    while (true) {
        reader = r->reader;
        writer = r->writer;

        // n.b. regarding comments in ring_write
        // if r->reader == r->writer, then the reader is caught up, so we are done
        size_t avail = ring_calculate_distance(r, reader, writer);

        if (avail >= len) {
            break;
        }

        // if the ring is closed, then allow reads to continue until ring is empty
        // once it's empty, then notify of its closed state
        if (r->is_closed) {
            return 0;
        }

        if (!is_blocking) {
            return RingErrorWouldBlock;
        }

        int res = ring_wait_wait(r->read_wait, &r->mutex, deadline);
        if (res) {
            if (res == ETIMEDOUT) {
                return RingErrorTimedout;
            }
            return RingErrorIO;
        }
    }

    size_t prewrap = ring_calculate_distance(r, reader, r->base + r->length);
    prewrap = (prewrap > len) ? len : prewrap;
    memcpy(dst, reader, prewrap);

    if (prewrap < len) {
        memcpy(dst + prewrap, r->base, len - prewrap);
    }

    r->reader = ring_calculate_advance(r, reader, len);
    ring_wait_signal(r->write_wait);
    return len;
}

// must be called with lock held
void ring_close(ring *r) {
    r->is_closed = true;
    ring_wait_broadcast(r->write_wait);
    ring_wait_broadcast(r->read_wait);
}

// must be called with lock held
void ring_advance_reader(ring *r, size_t len) {
    r->reader = ring_calculate_advance(r, r->reader, len);
}

void ring_reader_lock(ring *r) {
    pthread_mutex_lock(&r->mutex);
}

void ring_reader_unlock(ring *r) {
    pthread_mutex_unlock(&r->mutex);
}

void ring_writer_lock(ring *r) {
    pthread_mutex_lock(&r->mutex);
}

void ring_writer_unlock(ring *r) {
    pthread_mutex_unlock(&r->mutex);
}
