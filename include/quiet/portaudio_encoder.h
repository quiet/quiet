#ifdef _MSC_VER
#include <windows.h>
static inline void usleep(int micros)
{
    if (micros > 0 && micros < 1000) {
        return Sleep(1);
    }
    return Sleep(micros/1000);
}
#else
#include <unistd.h>
#endif

#include "quiet/portaudio.h"

typedef quiet_portaudio_encoder portaudio_encoder;

struct quiet_portaudio_encoder {
    encoder *enc;
    size_t sample_buffer_size;
    size_t num_channels;
    quiet_sample_t *sample_buffer; // unpacked, e.g. stereo copy w/ every other sample 0
    quiet_sample_t *mono_buffer;  // packed version written by quiet
    PaStream *stream;
};
