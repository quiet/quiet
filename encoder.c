#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <math.h>

#include    <sndfile.h>


#ifndef     M_PI
#define     M_PI        3.14159265358979323846264338
#endif


const int sample_rate = 44100;
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


int main(int argc, char **argv) {
    size_t sample_len = sample_rate * 4;
    float samples[sample_len];
    const float freq = (440.0 / sample_rate);

    for (size_t k = 0; k < sample_len; k++) {
        samples[k] = sin(freq * 2 * k * M_PI);
    }

    write_wav("sine.wav", samples, sample_len);

    return 0;
}
