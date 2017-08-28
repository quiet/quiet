#include "quiet/ring_blocking.h"

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#ifdef _MSC_VER
#include <windows.h>
static inline void usleep(int micros)
{
    if (micros > 0 && micros < 1000) {
        return Sleep(1);
    }
    return Sleep(micros/1000);
}

typedef DWORD thread_id;

static void thread_create(thread_id *id, uint32_t (*fn)(void*), void *arg) {
    CreateThread(NULL, NULL, fn, arg, 0, id);
}

static uint32_t thread_join(thread_id id) {
    return WaitForSingleObject(id, INFINITE);
}
#else
#include <unistd.h>
#include <pthread.h>

typedef pthread_t thread_id;

typedef struct {
    uint32_t (*fn)(void*);
    void *arg;
} thread_call;

static void *thread_wrapper(void *callv) {
    thread_call *call = (thread_call*)callv;
    uint32_t *res = (uint32_t*)malloc(sizeof(uint32_t));
    *res = call->fn(call->arg);
    free(call);
    pthread_exit(res);
    return NULL;
}

static void thread_create(thread_id *id, uint32_t (*fn)(void*), void *arg) {
    thread_call *call = (thread_call*)malloc(sizeof(thread_call));
    call->fn = fn;
    call->arg = arg;
    pthread_create(id, NULL, thread_wrapper, call);
}

static uint32_t thread_join(thread_id id) {
    void *res_p;
    pthread_join(id, &res_p);
    uint32_t *res = (uint32_t*)res_p;
    uint32_t res_temp = *res;
    free(res);
    return res_temp;
}
#endif

typedef struct {
    ring *buf;
    size_t write_len;
    bool multi;
} arg_t;

const uint8_t seq_len = 231; // make this non power of 2 to force buffer variations

uint32_t write_sequence(void *arg_void) {
    arg_t *arg = (arg_t*)arg_void;
    ring *buf = arg->buf;
    size_t write_len = arg->write_len;
    uint8_t seq = 0;
    size_t temp_len = 16;
    uint8_t *temp = (uint8_t*)malloc(temp_len * sizeof(uint8_t));
    uint32_t res = 0;
    for (size_t i = 0; i < write_len; ) {
        size_t nitems = rand() % 16 + 1;
        if (i + nitems > write_len) {
            nitems = write_len - i;
        }
        for (size_t j = 0; j < nitems; j++) {
            temp[j] = seq;
            res += seq;
            seq++;
            seq %= seq_len;
        }
        while (true) {
            if (arg->multi) {
                ring_writer_lock(buf);
            }
            ssize_t nwritten = ring_write(buf, temp, nitems);
            if (arg->multi) {
                ring_writer_unlock(buf);
            }
            if (nwritten != -1) {
                break;
            }
            usleep(10);
        }
        i += nitems;
    }
    free(temp);
    return res;
}

uint32_t read_sequence(void *arg_void) {
    arg_t *arg = (arg_t*)arg_void;
    ring *buf = arg->buf;
    size_t write_len = arg->write_len;
    size_t temp_len = 16;
    uint8_t *temp = (uint8_t*)malloc(temp_len * sizeof(uint8_t));
    uint8_t seq = 0;
    uint32_t res = 0;
    for (size_t i = 0; i < write_len; ) {
        size_t nitems = rand() % 16 + 1;
        if (i + nitems > write_len) {
            nitems = write_len - i;
        }
        while (true) {
            if (arg->multi) {
                ring_reader_lock(buf);
            }
            ssize_t nread = ring_read(buf, temp, nitems);
            if (arg->multi) {
                ring_reader_unlock(buf);
            }
            if (nread != -1) {
                break;
            }
            usleep(10);
        }
        for (size_t j = 0; j < nitems; j++) {
            if (arg->multi) {
                res += temp[j];
            } else {
                if (temp[j] != seq) {
                    printf("mismatch at %zu: %u != %u\n", i + j, temp[j], seq);
                    free(temp);
                    return 1;
                }
            }
            seq++;
            seq %= seq_len;
        }
        i += nitems;
    }
    free(temp);
    return res;
}

int main() {
    srand(time(NULL));
    ring *buf = ring_create(1 << 14);
    thread_id w, w1, r, r1;

    // first we do a test with a single writer, single reader
    // in this test, the reader will ensure the sequence appears
    // strictly in the same order it is written
    arg_t args = {
        buf,
        1 << 22,
        false,
    };
    thread_create(&w, write_sequence, &args);
    thread_create(&r, read_sequence, &args);

    thread_join(w);
    uint32_t res = thread_join(r);

    printf("single reader, single writer test passed: %s\n", res ? "FALSE" : "TRUE");

    // now do 2 writers, 2 readers
    // we relax the sequence restriction and now just look for the same sums
    args.multi = true;

    thread_create(&w, write_sequence, &args);
    thread_create(&w1, write_sequence, &args);
    thread_create(&r, read_sequence, &args);
    thread_create(&r1, read_sequence, &args);

    uint32_t write_sum = 0;
    write_sum += thread_join(w);
    write_sum += thread_join(w1);
    uint32_t read_sum = 0;
    read_sum += thread_join(r);
    read_sum += thread_join(r1);

    printf("2 reader, 2 writer test passed: %s\n", (read_sum != write_sum) ? "FALSE" : "TRUE");
    res = res ? res : read_sum != write_sum;

    ring_destroy(buf);
    return res;
}
