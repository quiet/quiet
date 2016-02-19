#define DEBUG_OFDMFLEXFRAMESYNC 1
#define DEBUG_FLEXFRAMESYNC 1
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

int decode_wav(const char *wav_fname, const char *payload_fname,
               quiet_decoder_options *opt) {
    FILE *payload = fopen(payload_fname, "wb");

    if (payload == NULL) {
        printf("failed to open payload file for writing\n");
        return 1;
    }

    unsigned int sample_rate;
    SNDFILE *wav = wav_open(wav_fname, &sample_rate);

    if (wav == NULL) {
        printf("failed to open wav file for reading\n");
        return 1;
    }
    quiet_decoder_opt_set_sample_rate(opt, sample_rate);

    quiet_decoder *d = quiet_decoder_create(opt);
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
    if (argc != 2) {
        printf("usage: encodefile <profilename>\n");
        exit(1);
    }
    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_file("profiles.json", argv[1]);

    decodeopt->is_debug = true;

    decode_wav("encoded.wav", "payload_out", decodeopt);

    free(decodeopt);

    return 0;
}
