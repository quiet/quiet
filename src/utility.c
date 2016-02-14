#include "quiet/common.h"

unsigned char *create_ofdm_subcarriers(const ofdm_options *opt) {
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
