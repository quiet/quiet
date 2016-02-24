#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>

typedef float quiet_sample_t;

typedef struct { float alpha; } quiet_dc_filter_options;

typedef struct {
    size_t delay;
    float bandwidth;
    float attenuation;
    size_t filter_bank_size;
} quiet_resampler_options;

typedef struct {
    unsigned int samples_per_symbol;
    unsigned int symbol_delay;
    float excess_bw;
    float center_rads;
    float gain;
    quiet_dc_filter_options dc_filter_opt;
} quiet_modulator_options;

typedef struct {
    unsigned int samples_per_symbol;
    unsigned int symbol_delay;
    float excess_bw;
    float center_rads;
} quiet_demodulator_options;

typedef struct {
    unsigned int num_subcarriers;
    unsigned int cyclic_prefix_len;
    unsigned int taper_len;
    size_t left_band;
    size_t right_band;
} quiet_ofdm_options;

typedef struct {
    quiet_ofdm_options ofdmopt;
    quiet_modulator_options modopt;
    quiet_resampler_options resampler;
    unsigned int checksum_scheme;
    unsigned int inner_fec_scheme;
    unsigned int outer_fec_scheme;
    unsigned int mod_scheme;
    size_t frame_len;
    size_t dummy_prefix;
    size_t noise_prefix;
    bool is_ofdm;
    float sample_rate;
    bool is_close_frame;
} quiet_encoder_options;

typedef struct {
    quiet_ofdm_options ofdmopt;
    quiet_demodulator_options demodopt;
    quiet_resampler_options resampler;
    bool is_ofdm;
    bool is_debug;
    float sample_rate;
} quiet_decoder_options;

typedef struct quiet_encoder_s quiet_encoder;

typedef struct quiet_decoder_s quiet_decoder;

quiet_encoder_options *quiet_encoder_profile_file(const char *fname,
                                                  const char *profilename);
quiet_encoder_options *quiet_encoder_profile_str(const char *input,
                                                 const char *profilename);
quiet_decoder_options *quiet_decoder_profile_file(const char *fname,
                                                  const char *profilename);
quiet_decoder_options *quiet_decoder_profile_str(const char *input,
                                                 const char *profilename);
void quiet_encoder_opt_set_sample_rate(quiet_encoder_options *opt, float sample_rate);
void quiet_decoder_opt_set_sample_rate(quiet_decoder_options *opt, float sample_rate);

quiet_encoder *quiet_encoder_create(const quiet_encoder_options *opt);
size_t quiet_encoder_clamp_frame_len(quiet_encoder *e, size_t sample_len);
int quiet_encoder_set_payload(quiet_encoder *e, const uint8_t *payload, size_t payload_length);
size_t quiet_encoder_sample_len(quiet_encoder *e, size_t data_len);
size_t quiet_encoder_emit(quiet_encoder *e, quiet_sample_t *samplebuf, size_t samplebuf_len);
void quiet_encoder_destroy(quiet_encoder *e);

quiet_decoder *quiet_decoder_create(const quiet_decoder_options *opt);
size_t quiet_decoder_readbuf(quiet_decoder *d, uint8_t *data, size_t data_len);
// returns number of uint8_ts accumulated in buf
size_t quiet_decoder_recv(quiet_decoder *d, quiet_sample_t *samplebuf, size_t sample_len);
size_t quiet_decoder_flush(quiet_decoder *d);
unsigned int quiet_decoder_checksum_fails(const quiet_decoder *d);
void quiet_decoder_destroy(quiet_decoder *d);
