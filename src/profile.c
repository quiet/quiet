#include "quiet/common.h"
#include <jansson.h>

encoder_options *encoder_profile(json_t *root, const char *profilename) {
    json_t *profile = json_object_get(root, profilename);
    if (!profile) {
        quiet_set_last_error(quiet_profile_missing_key);
        return NULL;
    }

    encoder_options *opt = calloc(1, sizeof(encoder_options));
    if (!opt) {
        quiet_set_last_error(quiet_mem_fail);
        return NULL;
    }

    json_t *v;
    if ((v = json_object_get(profile, "checksum_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->checksum_scheme = (quiet_checksum_scheme_t)liquid_getopt_str2crc(scheme);
    }
    if ((v = json_object_get(profile, "inner_fec_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->inner_fec_scheme = (quiet_error_correction_scheme_t)liquid_getopt_str2fec(scheme);
    }
    if ((v = json_object_get(profile, "outer_fec_scheme"))) {
        const char *scheme = json_string_value(v);
        opt->outer_fec_scheme = (quiet_error_correction_scheme_t)liquid_getopt_str2fec(scheme);
    }
    if ((v = json_object_get(profile, "mod_scheme"))) {
        const char *scheme = json_string_value(v);
        if (strcmp(scheme, "gmsk") == 0) {
            opt->encoding = gmsk_encoding;
        } else {
            opt->encoding = modem_encoding; // this will be overriden later if ofdm
            opt->mod_scheme = (quiet_modulation_scheme_t)liquid_getopt_str2mod(scheme);
        }
    }
    if ((v = json_object_get(profile, "header"))) {
        json_t *vv;
        opt->header_override_defaults = true;
        if ((vv = json_object_get(v, "checksum_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_checksum_scheme = (quiet_checksum_scheme_t)liquid_getopt_str2crc(scheme);
        }
        if ((vv = json_object_get(v, "inner_fec_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_inner_fec_scheme = (quiet_error_correction_scheme_t)liquid_getopt_str2fec(scheme);
        }
        if ((vv = json_object_get(v, "outer_fec_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_outer_fec_scheme = (quiet_error_correction_scheme_t)liquid_getopt_str2fec(scheme);
        }
        if ((vv = json_object_get(v, "mod_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_mod_scheme = (quiet_modulation_scheme_t)liquid_getopt_str2mod(scheme);
        }
    }
    if ((v = json_object_get(profile, "frame_length"))) {
        opt->frame_len = json_integer_value(v);
    }
    if ((v = json_object_get(profile, "ofdm"))) {
        if (opt->encoding == gmsk_encoding) {
            free(opt);
            quiet_set_last_error(quiet_profile_invalid_profile);
            return NULL;
        }
        json_t *vv;
        opt->encoding = ofdm_encoding;
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
    }
    if ((v = json_object_get(profile, "modulation"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "center_frequency"))) {
            float center_frequency = json_number_value(vv);
            opt->modopt.center_rads = (center_frequency/SAMPLE_RATE) * M_PI * 2;
        }
        if ((vv = json_object_get(v, "gain"))) {
            float gain = json_number_value(vv);
            if (gain < 0 || gain > 0.5) {
                free(opt);
                quiet_set_last_error(quiet_profile_invalid_profile);
                return NULL;
            }
            opt->modopt.gain = gain;
        }
    }
    if ((v = json_object_get(profile, "interpolation"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "shape"))) {
            const char *shape = json_string_value(vv);
            if (strcmp(shape, "gmsk") == 0) {
                shape = "gmsktx";
            }
            opt->modopt.shape = liquid_getopt_str2firfilt(shape);
        } else {
            opt->modopt.shape = LIQUID_FIRFILT_KAISER;
        }
        if ((vv = json_object_get(v, "samples_per_symbol"))) {
            opt->modopt.samples_per_symbol = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "symbol_delay"))) {
            opt->modopt.symbol_delay = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "excess_bandwidth"))) {
            opt->modopt.excess_bw = json_number_value(vv);
        }
    } else {
        opt->modopt.samples_per_symbol = 1;
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

    return opt;
}

encoder_options *quiet_encoder_profile_file(FILE *f, const char *profilename) {
    json_error_t error;
    json_t *root = json_loadf(f, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    encoder_options *opt = encoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

encoder_options *quiet_encoder_profile_filename(const char *fname,
                                                const char *profilename) {
    json_error_t error;
    json_t *root = json_load_file(fname, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
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
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    encoder_options *opt = encoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

decoder_options *decoder_profile(json_t *root, const char *profilename) {
    json_t *profile = json_object_get(root, profilename);
    if (!profile) {
        quiet_set_last_error(quiet_profile_missing_key);
        return NULL;
    }

    decoder_options *opt = calloc(1, sizeof(decoder_options));
    if (!opt) {
        quiet_set_last_error(quiet_mem_fail);
        return NULL;
    }
    json_t *v;

    // we check mod_scheme only to find out if we are gmsk
    if ((v = json_object_get(profile, "mod_scheme"))) {
        const char *scheme = json_string_value(v);
        if (strcmp(scheme, "gmsk") == 0) {
            opt->encoding = gmsk_encoding;
        } else {
            opt->encoding = modem_encoding; // this will be overriden later if ofdm
        }
    }
    if ((v = json_object_get(profile, "header"))) {
        json_t *vv;
        opt->header_override_defaults = true;
        if ((vv = json_object_get(v, "checksum_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_checksum_scheme = (quiet_checksum_scheme_t)liquid_getopt_str2crc(scheme);
        }
        if ((vv = json_object_get(v, "inner_fec_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_inner_fec_scheme = (quiet_error_correction_scheme_t)liquid_getopt_str2fec(scheme);
        }
        if ((vv = json_object_get(v, "outer_fec_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_outer_fec_scheme = (quiet_error_correction_scheme_t)liquid_getopt_str2fec(scheme);
        }
        if ((vv = json_object_get(v, "mod_scheme"))) {
            const char *scheme = json_string_value(vv);
            opt->header_mod_scheme = (quiet_modulation_scheme_t)liquid_getopt_str2mod(scheme);
        }
    }
    if ((v = json_object_get(profile, "ofdm"))) {
        if (opt->encoding == gmsk_encoding) {
            free(opt);
            quiet_set_last_error(quiet_profile_invalid_profile);
            return NULL;
        }
        json_t *vv;
        opt->encoding = ofdm_encoding;
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
    }
    if ((v = json_object_get(profile, "modulation"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "center_frequency"))) {
            float center_frequency = json_number_value(vv);
            opt->demodopt.center_rads = (center_frequency/SAMPLE_RATE) * M_PI * 2;
        }
    }
    if ((v = json_object_get(profile, "interpolation"))) {
        json_t *vv;
        if ((vv = json_object_get(v, "shape"))) {
            const char *shape = json_string_value(vv);
            if (strcmp(shape, "gmsk") == 0) {
                shape = "gmskrx";
            }
            opt->demodopt.shape = liquid_getopt_str2firfilt(shape);
        } else {
            opt->demodopt.shape = LIQUID_FIRFILT_KAISER;
        }
        if ((vv = json_object_get(v, "samples_per_symbol"))) {
            opt->demodopt.samples_per_symbol = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "symbol_delay"))) {
            opt->demodopt.symbol_delay = json_integer_value(vv);
        }
        if ((vv = json_object_get(v, "excess_bandwidth"))) {
            opt->demodopt.excess_bw = json_number_value(vv);
        }
    } else { 
        opt->demodopt.samples_per_symbol = 1;
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

decoder_options *quiet_decoder_profile_file(FILE *f, const char *profilename) {
    json_error_t error;
    json_t *root = json_loadf(f, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    decoder_options *opt = decoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

decoder_options *quiet_decoder_profile_filename(const char *fname,
                                                const char *profilename) {
    json_error_t error;
    json_t *root = json_load_file(fname, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
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
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    decoder_options *opt = decoder_profile(root, profilename);
    json_decref(root);
    return opt;
}

char **profile_keys(json_t *root, size_t *size) {
    size_t numkeys = json_object_size(root);
    *size = numkeys;
    char **keys = malloc(numkeys*sizeof(char*));
    size_t i = 0;

    void *iter = json_object_iter(root);
    while (iter) {
        const char *nextkey = json_object_iter_key(iter);
        size_t keylen = strlen(nextkey) + 1;
        char *key = malloc(keylen*sizeof(char));
        strncpy(key, nextkey, keylen);
        keys[i] = key;
        i++;
        iter = json_object_iter_next(root, iter);
    }

    return keys;
}

char **quiet_profile_keys_file(FILE *f, size_t *size) {
    json_error_t error;
    json_t *root = json_loadf(f, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    char **keys = profile_keys(root, size);
    json_decref(root);
    return keys;
}

char **quiet_profile_keys_filename(const char *filename, size_t *size) {
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    char **keys = profile_keys(root, size);
    json_decref(root);
    return keys;
}

char **quiet_profile_keys_str(const char *input, size_t *size) {
    json_error_t error;
    json_t *root = json_loads(input, 0, &error);

    if (!root) {
        quiet_set_last_error(quiet_profile_malformed_json);
        return NULL;
    }

    char **keys = profile_keys(root, size);
    json_decref(root);
    return keys;
}
