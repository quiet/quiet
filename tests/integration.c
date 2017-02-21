#include <math.h>
#include <time.h>

#include "quiet.h"

FILE *profiles_f;

int compare_chunk(const uint8_t *l, const uint8_t *r, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (l[i] != r[i]) {
            return -1;
        }
    }
    return 0;
}

int read_and_check(const uint8_t *payload, size_t payload_len,
                   size_t *accum, quiet_decoder *d, uint8_t *payload_decoded,
                   size_t payload_blocklen) {
    *accum = 0;
    for (;;) {
        ssize_t read = quiet_decoder_recv(d, payload_decoded, payload_blocklen);
        if (read < 0) {
            break;
        }
        if (read > payload_len) {
            printf("failed, decoded more payload than encoded, read=%zd, remaining payload=%zu\n", read, payload_len);
            return 1;
        }
        if (compare_chunk(payload, payload_decoded, read)) {
            printf("failed, decoded chunk differs from encoded payload, %zu payload remains\n", payload_len);
            return 1;
        }
        payload += read;
        payload_len -= read;
        *accum += read;
    }

    return 0;
}

int test_payload(const char *profile_name,
                 const uint8_t *payload, size_t payload_len,
                 unsigned int encode_rate, unsigned int decode_rate,
                 bool do_clamp) {
    fseek(profiles_f, 0, SEEK_SET);
    quiet_encoder_options *encodeopt =
        quiet_encoder_profile_file(profiles_f, profile_name);
    quiet_encoder *e = quiet_encoder_create(encodeopt, encode_rate);

    fseek(profiles_f, 0, SEEK_SET);
    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_file(profiles_f, profile_name);
    quiet_decoder *d = quiet_decoder_create(decodeopt, decode_rate);

    size_t samplebuf_len = 16384;
    quiet_sample_t *samplebuf = malloc(samplebuf_len * sizeof(quiet_sample_t));
    quiet_sample_t *silence = calloc(samplebuf_len, sizeof(quiet_sample_t));
    if (do_clamp) {
        quiet_encoder_clamp_frame_len(e, samplebuf_len);
    }

    size_t frame_len = quiet_encoder_get_frame_len(e);

    for (size_t sent = 0; sent < payload_len; sent += frame_len) {
        frame_len = (frame_len > (payload_len - sent)) ? (payload_len - sent) : frame_len;
        quiet_encoder_send(e, payload + sent, frame_len);
    }

    size_t payload_blocklen = 1 << 14;
    uint8_t *payload_decoded = malloc(payload_blocklen * sizeof(uint8_t));

    size_t written = samplebuf_len;
    size_t accum;
    while (written == samplebuf_len) {
        written = quiet_encoder_emit(e, samplebuf, samplebuf_len);
        if (written <= 0) {
            break;
        }
        quiet_decoder_consume(d, samplebuf, written);
        if (do_clamp) {
            quiet_decoder_consume(d, silence, samplebuf_len);
        }
        if (read_and_check(payload, payload_len, &accum, d, payload_decoded, payload_blocklen)) {
            return 1;
        }
        payload += accum;
        payload_len -= accum;
    }

    quiet_decoder_flush(d);
    if (read_and_check(payload, payload_len, &accum, d, payload_decoded, payload_blocklen)) {
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
    free(silence);
    free(encodeopt);
    free(decodeopt);
    quiet_encoder_destroy(e);
    quiet_decoder_destroy(d);
    return 0;
}

int test_profile(unsigned int encode_rate, unsigned int decode_rate, const char *profile) {
    size_t payload_lens[] = { 1, 2, 4, 12, 320, 399, 400, 797, 798, 799, 800, 1023 };
    size_t payload_lens_len = sizeof(payload_lens)/sizeof(size_t);
    bool do_close_frame[] = { false, true };
    size_t do_close_frame_len = sizeof(do_close_frame)/sizeof(bool);
    for (size_t i = 0; i < payload_lens_len; i++) {
        size_t payload_len = payload_lens[i];
        uint8_t *payload = malloc(payload_len*sizeof(uint8_t));
        for (size_t j = 0; j < payload_len; j++) {
            payload[j] = rand() & 0xff;
        }
        for (size_t j = 0; j < do_close_frame_len; j++) {
            printf("    payload_len=%6zu, close_frame=%s... ",
                   payload_len, (do_close_frame[j] ? " true":"false"));
            if (test_payload(profile, payload, payload_len,
                             encode_rate, decode_rate, do_close_frame[j])) {
                printf("FAILED\n");
                return -1;
            }
            printf("PASSED\n");
        }
        free(payload);
    }
    return 0;
}

int test_sample_rate_pair(unsigned int encode_rate, unsigned int decode_rate) {
    size_t num_profiles;
    fseek(profiles_f, 0, SEEK_SET);
    char **profiles = quiet_profile_keys_file(profiles_f, &num_profiles);
    for (size_t i = 0; i < num_profiles; i++) {
        const char *profile = profiles[i];
        printf("  profile=%s\n", profile);
        if (test_profile(encode_rate, decode_rate, profile)) {
            return -1;
        }
        free(profiles[i]);
    }
    free(profiles);
    return 0;
}

int main(int argc, char **argv) {
    profiles_f = fopen("test-profiles.json", "rb");
    srand(time(NULL));

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

    fclose(profiles_f);
    return 0;
}
