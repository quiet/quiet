#include "quiet/error.h"

#if QUIET_PTHREAD_ERROR
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
    quiet_error *last = pthread_getspecific(quiet_last_error_key);
    if (!last) {
        last = malloc(sizeof(quiet_error));
        pthread_setspecific(quiet_last_error_key, last);
        *last = quiet_success;
    }
    return *last;
}

void quiet_set_last_error_pthread(quiet_error err) {
    pthread_once(&quiet_last_error_once, quiet_error_pthread_init);
    quiet_error *last = pthread_getspecific(quiet_last_error_key);
    if (!last) {
        last = malloc(sizeof(quiet_error));
        pthread_setspecific(quiet_last_error_key, last);
    }
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
#else
    return quiet_get_last_error_global();
#endif
}

void quiet_set_last_error(quiet_error err) {
#if QUIET_PTHREAD_ERROR
    quiet_set_last_error_pthread(err);
#else
    quiet_set_last_error_global(err);
#endif
}
