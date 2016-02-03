#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include <liquid/liquid.h>
#include <jansson.h>

typedef float sample_t;

typedef struct { float alpha; } dc_filter_options;

typedef struct {
    float cutoff;
    unsigned int order;
} filter_options;

typedef struct {
    size_t delay;
    float bandwidth;
    float attenuation;
    size_t filter_bank_size;
} resampler_options;

typedef struct {
    unsigned int samples_per_symbol;
    unsigned int symbol_delay;
    float excess_bw;
    float center_rads;
    float gain;
    filter_options premix_filter_opt;
    dc_filter_options dc_filter_opt;
} modulator_options;

typedef struct {
    unsigned int samples_per_symbol;
    unsigned int symbol_delay;
    float excess_bw;
    float center_rads;
    filter_options mix_filter_opt;
} demodulator_options;

typedef struct {
    nco_crcf nco;
    firinterp_crcf interp;
    modulator_options opt;
    iirfilt_crcf premixfilter;
    firfilt_crcf mixfilter;
    iirfilt_crcf dcfilter;
} modulator;

typedef struct {
    nco_crcf nco;
    firdecim_crcf decim;
    demodulator_options opt;
    iirfilt_crcf premixfilter;
    iirfilt_crcf mixfilter;
} demodulator;

typedef struct {
    unsigned int num_subcarriers;
    unsigned int cyclic_prefix_len;
    unsigned int taper_len;
    size_t left_band;
    size_t right_band;
} ofdm_options;

typedef struct {
    unsigned int checksum_scheme;
    unsigned int inner_fec_scheme;
    unsigned int outer_fec_scheme;
    unsigned int mod_scheme;
    size_t frame_len;
    size_t dummy_prefix;
    size_t noise_prefix;
    bool is_ofdm;
    ofdm_options ofdmopt;
    modulator_options modopt;
    resampler_options resampler;
    float sample_rate;
} encoder_options;

typedef struct {
    bool is_ofdm;
    bool is_debug;
    ofdm_options ofdmopt;
    demodulator_options demodopt;
} decoder_options;

typedef struct { ofdmflexframegen framegen; } ofdm_encoder;

typedef struct {
    flexframegen framegen;
    size_t symbols_remaining;
} modem_encoder;

typedef struct {
    encoder_options opt;
    union {
        ofdm_encoder ofdm;
        modem_encoder modem;
    } frame;
    modulator *mod;
    float complex *symbolbuf;
    size_t symbolbuf_len;
    sample_t *samplebuf;
    size_t samplebuf_cap;
    size_t samplebuf_len;
    size_t samplebuf_offset;
    uint8_t *payload;
    size_t payload_length;
    bool has_flushed;
    size_t dummy_frames_remaining;
    size_t noise_prefix_remaining;
    float resample_rate;
    resamp_rrrf resampler;
} encoder;

typedef struct { ofdmflexframesync framesync; } ofdm_decoder;

typedef struct { flexframesync framesync; } modem_decoder;

typedef struct {
    decoder_options opt;
    union {
        ofdm_decoder ofdm;
        modem_decoder modem;
    } frame;
    demodulator *demod;
    uint8_t *writebuf;
    size_t writebuf_len;
    size_t writebuf_accum;
    float complex *symbolbuf;
    size_t symbolbuf_len;
    unsigned int i;
} decoder;

encoder_options *get_encoder_profile_file(const char *fname,
                                          const char *profilename);
encoder_options *get_encoder_profile_str(const char *input,
                                         const char *profilename);
decoder_options *get_decoder_profile_file(const char *fname,
                                          const char *profilename);
decoder_options *get_decoder_profile_str(const char *input,
                                         const char *profilename);
void encoder_opt_set_sample_rate(encoder_options *opt, float sample_rate);

modulator *create_modulator(const modulator_options *opt);
size_t modulate_sample_len(const modulator *m, size_t symbol_len);
// modulate assumes that samples is large enough to store symbol_len *
// samples_per_symbol samples
// returns number of samples written to *samples
size_t modulate(modulator *m, const float complex *symbols, size_t symbol_len,
                sample_t *samples);
size_t modulate_flush_sample_len(modulator *m);
size_t modulate_flush(modulator *m, sample_t *samples);
void destroy_modulator(modulator *m);

demodulator *create_demodulator(const demodulator_options *opt);
size_t demodulate_symbol_len(const demodulator *d, size_t sample_len);
size_t demodulate(demodulator *d, sample_t *samples, size_t sample_len,
                  float complex *symbols);
size_t demodulate_flush_symbol_len(demodulator *d);
size_t demodulate_flush(demodulator *d, float complex *symbols);
void destroy_demodulator(demodulator *d);

encoder *create_encoder(const encoder_options *opt);
int encoder_set_payload(encoder *e, uint8_t *payload, size_t payload_length);
size_t encoder_sample_len(encoder *e, size_t data_len);
size_t encode(encoder *e, sample_t *samplebuf, size_t samplebuf_len);
void destroy_encoder(encoder *e);

decoder *create_decoder(const decoder_options *opt);
size_t decoder_readbuf(decoder *d, uint8_t *data, size_t data_len);
size_t decode_max_len(decoder *d);
// returns number of uint8_ts accumulated in buf
size_t decode(decoder *d, sample_t *samplebuf, size_t sample_len);
size_t decode_flush(decoder *d);
void destroy_decoder(decoder *d);
