#include "quiet/common.h"

demodulator *create_demodulator(const demodulator_options *opt) {
    if (!opt) {
        return NULL;
    }

    demodulator *d = malloc(sizeof(demodulator));

    d->opt = *opt;

    d->nco = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(d->nco, 0.0f);
    nco_crcf_set_frequency(d->nco, opt->center_rads);

    if (opt->samples_per_symbol > 1) {
        d->decim = firdecim_crcf_create_kaiser(opt->samples_per_symbol,
                                               opt->symbol_delay, 60.0f);
    } else {
        d->opt.samples_per_symbol = 1;
        d->opt.symbol_delay = 0;
        d->decim = NULL;
    }

    return d;
}

size_t demodulate_symbol_len(const demodulator *d, size_t sample_len) {
    if (!d) {
        return 0;
    }

    if (sample_len % d->opt.samples_per_symbol != 0) {
        printf("must receive multiple of samples_per_symbol samples");
        return 0;
    }

    return sample_len / d->opt.samples_per_symbol;
}

size_t demodulate(demodulator *d, sample_t *samples, size_t sample_len,
                  float complex *symbols) {
    if (!d) {
        return 0;
    }

    if (sample_len % d->opt.samples_per_symbol != 0) {
        printf("must receive multiple of samples_per_symbol samples");
        return 0;
    }

    float complex post_mixer[d->opt.samples_per_symbol];
    size_t written = 0;
    for (size_t i = 0; i < sample_len; i += d->opt.samples_per_symbol) {
        for (size_t j = 0; j < d->opt.samples_per_symbol; j++) {
            nco_crcf_mix_down(d->nco, samples[i + j], &post_mixer[j]);
            nco_crcf_step(d->nco);
        }

        if (d->decim) {
            firdecim_crcf_execute(d->decim, &post_mixer[0],
                                  &symbols[(i / d->opt.samples_per_symbol)]);
            symbols[(i / d->opt.samples_per_symbol)] /=
                d->opt.samples_per_symbol;
        } else {
            symbols[i] = post_mixer[0];
        }
        written++;
    }

    return written;
}

size_t demodulate_flush_symbol_len(demodulator *d) {
    if (!d) {
        return 0;
    }

    return 2 * d->opt.symbol_delay;
}

size_t demodulate_flush(demodulator *d, float complex *symbols) {
    if (!d) {
        return 0;
    }

    size_t sample_len = 2 * d->opt.symbol_delay * d->opt.samples_per_symbol;
    sample_t terminate[sample_len];
    for (size_t i = 0; i < sample_len; i++) {
        terminate[i] = 0;
    }

    return demodulate(d, terminate, sample_len, symbols);
}

void destroy_demodulator(demodulator *d) {
    if (!d) {
        return;
    }

    nco_crcf_destroy(d->nco);
    if (d->decim) {
        firdecim_crcf_destroy(d->decim);
    }
    free(d);
}
