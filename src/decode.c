#include "quiet/common.h"
#include "quiet/demodulator.h"

void decoder_opt_set_sample_rate(decoder_options *opt, float sample_rate) {
    opt->sample_rate = sample_rate;
}

static const size_t decoder_writebuf_initial_len = 2 << 12;

int _decoder_resize_buffer(decoder *d) {
    if (!d) {
        return 1;
    }

    size_t newlen = d->writebuf_len * 2;
    uint8_t *newbuf = realloc(d->writebuf, newlen);
    if (!newbuf) {
        return 1;
    }

    d->writebuf = newbuf;
    d->writebuf_len = newlen;

    return 0;
}

int _on_decode(unsigned char *header, int header_valid, unsigned char *payload,
               unsigned int payload_len, int payload_valid,
               framesyncstats_s stats, void *dvoid) {
    if (!header_valid || !payload_valid) {
        // XXX
        return 1;
    }

    if (!dvoid) {
        return 0;
    }

    decoder *d = dvoid;

    while (payload_len > (d->writebuf_len - d->writebuf_accum)) {
        if (!_decoder_resize_buffer(d)) {
            return 1;
        }
    }

    memmove(d->writebuf + d->writebuf_accum, payload, payload_len);
    d->writebuf_accum += payload_len;

    return 0;
}

void create_ofdm_decoder(const decoder_options *opt, decoder *d) {
    ofdm_decoder ofdm;

    unsigned char *subcarriers = create_ofdm_subcarriers(&opt->ofdmopt);
    ofdm.framesync = ofdmflexframesync_create(
        opt->ofdmopt.num_subcarriers, opt->ofdmopt.cyclic_prefix_len,
        opt->ofdmopt.taper_len, subcarriers, _on_decode, d);
    ofdmflexframesync_set_header_len(ofdm.framesync, 0);
    if (opt->is_debug) {
        ofdmflexframesync_debug_enable(ofdm.framesync);
    }

    size_t symbolbuf_len =
        opt->ofdmopt.num_subcarriers + opt->ofdmopt.cyclic_prefix_len;
    d->symbolbuf = malloc(symbolbuf_len *
                          sizeof(float complex));  // XXX check malloc result
    d->symbolbuf_len = symbolbuf_len;

    free(subcarriers);

    d->frame.ofdm = ofdm;
}

void create_modem_decoder(const decoder_options *opt, decoder *d) {
    modem_decoder modem;

    modem.framesync = flexframesync_create(_on_decode, d);
    flexframesync_set_header_len(modem.framesync, 0);
    if (opt->is_debug) {
        flexframesync_debug_enable(modem.framesync);
    }

    size_t symbolbuf_len = 256;
    d->symbolbuf = malloc(symbolbuf_len * sizeof(float complex));
    d->symbolbuf_len = symbolbuf_len;

    d->frame.modem = modem;
}

decoder *create_decoder(const decoder_options *opt) {
    if (!opt) {
        return NULL;
    }

    decoder *d = malloc(sizeof(decoder));

    d->opt = *opt;

    size_t writebuf_len = decoder_writebuf_initial_len;
    uint8_t *writebuf = malloc(writebuf_len);
    d->writebuf = writebuf;
    d->writebuf_len = writebuf_len;
    d->writebuf_accum = 0;

    if (opt->is_ofdm) {
        create_ofdm_decoder(opt, d);
    } else {
        create_modem_decoder(opt, d);
    }

    d->demod = create_demodulator(&(opt->demodopt));

    d->i = 0;
    d->resample_rate = 1;
    d->baserate = NULL;
    d->resampler = NULL;
    if (opt->sample_rate != SAMPLE_RATE) {
        float rate =  (float)SAMPLE_RATE / (float)opt->sample_rate;
        d->resampler = resamp_rrrf_create(rate, opt->resampler.delay,
                                          opt->resampler.bandwidth, opt->resampler.attenuation,
                                          opt->resampler.filter_bank_size);
        d->resample_rate = rate;
    }

    size_t stride_len = decode_max_len(d);
    d->baserate = malloc(stride_len * sizeof(sample_t));
    d->baserate_offset = 0;

    return d;
}

size_t decoder_readbuf(decoder *d, uint8_t *data, size_t data_len) {
    if (!d) {
        return 0;
    }

    if (data_len > d->writebuf_accum) {
        return 0;
    }

    memmove(data, d->writebuf, data_len);

    d->writebuf_accum -= data_len;
    memmove(d->writebuf, d->writebuf + data_len, d->writebuf_accum);

    return data_len;
}

size_t decode_max_len(decoder *d) {
    if (!d) {
        return 0;
    }

    return d->symbolbuf_len * d->demod->opt.samples_per_symbol;
}

// returns number of uint8_ts accumulated in buf
size_t decode(decoder *d, sample_t *samplebuf, size_t sample_len) {
    if (!d) {
        return 0;
    }

    size_t stride_len = decode_max_len(d);

    for (size_t i = 0; i < sample_len; ) {
        size_t symbol_len;
        if (d->resampler) {
            unsigned int resamp_read, resamp_write;
            resamp_rrrf_execute_output_block(d->resampler, samplebuf + i,
                                             sample_len - i, &resamp_read,
                                             d->baserate + d->baserate_offset,
                                             stride_len - d->baserate_offset,
                                             &resamp_write);
            i += resamp_read;
            resamp_write += d->baserate_offset;

            size_t leftover = 0;
            if (resamp_write % d->demod->opt.samples_per_symbol) {
                leftover = resamp_write % d->demod->opt.samples_per_symbol;
                resamp_write -= leftover;
            }

            symbol_len =
                demodulate(d->demod, d->baserate, resamp_write, d->symbolbuf);

            if (leftover) {
                memmove(d->baserate, d->baserate + resamp_write, leftover * sizeof(sample_t));
            }
            d->baserate_offset = leftover;
        } else {
            size_t bufsize = stride_len;
            size_t remaining = sample_len - i + d->baserate_offset;
            if (remaining < bufsize) {
                bufsize = remaining;
            }
            memmove(d->baserate + d->baserate_offset, samplebuf + i,
                    (bufsize - d->baserate_offset) * sizeof(sample_t));
            i += bufsize - d->baserate_offset;

            size_t leftover = 0;
            if (bufsize % d->demod->opt.samples_per_symbol) {
                leftover = bufsize % d->demod->opt.samples_per_symbol;
                bufsize -= leftover;
            }
            symbol_len = demodulate(d->demod, d->baserate, bufsize, d->symbolbuf);

            if (leftover) {
                memmove(d->baserate, d->baserate + bufsize, leftover * sizeof(sample_t));
            }
            d->baserate_offset = leftover;
        }

        if (d->opt.is_ofdm) {
            ofdmflexframesync_execute(d->frame.ofdm.framesync, d->symbolbuf,
                                      symbol_len);

            if (d->opt.is_debug) {
                char fname[50];
                sprintf(fname, "framesync_%u.out", d->i);
                ofdmflexframesync_debug_print(d->frame.ofdm.framesync, fname);
                d->i++;
            }
        } else {
            flexframesync_execute(d->frame.modem.framesync, d->symbolbuf,
                                  symbol_len);
            if (d->opt.is_debug) {
                char fname[50];
                sprintf(fname, "framesync_%u.out", d->i);
                flexframesync_debug_print(d->frame.modem.framesync, fname);
                d->i++;
            }
        }
    }

    return d->writebuf_accum;
}

size_t decode_flush(decoder *d) {
    if (!d) {
        return 0;
    }

    // XXX flush the resampler here

    size_t symbol_len = demodulate_flush(d->demod, d->symbolbuf);

    if (d->opt.is_ofdm) {
        ofdmflexframesync_execute(d->frame.ofdm.framesync, d->symbolbuf,
                                  symbol_len);
    } else {
        flexframesync_execute(d->frame.modem.framesync, d->symbolbuf,
                              symbol_len);
    }

    return d->writebuf_accum;
}

void destroy_decoder(decoder *d) {
    if (!d) {
        return;
    }

    if (d->opt.is_ofdm) {
        ofdmflexframesync_destroy(d->frame.ofdm.framesync);
    } else {
        flexframesync_destroy(d->frame.modem.framesync);
    }
    if (d->resampler) {
        resamp_rrrf_destroy(d->resampler);
    }
    if (d->baserate) {
        free(d->baserate);
    }
    destroy_demodulator(d->demod);
    free(d->symbolbuf);
    free(d->writebuf);
    free(d);
}
