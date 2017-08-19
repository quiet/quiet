#include "quiet/error.h"

#if QUIET_PTHREAD_ERROR
#include <pthread.h>
static pthread_once_t quiet_last_error_once = PTHREAD_ONCE_INIT;
static pthread_key_t quiet_last_error_key;

static void quiet_error_pthread_destroy(void *error_v) {
    free((quiet_error*)error_v);
}

static void quiet_error_pthread_init() {
    pthread_key_create(&quiet_last_error_key, quiet_error_pthread_destroy);
}

quiet_error quiet_get_last_error_pthread() {
    pthread_once(&quiet_last_error_once, quiet_error_pthread_init);
    quiet_error *last = (quiet_error*)pthread_getspecific(quiet_last_error_key);
    if (!last) {
        last = (quiet_error*)malloc(sizeof(quiet_error));
        pthread_setspecific(quiet_last_error_key, last);
        *last = quiet_success;
    }
    return *last;
}

void quiet_set_last_error_pthread(quiet_error err) {
    pthread_once(&quiet_last_error_once, quiet_error_pthread_init);
    quiet_error *last = (quiet_error*)pthread_getspecific(quiet_last_error_key);
    if (!last) {
        last = (quiet_error*)malloc(sizeof(quiet_error));
        pthread_setspecific(quiet_last_error_key, last);
    }
    *last = err;
}
#elif QUIET_WIN_ERROR
#include <windows.h>
INIT_ONCE quiet_last_error_once = INIT_ONCE_STATIC_INIT;
static DWORD thread_local_error_key;

int CALLBACK quiet_error_win_init(INIT_ONCE *once, void *, void *) {
    thread_local_error_key = TlsAlloc();
    quiet_error *last = (quiet_error*)malloc(sizeof(quiet_error));
    *last = quiet_success;
    TlsSetValue(thread_local_error_key, last);
    return 1;
}

quiet_error quiet_get_last_error_win() {
    InitOnceExecuteOnce(&quiet_last_error_once, quiet_error_win_init, NULL, NULL);
    return *(quiet_error*)TlsGetValue(thread_local_error_key);
}

void quiet_set_last_error_win(quiet_error err) {
    InitOnceExecuteOnce(&quiet_last_error_once, quiet_error_win_init, NULL, NULL);
    quiet_error *last = (quiet_error*)TlsGetValue(thread_local_error_key);
    *last = err;
}

#else
static quiet_error quiet_last_error;

quiet_error quiet_get_last_error_global() {
    return quiet_last_error;
}

void quiet_set_last_error_global(quiet_error err) {
    quiet_last_error = err;
}
#endif

quiet_error quiet_get_last_error() {
#if QUIET_PTHREAD_ERROR
    return quiet_get_last_error_pthread();
#elif QUIET_WIN_ERROR
    return quiet_get_last_error_win();
#else
    return quiet_get_last_error_global();
#endif
}

void quiet_set_last_error(quiet_error err) {
#if QUIET_PTHREAD_ERROR
    quiet_set_last_error_pthread(err);
#elif QUIET_WIN_ERROR
    quiet_set_last_error_win(err);
#else
    quiet_set_last_error_global(err);
#endif
}
