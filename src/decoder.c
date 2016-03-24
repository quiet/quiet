#include "quiet/decoder.h"

unsigned int quiet_decoder_checksum_fails(const quiet_decoder *d) {
    return d->checksum_fails;
}

static int decoder_on_decode(unsigned char *header, int header_valid, unsigned char *payload,
                             unsigned int payload_len, int payload_valid,
                             framesyncstats_s stats, void *dvoid) {
    if (!header_valid) {
        // XXX
        return 1;
    }

    if (!dvoid) {
        return 0;
    }

    decoder *d = dvoid;

    if (d->stats_enabled) {
        size_t stats_index = d->num_frames_collected;
        if (stats_index < num_frames_stats) {
            float complex *sym = d->stats_symbols[stats_index];
            size_t sym_cap = d->stats_symbol_caps[stats_index];

            if (sym_cap < stats.num_framesyms) {
                d->stats_symbols[stats_index] = realloc(sym, stats.num_framesyms * sizeof(float complex));
                d->stats_symbol_caps[stats_index] = stats.num_framesyms;
            }
            memcpy(d->stats_symbols[stats_index], stats.framesyms, stats.num_framesyms * sizeof(float complex));

            quiet_decoder_frame_stats *fstats = d->stats + stats_index;

            fstats->symbols = d->stats_symbols[stats_index];
            fstats->num_symbols = stats.num_framesyms;
            fstats->error_vector_magnitude = stats.evm;
            fstats->received_signal_strength_indicator = stats.rssi;
            fstats->checksum_passed = payload_valid;

            d->num_frames_collected++;
        }
    }


    if (!payload_valid) {
        d->checksum_fails++;
        return 1;
    }

    size_t framelen = payload_len + sizeof(size_t);
    if (framelen > d->writeframe_len) {
        d->writeframe = realloc(d->writeframe, framelen);
        d->writeframe_len = framelen;
    }

    size_t len = payload_len;
    memcpy(d->writeframe, &len, sizeof(size_t));
    memcpy(d->writeframe + (sizeof(size_t)), payload, len);

    ring_write(d->buf, d->writeframe, framelen);
    return 0;
}

static void decoder_ofdm_create(const decoder_options *opt, decoder *d) {
    ofdm_decoder ofdm;

    unsigned char *subcarriers = ofdm_subcarriers_create(&opt->ofdmopt);
    ofdm.framesync = ofdmflexframesync_create(
        opt->ofdmopt.num_subcarriers, opt->ofdmopt.cyclic_prefix_len,
        opt->ofdmopt.taper_len, subcarriers, decoder_on_decode, d);
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

static void decoder_modem_create(const decoder_options *opt, decoder *d) {
    modem_decoder modem;

    modem.framesync = flexframesync_create(decoder_on_decode, d);
    flexframesync_set_header_len(modem.framesync, 0);
    if (opt->is_debug) {
        flexframesync_debug_enable(modem.framesync);
    }

    size_t symbolbuf_len = 256;
    d->symbolbuf = malloc(symbolbuf_len * sizeof(float complex));
    d->symbolbuf_len = symbolbuf_len;

    d->frame.modem = modem;
}

static void decoder_gmsk_create(const decoder_options *opt, decoder *d) {
    gmsk_decoder gmsk;

    gmsk.framesync = gmskframesync_create(decoder_on_decode, d);
    gmskframesync_set_header_len(gmsk.framesync, 0);

    if (opt->is_debug) {
        gmskframesync_debug_enable(gmsk.framesync);
    }

    size_t symbolbuf_len = 256;
    d->symbolbuf = malloc(symbolbuf_len * sizeof(float complex));
    d->symbolbuf_len = symbolbuf_len;

    d->frame.gmsk = gmsk;
}

decoder *quiet_decoder_create(const decoder_options *opt, float sample_rate) {
    if (!opt) {
        return NULL;
    }

    decoder *d = malloc(sizeof(decoder));

    d->opt = *opt;

    switch (d->opt.encoding) {
    case ofdm_encoding:
        decoder_ofdm_create(opt, d);
        break;
    case modem_encoding:
        decoder_modem_create(opt, d);
        break;
    case gmsk_encoding:
        decoder_gmsk_create(opt, d);
        break;
    }

    d->demod = demodulator_create(&(opt->demodopt));

    d->i = 0;
    d->resample_rate = 1;
    d->baserate = NULL;
    d->resampler = NULL;
    if (sample_rate != SAMPLE_RATE) {
        float rate =  (float)SAMPLE_RATE / (float)sample_rate;
        d->resampler = resamp_rrrf_create(rate, opt->resampler.delay,
                                          opt->resampler.bandwidth, opt->resampler.attenuation,
                                          opt->resampler.filter_bank_size);
        d->resample_rate = rate;
    }

    size_t stride_len = decoder_max_len(d);
    d->baserate = malloc(stride_len * sizeof(sample_t));
    d->baserate_offset = 0;

    d->checksum_fails = 0;

    d->buf = ring_create(decoder_default_buffer_len);
    d->writeframe_len = 0;
    d->writeframe = NULL;

    d->stats_enabled = false;
    for (size_t i = 0; i < num_frames_stats; i++) {
        d->stats_symbols[i] = NULL;
        d->stats_symbol_caps[i] = 0;
    }

    return d;
}

void quiet_decoder_enable_stats(quiet_decoder *d) {
    d->stats_enabled = true;

    for (size_t i = 0; i < num_frames_stats; i++) {
        d->stats_symbols[i] = NULL;
        d->stats_symbol_caps[i] = 0;
    }
    d->num_frames_collected = 0;
}

void quiet_decoder_disable_stats(quiet_decoder *d) {
    d->stats_enabled = false;

    for (size_t i = 0; i < num_frames_stats; i++) {
        if (d->stats_symbols[i]) {
            free(d->stats_symbols[i]);
            d->stats_symbols[i] = NULL;
            d->stats_symbol_caps[i] = 0;
        }
    }
    d->num_frames_collected = 0;
}

const quiet_decoder_frame_stats *quiet_decoder_consume_stats(quiet_decoder *d, size_t *num_frames) {
    *num_frames = d->num_frames_collected;

    return d->stats;
}

ssize_t quiet_decoder_recv(quiet_decoder *d, uint8_t *data, size_t len) {
    size_t framelen;
    if (ring_read(d->buf, (uint8_t*)(&framelen), sizeof(size_t)) < 0) {
        return -1;
    }

    // we will throw away part of the frame if len < framelen here
    // this mirrors the unix recv() spec
    len = (len > framelen) ? framelen : len;

    if (ring_read(d->buf, data, len) < 0) {
        assert(false && "ring buffer failed: frame not written atomically?");
        return -1;
    }

    ring_advance_reader(d->buf, framelen - len);

    return len;
}

static size_t decoder_max_len(decoder *d) {
    if (!d) {
        return 0;
    }

    return d->symbolbuf_len * d->demod->opt.samples_per_symbol;
}

void quiet_decoder_consume(decoder *d, const sample_t *samplebuf, size_t sample_len) {
    if (!d) {
        return;
    }

    size_t stride_len = decoder_max_len(d);

    if (d->stats_enabled) {
        d->num_frames_collected = 0;
    }

    for (size_t i = 0; i < sample_len; ) {
        size_t symbol_len, sample_chunk_len;
        if (d->resampler) {
            unsigned int resamp_read, resamp_write;
            resamp_rrrf_execute_output_block(d->resampler, samplebuf + i,
                                             sample_len - i, &resamp_read,
                                             d->baserate + d->baserate_offset,
                                             stride_len - d->baserate_offset,
                                             &resamp_write);
            i += resamp_read;
            sample_chunk_len = resamp_write + d->baserate_offset;
        } else {
            sample_chunk_len = stride_len;
            size_t remaining = sample_len - i + d->baserate_offset;
            if (remaining < sample_chunk_len) {
                sample_chunk_len = remaining;
            }

            // copy the next chunk of samplebuf into d->baserate, starting just after
            //   the last leftover samples
            memmove(d->baserate + d->baserate_offset, samplebuf + i,
                    (sample_chunk_len - d->baserate_offset) * sizeof(sample_t));
            i += sample_chunk_len - d->baserate_offset;
        }

        // now that we have our next chunk of samples picked out, reshape them to
        //    a multiple of samples_per_symbol
        size_t leftover = 0;
        if (sample_chunk_len % d->demod->opt.samples_per_symbol) {
            leftover = sample_chunk_len % d->demod->opt.samples_per_symbol;
            sample_chunk_len -= leftover;
        }

        symbol_len =
            demodulator_recv(d->demod, d->baserate, sample_chunk_len, d->symbolbuf);

        if (leftover) {
            memmove(d->baserate, d->baserate + sample_chunk_len, leftover * sizeof(sample_t));
        }
        d->baserate_offset = leftover;

        switch (d->opt.encoding) {
        case ofdm_encoding:
            ofdmflexframesync_execute(d->frame.ofdm.framesync, d->symbolbuf,
                                      symbol_len);

            if (d->opt.is_debug) {
                char fname[50];
                sprintf(fname, "framesync_%u.out", d->i);
                ofdmflexframesync_debug_print(d->frame.ofdm.framesync, fname);
                d->i++;
            }

            break;
        case modem_encoding:
            flexframesync_execute(d->frame.modem.framesync, d->symbolbuf,
                                  symbol_len);
            if (d->opt.is_debug) {
                char fname[50];
                sprintf(fname, "framesync_%u.out", d->i);
                flexframesync_debug_print(d->frame.modem.framesync, fname);
                d->i++;
            }

            break;
        case gmsk_encoding:
            gmskframesync_execute(d->frame.gmsk.framesync, d->symbolbuf,
                                  symbol_len);
            if (d->opt.is_debug) {
                char fname[50];
                sprintf(fname, "framesync_%u.out", d->i);
                gmskframesync_debug_print(d->frame.gmsk.framesync, fname);
                d->i++;
            }

            break;
        }
    }
}

void quiet_decoder_flush(decoder *d) {
    if (!d) {
        return;
    }

    size_t symbol_len = 0;

    if (d->resampler) {
        sample_t *flusher = calloc(d->opt.resampler.delay, sizeof(sample_t));
        size_t stride_len = decoder_max_len(d);
        unsigned int resamp_read, resamp_write;
        resamp_rrrf_execute_output_block(d->resampler, flusher,
                                         d->opt.resampler.delay, &resamp_read,
                                         d->baserate + d->baserate_offset,
                                         stride_len - d->baserate_offset,
                                         &resamp_write);
        resamp_write += d->baserate_offset;

        size_t leftover = 0;
        if (resamp_write % d->demod->opt.samples_per_symbol) {
            leftover = resamp_write % d->demod->opt.samples_per_symbol;
            resamp_write -= leftover;
        }

        symbol_len +=
            demodulator_recv(d->demod, d->baserate, resamp_write, d->symbolbuf + symbol_len);

        if (leftover) {
            memmove(d->baserate, d->baserate + resamp_write, leftover * sizeof(sample_t));
        }
        d->baserate_offset = leftover;
        free(flusher);
    }

    if (d->baserate_offset) {
        size_t baserate_flush_len = d->demod->opt.samples_per_symbol - d->baserate_offset;
        for (size_t i = 0; i < baserate_flush_len; i++) {
            d->baserate[i] = 0;
        }
        symbol_len += demodulator_recv(d->demod, d->baserate, d->demod->opt.samples_per_symbol, d->symbolbuf + symbol_len);
    }

    assert(demodulator_flush_symbol_len(d->demod) < d->symbolbuf_len);
    symbol_len += demodulator_flush(d->demod, d->symbolbuf + symbol_len);

    size_t framesync_flush_len;
    switch (d->opt.encoding) {
    case ofdm_encoding:
        ofdmflexframesync_execute(d->frame.ofdm.framesync, d->symbolbuf,
                                  symbol_len);
        break;
    case modem_encoding:
        // big heaping TODO -- figure out why we do this and what this number comes from
        // this has been empirically determined as necessary to get flexframesync to work
        // on very short payloads (< ~18 bytes).
        framesync_flush_len = 60;
        assert(symbol_len + framesync_flush_len < d->symbolbuf_len);
        for (size_t i = 0; i < framesync_flush_len; i++) {
            d->symbolbuf[symbol_len + i] = 0;
            symbol_len++;
        }
        flexframesync_execute(d->frame.modem.framesync, d->symbolbuf,
                              symbol_len);
        break;
    case gmsk_encoding:
        gmskframesync_execute(d->frame.gmsk.framesync, d->symbolbuf,
                              symbol_len);
        break;
    }
}

void quiet_decoder_destroy(decoder *d) {
    if (!d) {
        return;
    }

    switch (d->opt.encoding) {
    case ofdm_encoding:
        ofdmflexframesync_destroy(d->frame.ofdm.framesync);
        break;
    case modem_encoding:
        flexframesync_destroy(d->frame.modem.framesync);
        break;
    case gmsk_encoding:
        gmskframesync_destroy(d->frame.gmsk.framesync);
        break;
    }
    if (d->resampler) {
        resamp_rrrf_destroy(d->resampler);
    }
    if (d->baserate) {
        free(d->baserate);
    }
    for (size_t i = 0; i < num_frames_stats; i++) {
        if (d->stats_symbols[i]) {
            free(d->stats_symbols[i]);
        }
    }
    ring_destroy(d->buf);
    free(d->writeframe);
    demodulator_destroy(d->demod);
    free(d->symbolbuf);
    free(d);
}
