#include "quiet.h"

#include <sndfile.h>

float freq2rad(float freq) { return freq * 2 * M_PI; }

const int sample_rate = 48000;

float normalize_freq(float freq, float sample_rate) {
    return freq2rad(freq / (float)(sample_rate));
}

SNDFILE *wav_open(const char *fname, float sample_rate) {
    SF_INFO sfinfo;

    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = sample_rate;
    sfinfo.channels = 1;
    sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_FLOAT);

    return sf_open(fname, SFM_WRITE, &sfinfo);
}

size_t wav_write(SNDFILE *wav, const quiet_sample_t *samples, size_t sample_len) {
    return sf_write_float(wav, samples, sample_len);
}

void wav_close(SNDFILE *wav) { sf_close(wav); }

int encode_to_wav(FILE *payload, const char *out_fname,
                  const quiet_encoder_options *opt) {
    SNDFILE *wav = wav_open(out_fname, sample_rate);

    if (wav == NULL) {
        printf("failed to open wav file for writing\n");
        return 1;
    }

    quiet_encoder *e = quiet_encoder_create(opt, sample_rate);

    printf("created\n");

    size_t block_len = 16384;
    uint8_t *readbuf = malloc(block_len * sizeof(uint8_t));
    size_t samplebuf_len = 16384;
    quiet_sample_t *samplebuf = malloc(samplebuf_len * sizeof(quiet_sample_t));
    quiet_encoder_clamp_frame_len(e, samplebuf_len);
    bool done = false;
    if (readbuf == NULL) {
        return 1;
    }
    if (samplebuf == NULL) {
        return 1;
    }

    while (!done) {
        size_t nread = fread(readbuf, sizeof(uint8_t), block_len, payload);
        if (nread == 0) {
            break;
        } else if (nread < block_len) {
            done = true;
        }

        quiet_encoder_set_payload(e, readbuf, nread);

        printf("payload set\n");

        size_t written = samplebuf_len;
        while (written == samplebuf_len) {
            written = quiet_encoder_emit(e, samplebuf, samplebuf_len);
            for (size_t i = 0; i < written; i++) {
                if (samplebuf[i] > 1 || samplebuf[i] < -1) {
                    printf("%f\n", samplebuf[i]);
                }
            }
            wav_write(wav, samplebuf, written);
        }
    }

    quiet_encoder_destroy(e);
    free(readbuf);
    free(samplebuf);
    wav_close(wav);
    fclose(payload);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: encode_file <profilename> [<input_source>]\n");
        exit(1);
    }

    FILE *input;
    if ((argc == 2) || strncmp(argv[2], "-", 2) == 0) {
        input = stdin;
    } else {
        input = fopen(argv[2], "rb");
    }

    quiet_encoder_options *encodeopt =
        quiet_encoder_profile_filename("profiles.json", argv[1]);

    encode_to_wav(input, "encoded.wav", encodeopt);

    fclose(input);
    free(encodeopt);

    return 0;
}
