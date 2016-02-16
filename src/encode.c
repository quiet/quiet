#include "quiet/common.h"
#include "quiet/modulator.h"

void encoder_opt_set_sample_rate(encoder_options *opt, float sample_rate) {
    opt->sample_rate = sample_rate;
}

void create_ofdm_encoder(const encoder_options *opt, encoder *e) {
    ofdm_encoder ofdm;

    ofdmflexframegenprops_s props = {
        .check = opt->checksum_scheme,
        .fec0 = opt->inner_fec_scheme,
        .fec1 = opt->outer_fec_scheme,
        .mod_scheme = opt->mod_scheme,
    };

    unsigned char *subcarriers = create_ofdm_subcarriers(&opt->ofdmopt);
    ofdm.framegen = ofdmflexframegen_create(
        opt->ofdmopt.num_subcarriers, opt->ofdmopt.cyclic_prefix_len,
        opt->ofdmopt.taper_len, subcarriers, &props);
    ofdmflexframegen_set_header_len(ofdm.framegen, 0);

    size_t symbolbuf_len =
        opt->ofdmopt.num_subcarriers + opt->ofdmopt.cyclic_prefix_len;
    e->symbolbuf = malloc(symbolbuf_len *
                          sizeof(float complex));  // XXX check malloc result
    e->symbolbuf_len = symbolbuf_len;

    free(subcarriers);

    e->frame.ofdm = ofdm;
}

void create_modem_encoder(const encoder_options *opt, encoder *e) {
    modem_encoder modem;

    flexframegenprops_s props = {
        .check = opt->checksum_scheme,
        .fec0 = opt->inner_fec_scheme,
        .fec1 = opt->outer_fec_scheme,
        .mod_scheme = opt->mod_scheme,
    };

    modem.framegen = flexframegen_create(&props);
    flexframegen_set_header_len(modem.framegen, 0);

    e->symbolbuf = NULL;
    e->symbolbuf_len = 0;
    modem.symbols_remaining = 0;

    e->frame.modem = modem;
}

encoder *create_encoder(const encoder_options *opt) {
    if (!opt) {
        return NULL;
    }

    encoder *e = malloc(sizeof(encoder));

    e->opt = *opt;

    printf("%d %d\n", opt->checksum_scheme, opt->mod_scheme);
    printf("%f %f\n", opt->modopt.excess_bw, opt->modopt.center_rads);

    if (opt->is_ofdm) {
        create_ofdm_encoder(opt, e);
    } else {
        create_modem_encoder(opt, e);
    }

    e->mod = create_modulator(&(opt->modopt));

    e->samplebuf_cap = modulate_sample_len(e->mod, e->symbolbuf_len);
    e->samplebuf = malloc(e->samplebuf_cap * sizeof(sample_t));
    e->samplebuf_len = 0;
    e->samplebuf_offset = 0;

    e->payload = NULL;
    e->payload_length = 0;
    e->has_flushed = true;

    e->noise_prefix_remaining = opt->noise_prefix;

    e->resample_rate = 1;
    if (opt->sample_rate != SAMPLE_RATE) {
        float rate = (float)opt->sample_rate / (float)SAMPLE_RATE;
        e->resampler = resamp_rrrf_create(rate, opt->resampler.delay,
                                          opt->resampler.bandwidth, opt->resampler.attenuation,
                                          opt->resampler.filter_bank_size);
        e->resample_rate = rate;
    }


    return e;
}

void encoder_clamp_frame_len(encoder *e, size_t sample_len) {
    if (!e->opt.is_close_frame) {
        return;
    }

    // get sample_len in base rate (conservative estimate)
    // assume we can also get ceil(resample_rate) samples out plus "linear" count
    size_t baserate_sample_len = ceilf((float)sample_len/e->resample_rate) + ceilf(e->resample_rate);

    // subtract headroom for flushing mod & resamp
    baserate_sample_len -= modulate_flush_sample_len(e->mod);
    baserate_sample_len -= e->opt.resampler.delay;

    // do inverse calc from base rate sample len to frame length
    // this has to be iterative as we don't have inverse func for this
    // we'll start with the suggested length and then do binary search
    size_t max_frame_len = e->opt.frame_len;
    size_t min_frame_len = 0;
    size_t frame_len = max_frame_len / 2;
    while (max_frame_len - min_frame_len > 1) {
        size_t projected_sample_len = encoder_sample_len(e, frame_len);
        if (projected_sample_len > baserate_sample_len) {
            // this frame size was too big
            max_frame_len = frame_len;
        } else {
            // this frame size fit but maybe we can fit more
            min_frame_len = frame_len;
        }
        frame_len = (max_frame_len - min_frame_len) / 2 + min_frame_len;
    }
    e->opt.frame_len = frame_len;
    printf("new frame len %zu\n", e->opt.frame_len);
}

int _encoder_assembled(encoder *e) {
    if (e->opt.is_ofdm) {
        return ofdmflexframegen_is_assembled(e->frame.ofdm.framegen);
    } else {
        return flexframegen_is_assembled(e->frame.modem.framegen);
    }
}

int encoder_set_payload(encoder *e, const uint8_t *payload, size_t payload_length) {
    int had_payload = (e->payload_length != 0) || (_encoder_assembled(e)) ||
                      (e->samplebuf_len != 0);

    e->payload = payload;
    e->payload_length = payload_length;
    e->samplebuf_len = 0;
    e->samplebuf_offset = 0;
    e->has_flushed = false;

    modulate_reset(e->mod);

    if (e->opt.is_ofdm) {
        ofdmflexframegen_reset(e->frame.ofdm.framegen);
    } else {
        flexframegen_reset(e->frame.modem.framegen);
        e->frame.modem.symbols_remaining = 0;
    }

    return had_payload;
}

void _encoder_consume(encoder *e) {
    size_t payload_length = (e->opt.frame_len < e->payload_length) ? e->opt.frame_len : e->payload_length;
    const uint8_t *payload = e->payload;
    e->payload += payload_length;
    e->payload_length -= payload_length;
    if (e->opt.is_ofdm) {
        uint8_t *header = calloc(sizeof(uint8_t), 1);
        printf("first bytes: %d %d %d %d %d\n", payload[0], payload[1], payload[2], payload[3], payload[4]);
        ofdmflexframegen_assemble(e->frame.ofdm.framegen, header, payload,
                                  payload_length);
        free(header);
    } else {
        uint8_t *header = calloc(sizeof(uint8_t), 1);
        flexframegen_assemble(e->frame.modem.framegen, header, payload,
                              payload_length);
        e->frame.modem.symbols_remaining =
            flexframegen_getframelen(e->frame.modem.framegen);
        free(header);
    }
}

size_t encoder_sample_len(encoder *e, size_t data_len) {
    uint8_t *empty = calloc(data_len, sizeof(uint8_t));
    uint8_t header[1];
    if (e->opt.is_ofdm) {
        ofdmflexframegen_assemble(e->frame.ofdm.framegen, header, empty,
                                  data_len);  // TODO actual calculation?
        size_t num_ofdm_blocks =
            ofdmflexframegen_getframelen(e->frame.ofdm.framegen);
        return modulate_sample_len(e->mod, num_ofdm_blocks * e->symbolbuf_len);
    } else {
        flexframegen_assemble(e->frame.modem.framegen, header, empty, data_len);
        size_t num_symbols = flexframegen_getframelen(e->frame.modem.framegen);
        return modulate_sample_len(e->mod, num_symbols);
    }
}

size_t _constrained_write(sample_t *src, size_t src_len, sample_t *dst,
                          size_t dest_len) {
    size_t len = src_len;
    if (dest_len < src_len) {
        len = dest_len;
    }

    memmove(dst, src, len * sizeof(sample_t));

    return len;
}

size_t _encoder_write_noise(encoder *e) {
    for (size_t i = 0; i < e->noise_prefix_remaining; i++) {
        e->symbolbuf[i] = randnf() + _Complex_I * randnf() * M_SQRT1_2;
    }
    return e->noise_prefix_remaining;
}

size_t _encoder_pad(encoder *e) {
    size_t padding_len;
    if (e->opt.is_ofdm) {
        padding_len =
            modulate_sample_len(e->mod, e->opt.ofdmopt.cyclic_prefix_len);
    } else {
        padding_len = 32;
    }
    if (padding_len > e->samplebuf_cap) {
        e->samplebuf =
            realloc(e->samplebuf,
                    padding_len * sizeof(sample_t));  // XXX check malloc result
        e->samplebuf_cap = padding_len;
    }
    for (size_t i = 0; i < padding_len; i++) {
        e->samplebuf[i] = 0;
    }
    return padding_len;
}

size_t _encoder_fillsymbols(encoder *e, size_t requested_length) {
    if (e->noise_prefix_remaining > 0) {
        size_t noise_wanted = e->noise_prefix_remaining;
        if (noise_wanted > e->symbolbuf_len) {
            e->symbolbuf =
                realloc(e->symbolbuf,
                        noise_wanted *
                            sizeof(float complex));  // XXX check malloc result
            e->symbolbuf_len = noise_wanted;
        }
        size_t written = _encoder_write_noise(e);
        e->noise_prefix_remaining = 0;
        return written;
    }

    if (e->opt.is_ofdm) {
        // ofdm can't control the size of its blocks, so it ignores
        // requested_length
        ofdmflexframegen_writesymbol(e->frame.ofdm.framegen, e->symbolbuf);
        return e->opt.ofdmopt.num_subcarriers + e->opt.ofdmopt.cyclic_prefix_len;
    } else {
        if (requested_length > e->frame.modem.symbols_remaining) {
            requested_length = e->frame.modem.symbols_remaining;
        }

        if (requested_length > e->symbolbuf_len) {
            e->symbolbuf =
                realloc(e->symbolbuf,
                        requested_length *
                            sizeof(float complex));  // XXX check malloc result
            e->symbolbuf_len = requested_length;
        }

        flexframegen_write_samples(e->frame.modem.framegen, e->symbolbuf,
                                   requested_length);
        e->frame.modem.symbols_remaining -= requested_length;
        return requested_length;
    }
}

size_t encode(encoder *e, sample_t *samplebuf, size_t samplebuf_len) {
    if (!e) {
        return 0;
    }

    size_t written = 0;
    bool frame_closed = false;
    while (written < samplebuf_len) {
        size_t remaining = samplebuf_len - written;
        size_t iter_written;

        if (e->samplebuf_len > 0) {
            if (e->resampler) {
                unsigned int samples_read, samples_written;
                resamp_rrrf_execute_output_block(e->resampler, e->samplebuf + e->samplebuf_offset,
                                                 e->samplebuf_len, &samples_read,
                                                 samplebuf, remaining,
                                                 &samples_written);
                samplebuf += samples_written;
                written += samples_written;
                e->samplebuf_offset += samples_read;
                e->samplebuf_len -= samples_read;
            } else {
                iter_written =
                    _constrained_write(e->samplebuf + e->samplebuf_offset,
                                       e->samplebuf_len, samplebuf, remaining);
                samplebuf += iter_written;
                written += iter_written;
                e->samplebuf_offset += iter_written;
                e->samplebuf_len -= iter_written;
            }
            continue;
        }

        e->samplebuf_offset = 0;

        if (!(_encoder_assembled(e))) {
            // if we are in close-frame mode, and we've already written this time, then
            //    close out the buffer
            // also close it out if payload is emptied out
            bool do_close_frame = e->opt.is_close_frame && written > 0;
            if (e->payload_length == 0 || do_close_frame) {
                if (e->has_flushed) {
                    if (do_close_frame) {
                        frame_closed = true;
                    }
                    break;
                }
                e->samplebuf_len = modulate_flush(e->mod, e->samplebuf);
                if (e->resampler) {
                    for (size_t i = 0; i < e->opt.resampler.delay; i++) {
                        e->samplebuf[i + e->samplebuf_len] = 0;
                    }
                    e->samplebuf_len += e->opt.resampler.delay;
                }
                // XXX reset modulator or resampler?
                e->has_flushed = true;
                continue;
            } else {
                // e->samplebuf_len = _encoder_pad(e);
            }
            _encoder_consume(e);
            continue;
        }

        // now we get to the steady state, writing one block of symbols at a
        // time
        // we provide symbols_wanted as a hint of how many symbols to get

        // we're going to write into our baserate samples buffer and then restart loop
        // once we do, we'll resample (if desired) and write to output buffer

        size_t baserate_samples_wanted = (size_t)(ceilf(remaining / e->resample_rate));
        size_t symbols_wanted = modulate_symbol_len(e->mod, baserate_samples_wanted);
        if (baserate_samples_wanted % e->mod->opt.samples_per_symbol) {
            symbols_wanted++;
        }
        size_t symbols_written = _encoder_fillsymbols(e, symbols_wanted);
        size_t sample_buffer_needed = modulate_sample_len(e->mod, symbols_written);

        if (sample_buffer_needed > e->samplebuf_cap) {
            e->samplebuf =
                realloc(e->samplebuf,
                        sample_buffer_needed *
                            sizeof(sample_t));  // XXX check malloc result
            e->samplebuf_cap = sample_buffer_needed;
        }

        e->samplebuf_len =
            modulate(e->mod, e->symbolbuf, symbols_written, e->samplebuf);
        e->has_flushed = false;
    }

    if (frame_closed) {
        for (size_t i = written; i < samplebuf_len; ++i) {
            samplebuf[i] = 0;
        }

        return samplebuf_len;
    }

    return written;
}

void destroy_encoder(encoder *e) {
    if (!e) {
        return;
    }

    if (e->opt.is_ofdm) {
        ofdmflexframegen_destroy(e->frame.ofdm.framegen);
    } else {
        flexframegen_destroy(e->frame.modem.framegen);
    }
    if (e->resampler) {
        resamp_rrrf_destroy(e->resampler);
    }
    destroy_modulator(e->mod);
    free(e->symbolbuf);
    free(e->samplebuf);
    free(e);
}
