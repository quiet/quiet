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


const float ceiling = 1.0 * 0x7f000000;
const size_t writeblock_len = 512;

void write_wav(char *fname, const float *samples, size_t sample_len) {
    SF_INFO sfinfo;

    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = sample_rate;
    sfinfo.frames = sample_len;
    sfinfo.channels = 1;
    sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_PCM_24);

    SNDFILE *file = sf_open(fname, SFM_WRITE, &sfinfo);

    if (!file) {
        printf("failed to open wav file for writing\n");
        return;
    }

    int *buf = malloc(writeblock_len * sizeof(int));
    for (size_t i = 0; i < sample_len; ) {
        size_t written = sample_len - i;
        if (written > writeblock_len) {
            written = writeblock_len;
        }

        for (size_t j = 0; j < written; j++, i++) {
            buf[j] = ceiling * samples[i];
        }

        if (sf_write_int(file, buf, written) != written) {
            printf("failed to write to wav file\n");
            printf("%s", sf_strerror(file));
        }
    }

    sf_close(file);
    free(buf);
    return;
}


const float carrier_freq = 6300.0f;

void modulate(const float complex *symbols, size_t symbol_len, float *samples) {
    nco_crcf carrier = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(carrier, 0.0f);
    nco_crcf_set_frequency(carrier, normalize_freq(carrier_freq));

    for (size_t i = 0; i < symbol_len; i++) {
        float complex sample;
        // TODO modulation depth?
        nco_crcf_mix_up(carrier, symbols[i], &sample);
        samples[i] = crealf(sample);
        nco_crcf_step(carrier);
    }

    nco_crcf_destroy(carrier);
}


int main(int argc, char **argv) {
    size_t sample_len = sample_rate * 4;
    float complex symbols[sample_len];
    float samples[sample_len];
    const float freq = normalize_freq(440.0);

    for (size_t k = 0; k < sample_len; k++) {
        symbols[k] = sin(freq * k);
    }

    modulate(symbols, sample_len, samples);

    write_wav("sine.wav", samples, sample_len);

    return 0;
}
