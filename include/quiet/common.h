#ifndef QUIET_COMMON_H
#define QUIET_COMMON_H
#include <string.h>
#include <complex.h>
#include <math.h>

#include "quiet.h"
#include "quiet/error.h"
#include "quiet/ring_common.h"

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

static const float SAMPLE_RATE = 44100;
unsigned char *ofdm_subcarriers_create(const ofdm_options *opt);
size_t constrained_write(sample_t *src, size_t src_len, sample_t *dst,
                         size_t dest_len);
#endif  // QUIET_COMMON_H
