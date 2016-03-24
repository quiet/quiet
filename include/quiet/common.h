#ifndef QUIET_COMMON_H
#define QUIET_COMMON_H
#include <string.h>
#include <complex.h>
#include <math.h>

#include "quiet.h"

#include <liquid/liquid.h>

typedef quiet_sample_t sample_t;
typedef quiet_dc_filter_options dc_filter_options;
typedef quiet_resampler_options resampler_options;
typedef quiet_modulator_options modulator_options;
typedef quiet_demodulator_options demodulator_options;
typedef quiet_ofdm_options ofdm_options;
typedef quiet_encoder_options encoder_options;
typedef quiet_decoder_options decoder_options;
typedef quiet_encoder encoder;
typedef quiet_decoder decoder;

typedef struct {
    size_t length;
    uint8_t *base;
    uint8_t *reader; // reader points to next block to be read
    uint8_t *writer; // writer points to next block to be written
} ring;

typedef struct {
    modulator_options opt;
    nco_crcf nco;
    firinterp_crcf interp;
    iirfilt_crcf dcfilter;
} modulator;

typedef struct {
    demodulator_options opt;
    nco_crcf nco;
    firdecim_crcf decim;
} demodulator;

typedef struct { ofdmflexframegen framegen; } ofdm_encoder;

typedef struct {
    flexframegen framegen;
    size_t symbols_remaining;
} modem_encoder;

typedef struct {
    gmskframegen framegen;
    size_t stride;
} gmsk_encoder;

struct quiet_encoder {
    encoder_options opt;
    union {
        ofdm_encoder ofdm;
        modem_encoder modem;
        gmsk_encoder gmsk;
    } frame;
    modulator *mod;
    float complex *symbolbuf;
    size_t symbolbuf_len;
    sample_t *samplebuf;
    size_t samplebuf_cap;
    size_t samplebuf_len;
    size_t samplebuf_offset;
    const uint8_t *payload;
    size_t payload_length;
    bool has_flushed;
    bool is_close_frame;
    float resample_rate;
    resamp_rrrf resampler;
    ring *buf;
    uint8_t *tempframe;
    uint8_t *readframe;
};

typedef struct { ofdmflexframesync framesync; } ofdm_decoder;

typedef struct { flexframesync framesync; } modem_decoder;

typedef struct { gmskframesync framesync; } gmsk_decoder;

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

static const float SAMPLE_RATE = 44100;
unsigned char *ofdm_subcarriers_create(const ofdm_options *opt);
size_t constrained_write(sample_t *src, size_t src_len, sample_t *dst,
                         size_t dest_len);
#endif  // QUIET_COMMON_H
