#include "quiet/encoder.h"

void encoder_ofdm_create(const encoder_options *opt, encoder *e) {
    ofdm_encoder ofdm;

    ofdmflexframegenprops_s props = {
        .check = opt->checksum_scheme,
        .fec0 = opt->inner_fec_scheme,
        .fec1 = opt->outer_fec_scheme,
        .mod_scheme = opt->mod_scheme,
    };

    unsigned char *subcarriers = ofdm_subcarriers_create(&opt->ofdmopt);
    ofdm.framegen = ofdmflexframegen_create(
        opt->ofdmopt.num_subcarriers, opt->ofdmopt.cyclic_prefix_len,
        opt->ofdmopt.taper_len, subcarriers, &props);
    ofdmflexframegen_set_header_len(ofdm.framegen, 0);
    if (opt->header_override_defaults) {
        ofdmflexframegenprops_s header_props = {
            .check = opt->header_checksum_scheme,
            .fec0 = opt->header_inner_fec_scheme,
            .fec1 = opt->header_outer_fec_scheme,
            .mod_scheme = opt->header_mod_scheme,
        };
        ofdmflexframegen_set_header_props(ofdm.framegen, &header_props);
    }

    size_t symbolbuf_len =
        opt->ofdmopt.num_subcarriers + opt->ofdmopt.cyclic_prefix_len;
    e->symbolbuf = malloc(symbolbuf_len *
                          sizeof(float complex));  // XXX check malloc result
    e->symbolbuf_len = symbolbuf_len;

    free(subcarriers);

    e->frame.ofdm = ofdm;
}

void encoder_modem_create(const encoder_options *opt, encoder *e) {
    modem_encoder modem;

    flexframegenprops_s props = {
        .check = opt->checksum_scheme,
        .fec0 = opt->inner_fec_scheme,
        .fec1 = opt->outer_fec_scheme,
        .mod_scheme = opt->mod_scheme,
    };

    modem.framegen = flexframegen_create(&props);
    flexframegen_set_header_len(modem.framegen, 0);
    if (opt->header_override_defaults) {
        flexframegenprops_s header_props = {
            .check = opt->header_checksum_scheme,
            .fec0 = opt->header_inner_fec_scheme,
            .fec1 = opt->header_outer_fec_scheme,
            .mod_scheme = opt->header_mod_scheme,
        };
        flexframegen_set_header_props(modem.framegen, &header_props);
    }
    e->symbolbuf = NULL;
    e->symbolbuf_len = 0;
    modem.symbols_remaining = 0;

    e->frame.modem = modem;
}

void encoder_gmsk_create(const encoder_options *opt, encoder *e) {
    gmsk_encoder gmsk;

    gmsk.framegen = gmskframegen_create();
    gmskframegen_set_header_len(gmsk.framegen, 0);

    // we should eventually try to get gmskframegen to tell us about this
    // tldr gmskframegen writes *always* happen in lengths of 2 samples
    gmsk.stride = 2;

    e->symbolbuf = NULL;
    e->symbolbuf_len = 0;

    e->frame.gmsk = gmsk;
}

encoder *quiet_encoder_create(const encoder_options *opt, float sample_rate) {
    if (opt->modopt.gain < 0 || opt->modopt.gain > 0.5) {
        quiet_set_last_error(quiet_encoder_bad_config);
        return NULL;
    }

    encoder *e = malloc(sizeof(encoder));

    e->opt = *opt;

    switch (e->opt.encoding) {
    case ofdm_encoding:
        encoder_ofdm_create(opt, e);
        break;
    case modem_encoding:
        encoder_modem_create(opt, e);
        break;
    case gmsk_encoding:
        encoder_gmsk_create(opt, e);
        break;
    }

    e->mod = modulator_create(&(opt->modopt));

    size_t emit_len = modulator_sample_len(e->mod, e->symbolbuf_len);
    size_t flush_len = modulator_flush_sample_len(e->mod);
    e->samplebuf_cap = (emit_len > flush_len) ? emit_len : flush_len;
    e->samplebuf = malloc(e->samplebuf_cap * sizeof(sample_t));
    e->samplebuf_len = 0;
    e->samplebuf_offset = 0;

    e->payload = NULL;
    e->payload_length = 0;
    e->has_flushed = true;
    e->is_queue_closed = false;

    e->is_close_frame = false;

    e->resample_rate = 1;
    e->resampler = NULL;

    if (sample_rate != SAMPLE_RATE) {
        float rate = (float)sample_rate / (float)SAMPLE_RATE;
        e->resampler = resamp_rrrf_create(rate, opt->resampler.delay,
                                          opt->resampler.bandwidth, opt->resampler.attenuation,
                                          opt->resampler.filter_bank_size);
        e->resample_rate = rate;
    }

    e->buf = ring_create(encoder_default_buffer_len);
    e->tempframe = malloc(sizeof(size_t) + e->opt.frame_len);
    e->readframe = malloc(e->opt.frame_len);

    return e;
}

size_t quiet_encoder_get_frame_len(const encoder *e) {
    return e->opt.frame_len;
}

size_t quiet_encoder_clamp_frame_len(encoder *e, size_t sample_len) {
    e->is_close_frame = true;

    // get sample_len in base rate (conservative estimate)
    // assume we can also get ceil(resample_rate) samples out plus "linear" count
    size_t baserate_sample_len = ceilf((float)sample_len/e->resample_rate) + ceilf(e->resample_rate);

    // subtract headroom for flushing mod & resamp
    baserate_sample_len -= modulator_flush_sample_len(e->mod);
    if (e->resampler) {
        baserate_sample_len -= e->opt.resampler.delay;
    }

    // first, let's see if the current length will still work
    size_t projected_sample_len = quiet_encoder_sample_len(e, e->opt.frame_len);
    if (projected_sample_len <= baserate_sample_len) {
        return e->opt.frame_len;
    }

    // we need to reduce frame_len

    // do inverse calc from base rate sample len to frame length
    // this has to be iterative as we don't have inverse func for this
    // we'll start with the suggested length and then do binary search
    // in this search, max_frame_len is known to be too long,
    //      min_frame_len is known to be short enough to fit in sample_len
    size_t max_frame_len = e->opt.frame_len;
    size_t min_frame_len = 0;
    size_t frame_len = max_frame_len / 2;
    while (max_frame_len - min_frame_len > 1) {
        size_t projected_sample_len = quiet_encoder_sample_len(e, frame_len);
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
    return frame_len;
}

void quiet_encoder_set_blocking(quiet_encoder *e, time_t sec, long nano) {
    ring_writer_lock(e->buf);
    ring_set_writer_blocking(e->buf, sec, nano);
    ring_writer_unlock(e->buf);
}

void quiet_encoder_set_nonblocking(quiet_encoder *e) {
    ring_writer_lock(e->buf);
    ring_set_writer_nonblocking(e->buf);
    ring_writer_unlock(e->buf);
}

static int encoder_is_assembled(encoder *e) {
    switch (e->opt.encoding) {
    case ofdm_encoding:
        return ofdmflexframegen_is_assembled(e->frame.ofdm.framegen);
    case modem_encoding:
        return flexframegen_is_assembled(e->frame.modem.framegen);
    case gmsk_encoding:
        return gmskframegen_is_assembled(e->frame.gmsk.framegen);
    }
}

ssize_t quiet_encoder_send(quiet_encoder *e, const void *buf, size_t len) {
    if (len > e->opt.frame_len) {
        quiet_set_last_error(quiet_msg_size);
        return -1;
    }

    // it's painful to do this copy which could then fail, but we need to write atomically
    // TODO peek, decide if we have room, then abort if not
    memcpy(e->tempframe, &len, sizeof(size_t));
    memcpy(e->tempframe + (sizeof(size_t)), buf, len);

    ring_writer_lock(e->buf);
    ssize_t written = ring_write(e->buf, e->tempframe, sizeof(size_t) + len);
    ring_writer_unlock(e->buf);
    if (written == 0) {
        return 0;
    }
    if (written < 0) {
        switch (written) {
            case RingErrorWouldBlock:
                quiet_set_last_error(quiet_would_block);
                break;
            case RingErrorTimedout:
                quiet_set_last_error(quiet_timedout);
                break;
            default:
                quiet_set_last_error(quiet_io);
        }
        return -1;
    }
    return written - sizeof(size_t);
}

void quiet_encoder_close(quiet_encoder *e) {
    ring_writer_lock(e->buf);
    ring_close(e->buf);
    ring_writer_unlock(e->buf);
}

static bool encoder_read_next_frame(encoder *e) {
    if (e->is_queue_closed) {
        return false;
    }
    size_t framelen;
    ring_reader_lock(e->buf);
    ssize_t nread = ring_read(e->buf, (uint8_t*)(&framelen), sizeof(size_t));
    if (nread <= 0) {
        ring_reader_unlock(e->buf);
        if (nread == 0) {
            e->is_queue_closed = true;
        }
        return false;
    }
    ssize_t read = ring_read(e->buf, e->readframe, framelen);
    ring_reader_unlock(e->buf);
    if (read <= 0) {
        assert(false && "ring buffer failed: frame not written atomically?");
    }

    uint8_t header[1];
    switch (e->opt.encoding) {
    case ofdm_encoding:
        ofdmflexframegen_assemble(e->frame.ofdm.framegen, header, e->readframe,
                                  framelen);
        break;
    case modem_encoding:
        flexframegen_assemble(e->frame.modem.framegen, header, e->readframe,
                              framelen);
        e->frame.modem.symbols_remaining =
            flexframegen_getframelen(e->frame.modem.framegen);
        break;
    case gmsk_encoding:
        gmskframegen_reset(e->frame.gmsk.framegen);
        gmskframegen_assemble(e->frame.gmsk.framegen, header, e->readframe,
                              framelen, (crc_scheme)e->opt.checksum_scheme,
                              (fec_scheme)e->opt.inner_fec_scheme, (fec_scheme)e->opt.outer_fec_scheme);
        break;
    }

    e->has_flushed = false;
    return true;
}

static size_t quiet_encoder_sample_len(encoder *e, size_t data_len) {
    uint8_t *empty = calloc(data_len, sizeof(uint8_t));
    uint8_t header[1];
    size_t num_symbols;
    switch (e->opt.encoding) {
    case ofdm_encoding:
        ofdmflexframegen_assemble(e->frame.ofdm.framegen, header, empty,
                                  data_len);  // TODO actual calculation?
        size_t num_ofdm_blocks =
            ofdmflexframegen_getframelen(e->frame.ofdm.framegen);
        num_symbols = num_ofdm_blocks * e->symbolbuf_len;
        ofdmflexframegen_reset(e->frame.ofdm.framegen);
        break;
    case modem_encoding:
        flexframegen_assemble(e->frame.modem.framegen, header, empty, data_len);
        num_symbols = flexframegen_getframelen(e->frame.modem.framegen);
        flexframegen_reset(e->frame.modem.framegen);
        break;
    case gmsk_encoding:
        gmskframegen_assemble(e->frame.gmsk.framegen, header, empty, data_len,
                              (crc_scheme)e->opt.checksum_scheme, (fec_scheme)e->opt.inner_fec_scheme,
                              (fec_scheme)e->opt.outer_fec_scheme);
        num_symbols = gmskframegen_getframelen(e->frame.gmsk.framegen);
        gmskframegen_reset(e->frame.gmsk.framegen);
        break;
    }
    free(empty);
    return modulator_sample_len(e->mod, num_symbols);
}

static size_t encoder_fillsymbols(encoder *e, size_t requested_length) {
    size_t ofdmwritelen;
    switch (e->opt.encoding) {
    case ofdm_encoding:
        // ofdm can't control the size of its blocks, so it ignores
        // requested_length
        // XXX ofdm now can, update this to take advantage of that
        ofdmwritelen = e->opt.ofdmopt.num_subcarriers + e->opt.ofdmopt.cyclic_prefix_len;
        ofdmflexframegen_write(e->frame.ofdm.framegen, e->symbolbuf, ofdmwritelen);
        return ofdmwritelen;
    case modem_encoding:
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
    case gmsk_encoding:
        if (requested_length % e->frame.gmsk.stride) {
            requested_length += (e->frame.gmsk.stride - (requested_length % e->frame.gmsk.stride));
        }

        if (requested_length > e->symbolbuf_len) {
            e->symbolbuf =
                realloc(e->symbolbuf,
                        requested_length *
                            sizeof(float complex));  // XXX check malloc result
            e->symbolbuf_len = requested_length;
        }

        size_t i;
        for (i = 0; i < requested_length; i += e->frame.gmsk.stride) {
            int finished = gmskframegen_write_samples(e->frame.gmsk.framegen, e->symbolbuf + i);
            if (finished) {
                break;
            }
        }
        return i;
    }
}

ssize_t quiet_encoder_emit(encoder *e, sample_t *samplebuf, size_t samplebuf_len) {
    if (!e) {
        return 0;
    }

    ssize_t written = 0;
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
                    constrained_write(e->samplebuf + e->samplebuf_offset,
                                      e->samplebuf_len, samplebuf, remaining);
                samplebuf += iter_written;
                written += iter_written;
                e->samplebuf_offset += iter_written;
                e->samplebuf_len -= iter_written;
            }
            continue;
        }

        e->samplebuf_offset = 0;

        if (frame_closed) {
            break;
        }

        if (!(encoder_is_assembled(e))) {
            // if we are in close-frame mode, and we've already written this time, then
            //    close out the buffer
            // also close it out if we are out of frames to write
            bool do_close_frame = e->is_close_frame && written > 0;
            bool have_another_frame = encoder_read_next_frame(e);

            if (do_close_frame || !have_another_frame) {
                if (e->has_flushed) {
                    break;
                }
                e->samplebuf_len = modulator_flush(e->mod, e->samplebuf);
                if (e->resampler) {
                    for (size_t i = 0; i < e->opt.resampler.delay; i++) {
                        e->samplebuf[i + e->samplebuf_len] = 0;
                    }
                    e->samplebuf_len += e->opt.resampler.delay;
                }
                modulator_reset(e->mod);
                e->has_flushed = true;
                if (do_close_frame && have_another_frame) {
                    // set this flag here so that we don't re-attempt the next frame check
                    // this is hacky and this logic needs cleanup
                    frame_closed = true;
                }
                continue;
            }
        }

        // now we get to the steady state, writing one block of symbols at a
        // time
        // we provide symbols_wanted as a hint of how many symbols to get

        // we're going to write into our baserate samples buffer and then restart loop
        // once we do, we'll resample (if desired) and write to output buffer

        size_t baserate_samples_wanted = (size_t)(ceilf(remaining / e->resample_rate));
        size_t symbols_wanted = modulator_symbol_len(e->mod, baserate_samples_wanted);
        if (baserate_samples_wanted % e->mod->opt.samples_per_symbol) {
            symbols_wanted++;
        }
        size_t symbols_written = encoder_fillsymbols(e, symbols_wanted);
        size_t sample_buffer_needed = modulator_sample_len(e->mod, symbols_written);

        if (sample_buffer_needed > e->samplebuf_cap) {
            e->samplebuf =
                realloc(e->samplebuf,
                        sample_buffer_needed *
                            sizeof(sample_t));  // XXX check malloc result
            e->samplebuf_cap = sample_buffer_needed;
        }

        e->samplebuf_len =
            modulator_emit(e->mod, e->symbolbuf, symbols_written, e->samplebuf);
        e->has_flushed = false;
    }

    if (frame_closed) {
        size_t zerolen = samplebuf_len - written;
        for (size_t i = 0; i < zerolen; ++i) {
            samplebuf[i] = 0;
            written++;
        }
    }

    if (written == 0 && !e->is_queue_closed) {
        // we only return 0 if the queue is closed
        // here we remap to -1
        quiet_set_last_error(quiet_would_block);
        written = -1;
    }

    return written;
}

void quiet_encoder_destroy(encoder *e) {
    if (!e) {
        return;
    }

    switch (e->opt.encoding) {
    case ofdm_encoding:
        ofdmflexframegen_destroy(e->frame.ofdm.framegen);
        break;
    case modem_encoding:
        flexframegen_destroy(e->frame.modem.framegen);
        break;
    case gmsk_encoding:
        gmskframegen_destroy(e->frame.gmsk.framegen);
        break;
    }
    if (e->resampler) {
        resamp_rrrf_destroy(e->resampler);
    }
    modulator_destroy(e->mod);
    free(e->symbolbuf);
    free(e->samplebuf);
    ring_destroy(e->buf);
    free(e->tempframe);
    free(e->readframe);
    free(e);
}
