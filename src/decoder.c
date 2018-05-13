#include "quiet/decoder.h"

unsigned int quiet_decoder_checksum_fails(const quiet_decoder *d) {
    return d->checksum_fails;
}

static void decoder_collect_stats(decoder *d, framesyncstats_s stats, int payload_valid) {
    size_t stats_index = d->num_frames_collected;
    if (stats_index < num_frames_stats) {
        quiet_complex *sym = d->stats_symbols[stats_index];
        size_t sym_cap = d->stats_symbol_caps[stats_index];

        if (sym_cap < stats.num_framesyms) {
            d->stats_symbols[stats_index] = realloc(sym, stats.num_framesyms * sizeof(quiet_complex));
            d->stats_symbol_caps[stats_index] = stats.num_framesyms;
        }

        for (size_t i = 0; i < stats.num_framesyms; i++) {
            d->stats_symbols[stats_index][i].real = crealf(stats.framesyms[i]);
            d->stats_symbols[stats_index][i].imaginary = cimagf(stats.framesyms[i]);
        }

        quiet_decoder_frame_stats *fstats = d->stats + stats_index;

        fstats->symbols = d->stats_symbols[stats_index];
        fstats->num_symbols = stats.num_framesyms;
        fstats->error_vector_magnitude = stats.evm;
        fstats->received_signal_strength_indicator = stats.rssi;
        fstats->checksum_passed = payload_valid;

        d->num_frames_collected++;
    }
    // length-prefixed
    size_t write_len = sizeof(size_t);

    // length-prefixed symbol section
    // (this is slightly redundant but easier to think about)
    write_len += sizeof(size_t);

    // write all the symbols
    write_len += stats.num_framesyms * sizeof(complex float);

    // rssi and evm
    write_len += 2 * sizeof(float);

    // is payload valid?
    write_len += sizeof(int);

    ring_writer_lock(d->stats_ring);

    // reserve the space we need
    ssize_t reserved = ring_write_partial_init(d->stats_ring, write_len);

    if (reserved != write_len) {
        // bail
        return;
    }

    // total length of what's to come
    size_t prefix = write_len - sizeof(size_t);

    ssize_t written = ring_write_partial(d->stats_ring, &prefix, sizeof(size_t));

    if (written != sizeof(size_t)) {
        assert(false && "partial write failed");
    }

    // cast this to a size_t to be consistent
    // (liquid uses an unsigned int)
    size_t num_framesyms = stats.num_framesyms;

    written = ring_write_partial(d->stats_ring, &num_framesyms, sizeof(size_t));

    if (written != sizeof(size_t)) {
        assert(false && "partial write failed");
    }

    written = ring_write_partial(d->stats_ring, stats.framesyms, stats.num_framesyms * sizeof(complex float));

    if (written != (stats.num_framesyms * sizeof(complex float))) {
        assert(false && "partial write failed");
    }

    written = ring_write_partial(d->stats_ring, &stats.rssi, sizeof(float));

    if (written != sizeof(float)) {
        assert(false && "partial write failed");
    }

    written = ring_write_partial(d->stats_ring, &stats.evm, sizeof(float));

    if (written != sizeof(float)) {
        assert(false && "partial write failed");
    }

    written = ring_write_partial(d->stats_ring, &payload_valid, sizeof(int));

    if (written != sizeof(int)) {
        assert(false && "partial write failed");
    }

    ssize_t commit_ret = ring_write_partial_commit(d->stats_ring);

    if (commit_ret) {
        assert(false && "partial write commit failed");
    }

    ring_writer_unlock(d->stats_ring);
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
        decoder_collect_stats(d, stats, payload_valid);
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

    ring_writer_lock(d->buf);
    ring_write(d->buf, d->writeframe, framelen);
    ring_writer_unlock(d->buf);
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
    ofdmflexframesync_decode_header_soft(ofdm.framesync, 1);
    ofdmflexframesync_decode_payload_soft(ofdm.framesync, 1);
    if (opt->header_override_defaults) {
        ofdmflexframegenprops_s header_props = {
            .check = opt->header_checksum_scheme,
            .fec0 = opt->header_inner_fec_scheme,
            .fec1 = opt->header_outer_fec_scheme,
            .mod_scheme = opt->header_mod_scheme,
        };
        ofdmflexframesync_set_header_props(ofdm.framesync, &header_props);
    }

    size_t symbolbuf_len =
        opt->ofdmopt.num_subcarriers + opt->ofdmopt.cyclic_prefix_len;
    d->symbolbuf = malloc(symbolbuf_len *
                          sizeof(float complex));
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
    flexframesync_decode_header_soft(modem.framesync, 1);
    flexframesync_decode_payload_soft(modem.framesync, 1);
    if (opt->header_override_defaults) {
        flexframegenprops_s header_props = {
            .check = opt->header_checksum_scheme,
            .fec0 = opt->header_inner_fec_scheme,
            .fec1 = opt->header_outer_fec_scheme,
            .mod_scheme = opt->header_mod_scheme,
        };
        flexframesync_set_header_props(modem.framesync, &header_props);
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
    d->stats_ring = NULL;
    d->stats_packed = NULL;
    d->stats_unpacked = NULL;

    return d;
}

void quiet_decoder_set_blocking(quiet_decoder *d, time_t sec, long nano) {
    ring_reader_lock(d->buf);
    ring_set_reader_blocking(d->buf, sec, nano);
    ring_reader_unlock(d->buf);
}

void quiet_decoder_set_nonblocking(quiet_decoder *d) {
    ring_reader_lock(d->buf);
    ring_set_reader_nonblocking(d->buf);
    ring_reader_unlock(d->buf);
}

void quiet_decoder_set_stats_blocking(quiet_decoder *d, time_t sec, long nano) {
    if (d->stats_ring) {
        ring_reader_lock(d->stats_ring);
        ring_set_reader_blocking(d->stats_ring, sec, nano);
        ring_reader_unlock(d->stats_ring);
    }
}

void quiet_decoder_set_stats_nonblocking(quiet_decoder *d) {
    if (d->stats_ring) {
        ring_reader_lock(d->stats_ring);
        ring_set_reader_nonblocking(d->stats_ring);
        ring_reader_unlock(d->stats_ring);
    }
}

void quiet_decoder_enable_stats(quiet_decoder *d) {
    d->stats_enabled = true;

    for (size_t i = 0; i < num_frames_stats; i++) {
        d->stats_symbols[i] = NULL;
        d->stats_symbol_caps[i] = 0;
    }
    d->num_frames_collected = 0;

    d->stats_ring = ring_create(decoder_default_stats_buffer_len);
    d->stats_packed = NULL;
    d->stats_packed_len = 0;
    d->stats_unpacked = malloc(sizeof(quiet_decoder_frame_stats));
    d->stats_unpacked->symbols = NULL;
    d->stats_unpacked_symbols_cap = 0;
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

    if (d->stats_ring) {
        ring_destroy(d->stats_ring);
        d->stats_ring = NULL;
    }

    if (d->stats_packed) {
        free(d->stats_packed);
        d->stats_packed = NULL;
        d->stats_packed_len = 0;
    }

    if (d->stats_unpacked) {
        if (d->stats_unpacked->symbols) {
            free((quiet_complex *)d->stats_unpacked->symbols);
        }
        free(d->stats_unpacked);
        d->stats_unpacked = NULL;
        d->stats_unpacked_symbols_cap = 0;
    }
}

const quiet_decoder_frame_stats *quiet_decoder_consume_stats(quiet_decoder *d, size_t *num_frames) {
    *num_frames = d->num_frames_collected;

    return d->stats;
}

const quiet_decoder_frame_stats *quiet_decoder_recv_stats(quiet_decoder *d) {
    if (!d->stats_ring) {
        return NULL;
    }

    ring_reader_lock(d->stats_ring);

    size_t statslen;
    ssize_t statslen_written = ring_read(d->stats_ring, (uint8_t*)(&statslen), sizeof(size_t));

    if (statslen_written == 0) {
        quiet_set_last_error(quiet_success);
        return NULL;
    }

    if (statslen_written < 0) {
        ring_reader_unlock(d->stats_ring);
        switch (statslen_written) {
            case RingErrorWouldBlock:
                quiet_set_last_error(quiet_would_block);
                break;
            case RingErrorTimedout:
                quiet_set_last_error(quiet_timedout);
                break;
            default:
                quiet_set_last_error(quiet_io);
        }
        return NULL;
    }

    if (statslen > d->stats_packed_len) {
        d->stats_packed = realloc(d->stats_packed, statslen);
        d->stats_packed_len = statslen;
    }

    if (ring_read(d->stats_ring, d->stats_packed, statslen) < 0) {
        ring_reader_unlock(d->stats_ring);
        assert(false && "packed stats read failed");
        quiet_set_last_error(quiet_io);
        return NULL;
    }

    // we keep the ring locked because we're also using it
    // to sync on writing this stats object

    uint8_t *packed_iter = d->stats_packed;

    memcpy(&d->stats_unpacked->num_symbols, packed_iter, sizeof(size_t));
    packed_iter += sizeof(size_t);

    size_t symbols_write_size = d->stats_unpacked->num_symbols * sizeof(quiet_complex);

    if (symbols_write_size > d->stats_unpacked_symbols_cap) {
        d->stats_unpacked->symbols = realloc((quiet_complex*)d->stats_unpacked->symbols, symbols_write_size);
        d->stats_unpacked_symbols_cap = symbols_write_size;
    }

    complex float temp;
    quiet_complex *symbols = (quiet_complex *)d->stats_unpacked->symbols;
    for (size_t i = 0; i < d->stats_unpacked->num_symbols; i++) {
        memcpy(&temp, packed_iter, sizeof(complex float));
        packed_iter += sizeof(complex float);
        symbols[i].real = crealf(temp);
        symbols[i].imaginary = cimagf(temp);
    }

    memcpy(&d->stats_unpacked->received_signal_strength_indicator, packed_iter, sizeof(float));
    packed_iter += sizeof(float);

    memcpy(&d->stats_unpacked->error_vector_magnitude, packed_iter, sizeof(float));
    packed_iter += sizeof(float);

    int valid;
    memcpy(&valid, packed_iter, sizeof(int));
    d->stats_unpacked->checksum_passed = (bool)(valid);

    ring_reader_unlock(d->stats_ring);

    return d->stats_unpacked;
}

ssize_t quiet_decoder_recv(quiet_decoder *d, uint8_t *data, size_t len) {
    size_t framelen;
    ssize_t framelen_written;
    ring_reader_lock(d->buf);
    framelen_written = ring_read(d->buf, (uint8_t*)(&framelen), sizeof(size_t));
    if (framelen_written <= 0) {
        ring_reader_unlock(d->buf);
        switch (framelen_written) {
            case 0:
                return 0;
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

    // we will throw away part of the frame if len < framelen here
    // this mirrors the unix recv() spec
    len = (len > framelen) ? framelen : len;

    if (ring_read(d->buf, data, len) < 0) {
        // the last read we did was for the length of the frame, and
        //   now we tried to read as many bytes as that length describes
        // but that read failed, and the decoder writer is supposed to write
        //   them together, so something has gone very wrong
        // e.g. we should never see a frame length without a frame written
        //   (the lock would be held once for both calls)
        ring_reader_unlock(d->buf);
        assert(false && "ring buffer failed: frame not written atomically?");
        quiet_set_last_error(quiet_io);
        return -1;
    }

    ring_advance_reader(d->buf, framelen - len);

    ring_reader_unlock(d->buf);
    return len;
}

static size_t decoder_max_len(decoder *d) {
    if (!d) {
        return 0;
    }

    return d->symbolbuf_len * d->demod->opt.samples_per_symbol;
}

ssize_t quiet_decoder_consume(decoder *d, const sample_t *samplebuf, size_t sample_len) {
    if (!d) {
        return 0;
    }

    ring_writer_lock(d->buf);
    bool closed = ring_is_closed(d->buf);
    ring_writer_unlock(d->buf);

    if (closed) {
        return 0;
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

    return sample_len;
}

bool quiet_decoder_frame_in_progress(decoder *d) {
    switch (d->opt.encoding) {
    case ofdm_encoding:
        return ofdmflexframesync_is_frame_open(d->frame.ofdm.framesync);
        break;
    case modem_encoding:
        return flexframesync_is_frame_open(d->frame.modem.framesync);
        break;
    case gmsk_encoding:
        return gmskframesync_is_frame_open(d->frame.gmsk.framesync);
        break;
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

void quiet_decoder_close(decoder *d) {
    ring_reader_lock(d->buf);
    ring_close(d->buf);
    ring_reader_unlock(d->buf);

    if (d->stats_ring) {
        ring_reader_lock(d->stats_ring);
        ring_close(d->stats_ring);
        ring_reader_unlock(d->stats_ring);
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
    if (d->stats_ring) {
        ring_destroy(d->stats_ring);
    }
    if (d->stats_packed) {
        free(d->stats_packed);
    }
    if (d->stats_unpacked) {
        if (d->stats_unpacked->symbols) {
            free((quiet_complex *)d->stats_unpacked->symbols);
        }
        free(d->stats_unpacked);
    }
    if (d->writeframe) {
        free(d->writeframe);
    }
    demodulator_destroy(d->demod);
    free(d->symbolbuf);
    free(d);
}
