#include "quiet/portaudio.h"

typedef quiet_portaudio_decoder portaudio_decoder;

struct quiet_portaudio_decoder {
    decoder *dec;
    size_t sample_buffer_size;
    size_t num_channels;
    quiet_sample_t *sample_buffer;
    quiet_sample_t *mono_buffer;
    PaStream *stream;
};
