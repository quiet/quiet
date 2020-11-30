#include "quiet/common.h"

unsigned char *ofdm_subcarriers_create(const ofdm_options *opt) {
    unsigned char *subcarriers =
        malloc(opt->num_subcarriers * sizeof(unsigned char));
    // get the default subcarrier placement and then modify it slightly
    ofdmframe_init_default_sctype(opt->num_subcarriers, subcarriers);
    // now add some nulls
    size_t left_end = opt->num_subcarriers / 2;
    while (subcarriers[left_end] == OFDMFRAME_SCTYPE_NULL) {
        left_end--;
    }
    size_t right_end = opt->num_subcarriers / 2;
    while (subcarriers[right_end] == OFDMFRAME_SCTYPE_NULL) {
        right_end++;
    }
    // n.b. confusingly the left part of the array corresponds to the right band
    // and vice versa
    for (size_t i = 0; i < opt->right_band; i++) {
        subcarriers[left_end - i] = OFDMFRAME_SCTYPE_NULL;
    }
    for (size_t i = 0; i < opt->left_band; i++) {
        subcarriers[right_end + i] = OFDMFRAME_SCTYPE_NULL;
    }

    return subcarriers;
}

size_t constrained_write(sample_t *src, size_t src_len, sample_t *dst,
                         size_t dest_len) {
    size_t len = src_len;
    if (dest_len < src_len) {
        len = dest_len;
    }

    memmove(dst, src, len * sizeof(sample_t));

    return len;
}

quiet_modulation_scheme_t getopt_str2fskmod(const char *s) {
    if (strncmp(s, "fsk", 3) != 0) {
        return 0;
    }
    unsigned long num_symbols = strtol(s + 3, NULL, 10);
    if (num_symbols == 0) {
        return 0;
    }
    switch (num_symbols) {
    case 2:   return quiet_modulation_fsk2;
    case 4:   return quiet_modulation_fsk4;
    case 8:   return quiet_modulation_fsk8;
    case 16:  return quiet_modulation_fsk16;
    case 32:  return quiet_modulation_fsk32;
    case 64:  return quiet_modulation_fsk64;
    case 128: return quiet_modulation_fsk128;
    case 256: return quiet_modulation_fsk256;
    default:  return 0;
    }
}

unsigned int fskmod2bits(quiet_modulation_scheme_t scheme) {
    switch (scheme) {
    case quiet_modulation_fsk2:   return 1;
    case quiet_modulation_fsk4:   return 2;
    case quiet_modulation_fsk8:   return 3;
    case quiet_modulation_fsk16:  return 4;
    case quiet_modulation_fsk32:  return 5;
    case quiet_modulation_fsk64:  return 6;
    case quiet_modulation_fsk128: return 7;
    case quiet_modulation_fsk256: return 8;
    default: return 0;
    }
}
