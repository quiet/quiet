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

int read_and_check(const uint8_t *payload, size_t payload_len,
                   size_t accum, decoder *d, uint8_t *payload_decoded,
                   size_t payload_blocklen) {
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

    return 0;
}

int test_profile(const char *profiles_fname, const char *profile_name,
                 const uint8_t *payload, size_t payload_len,
                 unsigned int encode_rate, unsigned int decode_rate) {
    encoder_options *encodeopt =
        get_encoder_profile_file(profiles_fname, profile_name);
    encoder_opt_set_sample_rate(encodeopt, encode_rate);
    encoder *e = create_encoder(encodeopt);

    decoder_options *decodeopt =
        get_decoder_profile_file(profiles_fname, profile_name);
    decoder_opt_set_sample_rate(decodeopt, decode_rate);
    decoder *d = create_decoder(decodeopt);

    size_t samplebuf_len = 16384;
    sample_t *samplebuf = malloc(samplebuf_len * sizeof(sample_t));

    encoder_set_payload(e, payload, payload_len);

    size_t payload_blocklen = 4096;
    uint8_t *payload_decoded = malloc(payload_blocklen * sizeof(uint8_t));

    size_t written = samplebuf_len;
    while (written == samplebuf_len) {
        written = encode(e, samplebuf, samplebuf_len);
        size_t accum = decode(d, samplebuf, written);
        if (read_and_check(payload, payload_len, accum, d, payload_decoded, payload_blocklen)) {
            return 1;
        }
        payload += accum;
        payload_len -= accum;
    }

    size_t accum = decode_flush(d);
    if (read_and_check(payload, payload_len, accum, d, payload_decoded, payload_blocklen)) {
        return 1;
    }
    payload += accum;
    payload_len -= accum;

    if (payload_len) {
        printf("failed, decoded less payload than encoded, remaining payload=%zu\n", payload_len);
        return 1;
    }

    free(payload_decoded);
    free(samplebuf);
    free(encodeopt);
    free(decodeopt);
    destroy_encoder(e);
    destroy_decoder(d);
    return 0;
}

int test_sample_rate_pair(unsigned int encode_rate, unsigned int decode_rate) {
    size_t payload_lens[] = { 1, 2, 4, 12, 320, 1023, 1<<16 };
    size_t payload_lens_len = sizeof(payload_lens)/sizeof(size_t);
    for (size_t i = 0; i < payload_lens_len; i++) {
        size_t payload_len = payload_lens[i];
        uint8_t *payload = malloc(payload_len*sizeof(uint8_t));
        for (size_t j = 0; j < payload_len; j++) {
            payload[j] = rand() & 0xff;
        }
        printf("testing encode_rate=%u, decode_rate=%u, payload_len=%6zu... ",
               encode_rate, decode_rate, payload_len);
        if (test_profile("test-profiles.json", "ofdm", payload, payload_len,
                         encode_rate, decode_rate)) {
            printf("FAILED\n");
            return -1;
        }
        printf("PASSED\n");
        free(payload);
    }
    return 0;
}

int main(int argc, char **argv) {
    unsigned int rates[][2]={ {44100, 44100}, {48000, 48000} };
    size_t rates_len = sizeof(rates)/(2 * sizeof(unsigned int));
    for (size_t i = 0; i < rates_len; i++) {
        unsigned int encode_rate = rates[i][0];
        unsigned int decode_rate = rates[i][1];
        printf("running tests on encode_rate=%u, decode_rate=%u\n", encode_rate, decode_rate);
        if (test_sample_rate_pair(encode_rate, decode_rate)) {
            return 1;
        }
    }
    return 0;
}
