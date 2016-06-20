#ifdef QUIET_DEBUG
#define DEBUG_OFDMFLEXFRAMESYNC 1
#define DEBUG_FLEXFRAMESYNC 1
#endif
#include "quiet.h"
#include <math.h>
#include <string.h>
#include <sndfile.h>

float freq2rad(float freq) { return freq * 2 * M_PI; }

SNDFILE *wav_open(const char *fname, unsigned int *sample_rate) {
    SF_INFO sfinfo;

    memset(&sfinfo, 0, sizeof(sfinfo));

    SNDFILE *f = sf_open(fname, SFM_READ, &sfinfo);

    *sample_rate = sfinfo.samplerate;

    return f;
}

size_t wav_read(SNDFILE *wav, float *samples, size_t sample_len) {
    return sf_read_float(wav, samples, sample_len);
}

void wav_close(SNDFILE *wav) { sf_close(wav); }

void recv_all(quiet_decoder *d, uint8_t *buf,
              size_t bufsize, FILE *payload) {
    for (;;) {
        ssize_t read = quiet_decoder_recv(d, buf, bufsize);
        if (read < 0) {
            break;
        }
        fwrite(buf, 1, read, payload);
    }
}

int decode_wav(FILE *payload, const char *wav_fname,
               quiet_decoder_options *opt) {
    unsigned int sample_rate;
    SNDFILE *wav = wav_open(wav_fname, &sample_rate);

    if (wav == NULL) {
        printf("failed to open wav file for reading\n");
        return 1;
    }

    quiet_decoder *d = quiet_decoder_create(opt, sample_rate);
    size_t wantread = 16384;
    quiet_sample_t *samplebuf = malloc(wantread * sizeof(quiet_sample_t));
    if (samplebuf == NULL) {
        return 1;
    }
    bool done = false;
    size_t bufsize = 1 << 13;
    uint8_t *buf = malloc(bufsize);
    while (!done) {
        size_t nread = wav_read(wav, samplebuf, wantread);

        if (nread == 0) {
            break;
        } else if (nread < wantread) {
            done = true;
        }

        quiet_decoder_consume(d, samplebuf, nread);
        recv_all(d, buf, bufsize, payload);
    }

    quiet_decoder_flush(d);
    recv_all(d, buf, bufsize, payload);

    free(samplebuf);
    free(buf);
    quiet_decoder_destroy(d);
    wav_close(wav);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: decode_file <profilename> [<output_destination>]\n");
        exit(1);
    }

    FILE *output;
    if ((argc == 2) || strncmp(argv[2], "-", 2) == 0) {
        output = stdout;
    } else {
        output = fopen(argv[2], "wb");
        if (!output) {
            fprintf(stderr, "failed to open %s: ", argv[2]);
            perror(NULL);
            exit(1);
        }
    }

    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_filename(QUIET_PROFILES_LOCATION, argv[1]);

    if (!decodeopt) {
        printf("failed to read profile %s from %s\n", argv[1], QUIET_PROFILES_LOCATION);
        exit(1);
    }

#ifdef QUIET_DEBUG
    decodeopt->is_debug = true;
#endif

    decode_wav(output, "encoded.wav", decodeopt);

    fclose(output);
    free(decodeopt);

    return 0;
}
