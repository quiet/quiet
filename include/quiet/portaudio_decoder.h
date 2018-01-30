#include "quiet/portaudio.h"
#include "quiet/ring.h"

typedef quiet_portaudio_decoder portaudio_decoder;

struct quiet_portaudio_decoder {
    decoder *dec;
    size_t num_channels;
    size_t mono_buffer_size;
    quiet_sample_t *mono_buffer;
    ring *consume_ring;
    pthread_t consume_thread;
    pthread_mutex_t reflock;
    unsigned int refcnt;
    PaStream *stream;
};
