#include <assert.h>
#include <complex.h>

#include "quiet/common.h"
#include "quiet/demodulator.h"
#if RING_ATOMIC
#include "quiet/ring_atomic.h"
#else
#include "quiet/ring.h"
#endif

const size_t decoder_default_buffer_len = 1 << 16;

typedef struct { ofdmflexframesync framesync; } ofdm_decoder;

typedef struct { flexframesync framesync; } modem_decoder;

typedef struct { gmskframesync framesync; } gmsk_decoder;

struct quiet_decoder_frame_stats {
    // Raw symbols, in complex plane, as seen after decimation and downmixing
    const float complex *symbols;
    size_t num_symbols;

    // Magnitude of vector from received symbols to reference symbols, in dB
    float error_vector_magnitude;

    // Power level of received signal after decimation and downmixing, in dB
    float received_signal_strength_indicator;

    bool checksum_passed;
};

enum { num_frames_stats = 8 };

struct quiet_decoder {
    decoder_options opt;
    union {
        ofdm_decoder ofdm;
        modem_decoder modem;
        gmsk_decoder gmsk;
    } frame;
    demodulator *demod;
    float complex *symbolbuf;
    size_t symbolbuf_len;
    unsigned int i;
    float resample_rate;
    resamp_rrrf resampler;
    sample_t *baserate;
    size_t baserate_offset;
    unsigned int checksum_fails;
    ring *buf;
    uint8_t *writeframe;
    size_t writeframe_len;
    quiet_decoder_frame_stats stats[num_frames_stats];
    float complex *stats_symbols[num_frames_stats];
    size_t stats_symbol_caps[num_frames_stats];
    size_t num_frames_collected;
    bool stats_enabled;
};

static int decoder_on_decode(unsigned char *header, int header_valid, unsigned char *payload,
                             unsigned int payload_len, int payload_valid,
                             framesyncstats_s stats, void *dvoid);
static void decoder_ofdm_create(const decoder_options *opt, decoder *d);
static void decoder_modem_create(const decoder_options *opt, decoder *d);
static size_t decoder_max_len(decoder *d);
