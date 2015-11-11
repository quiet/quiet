#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEBUG_OFDMFLEXFRAMESYNC 1
#include <liquid/liquid.h>
#include <sndfile.h>


const int sample_rate = 44100;

float normalize_freq(float freq) {
    return (freq / sample_rate) * 2 * M_PI;
}


SNDFILE *wav_open(const char *fname) {
    SF_INFO sfinfo;

    memset(&sfinfo, 0, sizeof(sfinfo));

    return sf_open(fname, SFM_READ, &sfinfo);
}


size_t wav_read(SNDFILE *wav, float *samples, size_t sample_len) {
    return sf_read_float(wav, samples, sample_len);
}

void wav_close(SNDFILE *wav) {
    sf_close(wav);
}


typedef struct {
    nco_crcf nco;
    firdecim_crcf decim;
} Carrier;


const float carrier_freq = 11000.0f;
const unsigned int samples_per_symbol = 2;
const unsigned int symbol_delay = 11;
const float excess_bw = 0.2f;
const size_t num_coeffs = (2 * samples_per_symbol * symbol_delay) + 1;


Carrier *create_demodulator() {
    Carrier *c = malloc(sizeof(Carrier));

    c->nco = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(c->nco, 0.0f);
    nco_crcf_set_frequency(c->nco, normalize_freq(carrier_freq));

    float coeff_rev[num_coeffs], coeff[num_coeffs];
    liquid_firdes_prototype(LIQUID_FIRFILT_RRC, samples_per_symbol, symbol_delay, excess_bw, 0, coeff_rev);
    for (size_t i = 0; i < num_coeffs; i++) {
        coeff[i] = coeff_rev[num_coeffs - i - 1];
    }
    c->decim = firdecim_crcf_create(samples_per_symbol, coeff, num_coeffs);

    return c;
}

void demodulate(Carrier *carrier, float *samples, size_t sample_len, float complex *symbols) {
    if (sample_len % samples_per_symbol != 0) {
        printf("must receive multiple of samples_per_symbol samples");
        return;
    }

    float complex post_mixer[samples_per_symbol];

    for (size_t i = 0; i < sample_len; i += samples_per_symbol) {
        for (size_t j = 0; j < samples_per_symbol; j++) {
            nco_crcf_mix_down(carrier->nco, samples[i + j], &post_mixer[j]);
            nco_crcf_step(carrier->nco);
        }

        firdecim_crcf_execute(carrier->decim, &post_mixer[0], &symbols[(i/samples_per_symbol)]);
        symbols[(i/samples_per_symbol)] /= samples_per_symbol;
    }
}

void destroy_demodulator(Carrier *c) {
    nco_crcf_destroy(c->nco);
    firdecim_crcf_destroy(c->decim);
}


const size_t decode_buf_len = 1 << 20;
size_t accum = 0;

int on_decode(unsigned char *header, int header_valid, unsigned char *payload, unsigned int payload_len,
              int payload_valid, framesyncstats_s stats, void *vbuf) {
    printf("%d %d\n", header_valid, payload_valid);
    if (!header_valid || !payload_valid) {
        return 1;
    }

    if (payload_len > decode_buf_len) {
        payload_len = decode_buf_len;
    }
    memmove(vbuf, payload, payload_len);
    accum += payload_len;

    return 0;
}


const unsigned int num_subcarriers = 512;
const unsigned int cyclic_prefix_len = 16;
const unsigned int taper_len = 4;
const size_t encode_block_len = 4096;
const size_t encode_symbol_len = num_subcarriers + cyclic_prefix_len;
const size_t encode_sample_len = encode_symbol_len * samples_per_symbol;

int decode_wav(const char *wav_fname, const char *payload_fname) {
    FILE *payload = fopen(payload_fname, "wb");

    if (payload == NULL) {
        printf("failed to open payload file for writing\n");
        return 1;
    }

    SNDFILE *wav = wav_open(wav_fname);

    if (wav == NULL) {
        printf("failed to open wav file for reading\n");
        return 1;
    }

    unsigned char *writebuf = calloc(decode_buf_len, sizeof(unsigned char));
    Carrier *demodulator = create_demodulator();
    ofdmflexframesync framesync = ofdmflexframesync_create(num_subcarriers,
                                                           cyclic_prefix_len,
                                                           taper_len,
                                                           NULL,
                                                           on_decode,
                                                           writebuf);
    ofdmflexframesync_debug_enable(framesync);

    float *samplebuf = malloc(encode_sample_len * sizeof(float));
    float complex *symbolbuf = malloc(encode_symbol_len * sizeof(float complex));
    bool done = false;
    if (writebuf == NULL) {
        return 1;
    }
    if (symbolbuf == NULL) {
        return 1;
    }
    if (samplebuf == NULL) {
        return 1;
    }
    size_t wantread = 96;
    unsigned int i = 0;
    while (!done) {
        size_t nread = wav_read(wav, samplebuf, wantread);

        if (nread == 0) {
            break;
        } else if (nread < wantread) {
            done = true;
        }

        demodulate(demodulator, samplebuf, nread, symbolbuf);

        ofdmflexframesync_execute(framesync, symbolbuf, (nread/samples_per_symbol));

        if (accum > 0) {
            char fname[50];
            sprintf(fname, "framesync_%u.out", i);
            ofdmflexframesync_debug_print(framesync, fname);
            i++;

            fwrite(writebuf, sizeof(unsigned char), accum, payload);
            accum = 0;
        }
    }

    free(symbolbuf);
    free(samplebuf);
    ofdmflexframesync_destroy(framesync);
    destroy_demodulator(demodulator);
    free(writebuf);
    wav_close(wav);
    fclose(payload);
    return 0;
}



int main (int argc, char **argv) {
    decode_wav("encoded.wav", "payload_out");

    return 0;
}
