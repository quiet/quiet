#include "quiet/common.h"
#include <jansson.h>

encoder_options *encoder_profile(json_t *root, const char *profilename) {
    json_t *profile = json_object_get(root, profilename);
    if (!profile) {
        printf("failed to access profile %s\n", profilename);
        return NULL;
    }

    encoder_options *opt = calloc(1, sizeof(encoder_options));
    if (!opt) {
        printf("allocation of encoder_options failed\n");
        return NULL;
    }

    json_t *v;
    if ((v = json_object_get(profile, "checksum_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->checksum_scheme = liquid_getopt_str2crc(scheme);
    }
    if ((v = json_object_get(profile, "inner_fec_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->inner_fec_scheme = liquid_getopt_str2fec(scheme);
    }
    if ((v = json_object_get(profile, "outer_fec_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->outer_fec_scheme = liquid_getopt_str2fec(scheme);
    }
    if ((v = json_object_get(profile, "mod_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->mod_scheme = liquid_getopt_str2mod(scheme);
    }
    if ((v = json_object_get(profile, "frame_length"))) {
        opt->frame_len = json_integer_value(v);
    }
    if ((v = json_object_get(profile, "ofdm"))) {
        json_t *vv;
        opt->is_ofdm = true;
        if ((vv = json_object_get(v, "num_subcarriers"))) {
            opt->ofdmopt.num_subcarriers = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "cyclic_prefix_length"))) {
            opt->ofdmopt.cyclic_prefix_len = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "taper_length"))) {
            opt->ofdmopt.taper_len = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "left_band"))) {
            opt->ofdmopt.left_band = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "right_band"))) {
            opt->ofdmopt.right_band = json_integer_value(vv);
        }
    } else {
        opt->is_ofdm = false;
    }
    if ((v = json_object_get(profile, "modulation"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "samples_per_symbol"))) {
            opt->modopt.samples_per_symbol = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "symbol_delay"))) {
            opt->modopt.symbol_delay = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "excess_bandwidth"))) {
            opt->modopt.excess_bw = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "center_radians"))) {
            opt->modopt.center_rads = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "gain"))) {
            opt->modopt.gain = json_number_value(vv);
        }
    }
    if ((v = json_object_get(profile, "encoder_filters"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "dc_filter_alpha"))) {
            opt->modopt.dc_filter_opt.alpha = json_number_value(vv);
        }
    }
    if ((v = json_object_get(profile, "resampler"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "delay"))) {
            opt->resampler.delay = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "bandwidth"))) {
            opt->resampler.bandwidth = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "attenuation"))) {
            opt->resampler.attenuation = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "filter_bank_size"))) {
            opt->resampler.filter_bank_size = json_number_value(vv);
        }
    }

    opt->sample_rate = SAMPLE_RATE;

    return opt;
}

encoder_options *quiet_encoder_profile_file(const char *fname,
                                            const char *profilename) {
    json_error_t error;
    json_t *root = json_load_file(fname, 0, &error);

    if (!root) {
        printf("failed to read profiles\n");
        return NULL;
    }

    encoder_options *opt = encoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

encoder_options *quiet_encoder_profile_str(const char *input,
                                           const char *profilename) {
    json_error_t error;
    json_t *root = json_loads(input, 0, &error);

    if (!root) {
        printf("failed to read profiles\n");
        return NULL;
    }

    encoder_options *opt = encoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

decoder_options *decoder_profile(json_t *root, const char *profilename) {
    json_t *profile = json_object_get(root, profilename);
    if (!profile) {
        printf("failed to access profile %s\n", profilename);
        return NULL;
    }

    decoder_options *opt = calloc(1, sizeof(decoder_options));
    if (!opt) {
        printf("allocation of decoder_options failed\n");
        return NULL;
    }

    json_t *v;
    if ((v = json_object_get(profile, "ofdm"))) {
        json_t *vv;
        opt->is_ofdm = true;
        if ((vv = json_object_get(v, "num_subcarriers"))) {
            opt->ofdmopt.num_subcarriers = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "cyclic_prefix_length"))) {
            opt->ofdmopt.cyclic_prefix_len = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "taper_length"))) {
            opt->ofdmopt.taper_len = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "left_band"))) {
            opt->ofdmopt.left_band = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "right_band"))) {
            opt->ofdmopt.right_band = json_integer_value(vv);
        }
    } else {
        opt->is_ofdm = false;
    }
    if ((v = json_object_get(profile, "modulation"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "samples_per_symbol"))) {
            opt->demodopt.samples_per_symbol = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "symbol_delay"))) {
            opt->demodopt.symbol_delay = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "excess_bandwidth"))) {
            opt->demodopt.excess_bw = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "center_radians"))) {
            opt->demodopt.center_rads = json_number_value(vv);
        }
    }
    if ((v = json_object_get(profile, "resampler"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "delay"))) {
            opt->resampler.delay = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "bandwidth"))) {
            opt->resampler.bandwidth = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "attenuation"))) {
            opt->resampler.attenuation = json_number_value(vv);
        }
        if ((vv = json_object_get(v, "filter_bank_size"))) {
            opt->resampler.filter_bank_size = json_number_value(vv);
        }
    }

    return opt;
}

decoder_options *quiet_decoder_profile_file(const char *fname,
                                            const char *profilename) {
    json_error_t error;
    json_t *root = json_load_file(fname, 0, &error);

    if (!root) {
        printf("failed to read profiles\n");
        return NULL;
    }

    decoder_options *opt = decoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

decoder_options *quiet_decoder_profile_str(const char *input,
                                           const char *profilename) {
    json_error_t error;
    json_t *root = json_loads(input, 0, &error);

    if (!root) {
        printf("failed to read profiles\n");
        return NULL;
    }

    decoder_options *opt = decoder_profile(root, profilename);
    json_decref(root);
    return opt;
}
