#include "quiet/ring_blocking.h"

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    ring *buf;
    size_t write_len;
    bool multi;
} arg_t;

const uint8_t seq_len = 231; // make this non power of 2 to force buffer variations

void *write_sequence(void *arg_void) {
    arg_t *arg = (arg_t*)arg_void;
    ring *buf = arg->buf;
    size_t write_len = arg->write_len;
    uint8_t seq = 0;
    size_t temp_len = 16;
    uint8_t *temp = malloc(temp_len * sizeof(uint8_t));
    int *res = malloc(1 * sizeof(int));
    *res = 0;
    for (size_t i = 0; i < write_len; ) {
        size_t nitems = rand() % 16 + 1;
        if (i + nitems > write_len) {
            nitems = write_len - i;
        }
        for (size_t j = 0; j < nitems; j++) {
            temp[j] = seq;
            *res += seq;
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
    pthread_exit(res);
    return NULL;
}

void *read_sequence(void *arg_void) {
    arg_t *arg = (arg_t*)arg_void;
    ring *buf = arg->buf;
    size_t write_len = arg->write_len;
    size_t temp_len = 16;
    uint8_t *temp = malloc(temp_len * sizeof(uint8_t));
    uint8_t seq = 0;
    int *res = malloc(1 * sizeof(int));
    *res = 0;
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
                *res += temp[j];
            } else {
                if (temp[j] != seq) {
                    printf("mismatch at %zu: %u != %u\n", i + j, temp[j], seq);
                    free(temp);
                    *res = 1;
                    pthread_exit(res);
                    return NULL;
                }
            }
            seq++;
            seq %= seq_len;
        }
        i += nitems;
    }
    free(temp);
    pthread_exit(res);
    return NULL;
}


int main() {
    srand(time(NULL));
    ring *buf = ring_create(1 << 14);
    pthread_t w, w1, r, r1;

    // first we do a test with a single writer, single reader
    // in this test, the reader will ensure the sequence appears
    // strictly in the same order it is written
    arg_t args = {
        .buf = buf,
        .write_len = 1 << 22,
        .multi = false,
    };
    pthread_create(&w, NULL, write_sequence, &args);
    pthread_create(&r, NULL, read_sequence, &args);

    pthread_join(w, NULL);
    int res;
    void *res_p;
    pthread_join(r, &res_p);
    res = *(int*)res_p;
    free(res_p);

    printf("single reader, single writer test passed: %s\n", res ? "FALSE" : "TRUE");

    // now do 2 writers, 2 readers
    // we relax the sequence restriction and now just look for the same sums
    args.multi = true;

    pthread_create(&w, NULL, write_sequence, &args);
    pthread_create(&w1, NULL, write_sequence, &args);
    pthread_create(&r, NULL, read_sequence, &args);
    pthread_create(&r1, NULL, read_sequence, &args);

    int write_sum = 0;
    pthread_join(w, &res_p);
    write_sum += *(int*)res_p;
    free(res_p);
    pthread_join(w1, &res_p);
    write_sum += *(int*)res_p;
    free(res_p);
    int read_sum = 0;
    pthread_join(r, &res_p);
    read_sum += *(int*)res_p;
    free(res_p);
    pthread_join(r1, &res_p);
    read_sum += *(int*)res_p;
    free(res_p);

    printf("2 reader, 2 writer test passed: %s\n", (read_sum != write_sum) ? "FALSE" : "TRUE");
    res = res ? res : read_sum != write_sum;

    ring_destroy(buf);
    return res;
}
