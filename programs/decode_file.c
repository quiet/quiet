#ifdef QUIET_DEBUG
#define DEBUG_OFDMFLEXFRAMESYNC 1
#define DEBUG_FLEXFRAMESYNC 1
#endif
#include "quiet.h"
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
    size_t bufsize = 4096;
    uint8_t *buf = malloc(bufsize);
    while (!done) {
        size_t nread = wav_read(wav, samplebuf, wantread);

        if (nread == 0) {
            break;
        } else if (nread < wantread) {
            done = true;
        }

        size_t accum = quiet_decoder_recv(d, samplebuf, nread);

        if (accum > 0) {
            if (accum > bufsize) {
                bufsize = accum;
                buf = realloc(buf, bufsize);
            }
            size_t nquiet_decoderread = quiet_decoder_readbuf(d, buf, accum);
            fwrite(buf, 1, nquiet_decoderread, payload);
        }
    }

    size_t accum = quiet_decoder_flush(d);

    if (accum) {
        // XXX buffer overrun!!!
        size_t nquiet_decoderread = quiet_decoder_readbuf(d, buf, accum);
        fwrite(buf, 1, nquiet_decoderread, payload);
    }

    free(samplebuf);
    free(buf);
    quiet_decoder_destroy(d);
    wav_close(wav);
    fclose(payload);
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
    }

    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_file("profiles.json", argv[1]);

#ifdef QUIET_DEBUG
    decodeopt->is_debug = true;
#endif

    decode_wav(output, "encoded.wav", decodeopt);

    free(decodeopt);

    return 0;
}
