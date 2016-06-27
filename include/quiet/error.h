#include "quiet.h"

#if QUIET_PTHREAD_ERROR
#include <pthread.h>
#endif

void quiet_set_last_error(quiet_error err);
