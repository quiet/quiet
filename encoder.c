#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <liquid/liquid.h>
#include <sndfile.h>

const int sample_rate = 44100;

float normalize_freq(float freq) {
    return (freq / sample_rate) * 2 * M_PI;
}

SNDFILE *wav_open(const char *fname) {
    SF_INFO sfinfo;

    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = sample_rate;
    //sfinfo.frames = sample_len;
    sfinfo.channels = 1;
    sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_FLOAT);

    return sf_open(fname, SFM_WRITE, &sfinfo);
}

size_t wav_write(SNDFILE *wav, const float *samples, size_t sample_len) {
    return sf_write_float(wav, samples, sample_len);
}

void wav_close(SNDFILE *wav) {
    sf_close(wav);
}


typedef struct {
    nco_crcf nco;
    firinterp_crcf interp;
} Carrier;


const float carrier_freq = 3150.0f;
const unsigned int samples_per_symbol = 2;
const unsigned int symbol_delay = 3;
const float excess_bw = 0.3f;
const size_t num_coeffs = (2 * samples_per_symbol * symbol_delay) + 1;


Carrier *create_modulator() {
    Carrier *c = malloc(sizeof(Carrier));

    c->nco = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(c->nco, 0.0f);
    nco_crcf_set_frequency(c->nco, normalize_freq(carrier_freq));

    float coeff[num_coeffs];
    liquid_firdes_prototype(LIQUID_FIRFILT_RRC, samples_per_symbol, symbol_delay, excess_bw, 0, coeff);
    c->interp = firinterp_crcf_create(samples_per_symbol, coeff, num_coeffs);

    return c;
}

void modulate(Carrier *carrier, const float complex *symbols, size_t symbol_len, float *samples) {
    float complex post_filter[samples_per_symbol];
    for (size_t i = 0; i < symbol_len; i++) {
        firinterp_crcf_execute(carrier->interp, symbols[i], &post_filter[0]);
        for (size_t j = 0; j < samples_per_symbol; j++) {
            float c, s;
            nco_crcf_sincos(carrier->nco, &s, &c);
            nco_crcf_step(carrier->nco);
            samples[i * samples_per_symbol + j] = (crealf(post_filter[j]) * c) - (cimagf(post_filter[j]) * s);
        }
    }
}

void destroy_modulator(Carrier *c) {
    nco_crcf_destroy(c->nco);
    firinterp_crcf_destroy(c->interp);
}


const unsigned int num_subcarriers = 64;
const unsigned int cyclic_prefix_len = 16;
const unsigned int taper_len = 4;
const size_t encode_block_len = 120;
const size_t encode_symbol_len = num_subcarriers + cyclic_prefix_len;
const size_t encode_sample_len = encode_symbol_len * samples_per_symbol;

int encode_to_wav(const char *payload_fname, const char *out_fname) {
    FILE *payload = fopen(payload_fname, "rb");

    if (payload == NULL) {
        printf("failed to open payload file for reading\n");
        return 1;
    }

    SNDFILE *wav = wav_open(out_fname);

    if (wav == NULL) {
        printf("failed to open wav file for writing\n");
        return 1;
    }

    ofdmflexframegen framegen = ofdmflexframegen_create(num_subcarriers,
                                                        cyclic_prefix_len,
                                                        taper_len,
                                                        NULL,
                                                        NULL);

    Carrier *modulator = create_modulator();

    unsigned char *readbuf = malloc(encode_block_len * sizeof(unsigned char));
    float complex *symbolbuf = malloc(encode_symbol_len * sizeof(float complex));
    float *samplebuf = malloc(encode_sample_len * sizeof(float));
    bool done = false;
    if (readbuf == NULL) {
        return 1;
    }
    if (symbolbuf == NULL) {
        return 1;
    }
    if (samplebuf == NULL) {
        return 1;
    }
    float pad[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    wav_write(wav, pad, 18);
    while (!done) {
        size_t nread = fread(readbuf, sizeof(unsigned char), encode_block_len, payload);
        if (nread == 0) {
            break;
        } else if (nread < encode_block_len) {
            done = true;
        }

        unsigned char header[8];
        ofdmflexframegen_assemble(framegen, header, readbuf, encode_block_len);

        int last_symbol = 0;
        while (!last_symbol) {
            last_symbol = ofdmflexframegen_writesymbol(framegen, symbolbuf);
            modulate(modulator, symbolbuf, encode_symbol_len, samplebuf);
            wav_write(wav, samplebuf, encode_sample_len);
            printf("%lu samples written.\n", encode_sample_len);
        }

    }

    wav_write(wav, pad, 18);

    free(readbuf);
    free(symbolbuf);
    free(samplebuf);
    ofdmflexframegen_destroy(framegen);
    destroy_modulator(modulator);
    wav_close(wav);
    fclose(payload);
    return 0;
}


int main(int argc, char **argv) {
    encode_to_wav("payload", "encoded.wav");

    return 0;
}
