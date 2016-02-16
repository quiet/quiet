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

size_t wav_write(SNDFILE *wav, const sample_t *samples, size_t sample_len) {
    return sf_write_float(wav, samples, sample_len);
}

void wav_close(SNDFILE *wav) { sf_close(wav); }

int encode_to_wav(const char *payload_fname, const char *out_fname,
                  const encoder_options *opt) {
    FILE *payload = fopen(payload_fname, "rb");

    if (payload == NULL) {
        printf("failed to open payload file for reading\n");
        return 1;
    }

    SNDFILE *wav = wav_open(out_fname, sample_rate);

    if (wav == NULL) {
        printf("failed to open wav file for writing\n");
        return 1;
    }

    encoder *e = create_encoder(opt);

    printf("created\n");

    size_t block_len = 16384;
    uint8_t *readbuf = malloc(block_len * sizeof(uint8_t));
    size_t samplebuf_len = 16384;
    sample_t *samplebuf = malloc(samplebuf_len * sizeof(sample_t));
    encoder_clamp_frame_len(e, samplebuf_len);
    bool done = false;
    if (readbuf == NULL) {
        return 1;
    }
    if (samplebuf == NULL) {
        return 1;
    }

    sample_t *pad = calloc((sample_rate / 1000), sizeof(sample_t));  // ~1ms
    for (size_t i = 0; i < 5; i++) {
        wav_write(wav, pad, 18);
    }

    while (!done) {
        size_t nread = fread(readbuf, sizeof(uint8_t), block_len, payload);
        if (nread == 0) {
            break;
        } else if (nread < block_len) {
            done = true;
        }

        encoder_set_payload(e, readbuf, nread);

        printf("payload set\n");

        size_t written = samplebuf_len;
        while (written == samplebuf_len) {
            written = encode(e, samplebuf, samplebuf_len);
            for (size_t i = 0; i < written; i++) {
                if (samplebuf[i] > 1 || samplebuf[i] < -1) {
                    printf("%f\n", samplebuf[i]);
                }
            }
            wav_write(wav, samplebuf, written);
        }
    }

    for (size_t i = 0; i < 9; i++) {
        wav_write(wav, pad, 18);
    }

    destroy_encoder(e);
    free(readbuf);
    free(samplebuf);
    free(pad);
    wav_close(wav);
    fclose(payload);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: encodefile <profilename>\n");
        exit(1);
    }
    encoder_options *encodeopt =
        get_encoder_profile_file("profiles.json", argv[1]);
    encoder_opt_set_sample_rate(encodeopt, sample_rate);

    encode_to_wav("payload", "encoded.wav", encodeopt);

    free(encodeopt);

    return 0;
}
