#include <math.h>

#include "quiet.h"


int compare_chunk(const uint8_t *l, const uint8_t *r, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (l[i] != r[i]) {
            return -1;
        }
    }
    return 0;
}

int test_profile(const char *profiles_fname, const char *profile_name,
                 const uint8_t *payload, size_t payload_len,
                 unsigned int sample_rate) {
    encoder_options *encodeopt =
        get_encoder_profile_file(profiles_fname, profile_name);
    encoder_opt_set_sample_rate(encodeopt, sample_rate);
    encoder *e = create_encoder(encodeopt);

    decoder_options *decodeopt =
        get_decoder_profile_file(profiles_fname, profile_name);
    decoder_opt_set_sample_rate(decodeopt, sample_rate);
    decoder *d = create_decoder(decodeopt);

    size_t samplebuf_len = 16384;
    sample_t *samplebuf = malloc(samplebuf_len * sizeof(sample_t));
    encoder_clamp_frame_len(e, samplebuf_len);

    encoder_set_payload(e, payload, payload_len);

    size_t payload_blocklen = 4096;
    uint8_t *payload_decoded = malloc(payload_blocklen * sizeof(uint8_t));

    size_t written = samplebuf_len;
    while (written == samplebuf_len) {
        written = encode(e, samplebuf, samplebuf_len);
        size_t accum = decode(d, samplebuf, written);

        while (accum > 0) {
            size_t want = (payload_blocklen < accum) ? payload_blocklen : accum;
            size_t read = decoder_readbuf(d, payload_decoded, want);
            if (want != read) {
                printf("failed, read less from decoder than asked for, want=%zu, read=%zu\n", want, read);
                return 1;
            }
            if (read > payload_len) {
                printf("failed, decoded more payload than encoded, read=%zu, remaining payload=%zu\n", read, payload_len);
                return 1;
            }
            if (compare_chunk(payload, payload_decoded, read)) {
                printf("failed, decoded chunk differs from encoded payload\n");
                return 1;
            }
            payload += read;
            payload_len -= read;
            accum -= read;
        }
    }

    if (payload_len) {
        printf("failed, decoded less payload than encoded, remaining payload=%zu\n", payload_len);
        return 1;
    }

    free(payload_decoded);
    free(samplebuf);
    destroy_encoder(e);
    destroy_decoder(d);
    return 0;
}

int main(int argc, char **argv) {
    size_t payload_len = 1<<16;
    uint8_t *payload = malloc(payload_len*sizeof(uint8_t));
    for (size_t i = 0; i < payload_len; i++) {
        payload[i] = rand() & 0xff;
    }
    return test_profile("test-profiles.json", "ofdm", payload, payload_len, 44100);
}
