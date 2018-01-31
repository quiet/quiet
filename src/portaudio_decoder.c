#include "quiet/portaudio_decoder.h"
#include <unistd.h>

static void decoder_dealloc(portaudio_decoder *dec) {
    if (!dec) {
        return;
    }
    if (dec->stream) {
        Pa_CloseStream(dec->stream);
    }
    if (dec->dec) {
        quiet_decoder_destroy(dec->dec);
    }
    if (dec->mono_buffer) {
        free(dec->mono_buffer);
    }
    if (dec->consume_ring) {
        ring_destroy(dec->consume_ring);
    }
    pthread_mutex_destroy(&dec->reflock);
    free(dec);
}

static unsigned int decoder_decref(portaudio_decoder *dec) {
    pthread_mutex_lock(&dec->reflock);
    dec->refcnt--;
    unsigned int refcnt = dec->refcnt;
    pthread_mutex_unlock(&dec->reflock);
    if (refcnt == 0) {
        decoder_dealloc(dec);
    }
    return refcnt;
}

static int decoder_callback(const void *input_buffer_v, void *output_buffer_v,
                            unsigned long frame_count, const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags, void *decoder_v) {

    portaudio_decoder *dec = (portaudio_decoder *)decoder_v;
    quiet_sample_t *input_buffer = (quiet_sample_t *)input_buffer_v;

    if (frame_count > dec->mono_buffer_size) {
        // XXX we need an upsized buffer if this happens
        return paAbort;
    }

    for (size_t i = 0; i < frame_count; i++) {
        dec->mono_buffer[i] = input_buffer[i * dec->num_channels];
    }

    ring_writer_lock(dec->consume_ring);
    quiet_sample_t *buf = dec->mono_buffer;
    while (frame_count > 0) {
        size_t next_write = frame_count < 64 ? frame_count : 64;
        frame_count -= next_write;
        ssize_t ring_written =
            ring_write(dec->consume_ring, buf, next_write * sizeof(quiet_sample_t));
        if (ring_written == RingErrorWouldBlock) {
            // in this case, do we signal that we lost samples?
            ring_writer_unlock(dec->consume_ring);
            return paContinue;
        }
        if (ring_written == 0) {
            // someone closed the ring, so quit
            ring_writer_unlock(dec->consume_ring);
            return paComplete;
        }
        if (ring_written < 0) {
            ring_writer_unlock(dec->consume_ring);
            return paAbort;
        }
        buf += next_write;
    }
    ring_writer_unlock(dec->consume_ring);
    return paContinue;
}

static void *consume(void *dec_void) {
    portaudio_decoder *dec = (portaudio_decoder *)dec_void;
    size_t pcm_buffer_len = 64;
    quiet_sample_t *pcm = malloc(pcm_buffer_len * sizeof(quiet_sample_t));
    for (;;) {
        // get some pcm from the soundcard thread via dec->consume_ring
        ring_reader_lock(dec->consume_ring);
        ssize_t bytes_read = ring_read(dec->consume_ring, pcm, pcm_buffer_len * sizeof(quiet_sample_t));
        ring_reader_unlock(dec->consume_ring);

        if (bytes_read <= 0) {
            break;
        }

        quiet_decoder_consume(dec->dec, pcm, (bytes_read / (sizeof(quiet_sample_t))));
    }
    free(pcm);

    while (Pa_IsStreamActive(dec->stream)) {
        usleep(100);
    }
    quiet_decoder_close(dec->dec);

    decoder_decref(dec);

    return NULL;
}

portaudio_decoder *quiet_portaudio_decoder_create(const decoder_options *opt, PaDeviceIndex device,
                                                  PaTime latency, double sample_rate) {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    size_t num_channels = 2 < deviceInfo->maxInputChannels ? 2 : deviceInfo->maxInputChannels;
    PaStreamParameters param = {
        .device = device,
        .channelCount = num_channels,
        .sampleFormat = paFloat32,
        .suggestedLatency = latency,
        .hostApiSpecificStreamInfo = NULL,
    };

    portaudio_decoder *dec = calloc(1, sizeof(portaudio_decoder));
    dec->refcnt++;
    pthread_mutex_init(&dec->reflock, NULL);
    PaError err = Pa_OpenStream(&dec->stream, &param, NULL, sample_rate, 0, paNoFlag, decoder_callback, dec);
    if (err != paNoError) {
        printf("failed to open port audio stream, %s\n", Pa_GetErrorText(err));
        decoder_decref(dec);
        return NULL;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(dec->stream);
    decoder *d = quiet_decoder_create(opt, info->sampleRate);

    dec->mono_buffer_size = (1 << 14);
    quiet_sample_t *mono_buffer =
        malloc(dec->mono_buffer_size * sizeof(quiet_sample_t));
    dec->dec = d;
    dec->mono_buffer = mono_buffer;
    dec->num_channels = num_channels;
    dec->consume_ring = ring_create(4 * (1 << 14));
    ring_set_reader_blocking(dec->consume_ring, 0, 0);

    err = Pa_StartStream(dec->stream);
    if (err != paNoError) {
        printf("failed to start port audio stream, %s\n", Pa_GetErrorText(err));
        decoder_decref(dec);
        return NULL;
    }

    dec->refcnt++;
    pthread_attr_t consume_attr;
    pthread_attr_init(&consume_attr);
    pthread_attr_setdetachstate(&consume_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&dec->consume_thread, &consume_attr, consume, dec);

    return dec;
}

ssize_t quiet_portaudio_decoder_recv(quiet_portaudio_decoder *d, uint8_t *data, size_t len) {
    return quiet_decoder_recv(d->dec, data, len);
}

void quiet_portaudio_decoder_set_blocking(quiet_portaudio_decoder *d, time_t sec, long nano) {
    quiet_decoder_set_blocking(d->dec, sec, nano);
}

void quiet_portaudio_decoder_set_nonblocking(quiet_portaudio_decoder *d) {
    quiet_decoder_set_nonblocking(d->dec);
}

bool quiet_portaudio_decoder_frame_in_progress(quiet_portaudio_decoder *d) {
    // TODO lock decoder!
    return quiet_decoder_frame_in_progress(d->dec);
}

unsigned int quiet_portaudio_decoder_checksum_fails(const quiet_portaudio_decoder *d) {
    // TODO lock decoder!
    return quiet_decoder_checksum_fails(d->dec);
}

const quiet_decoder_frame_stats *quiet_portaudio_decoder_consume_stats(quiet_portaudio_decoder *d,
                                                                       size_t *num_frames) {
    // TODO lock decoder!
    return quiet_decoder_consume_stats(d->dec, num_frames);
}

void quiet_portaudio_decoder_enable_stats(quiet_portaudio_decoder *d) {
    // TODO lock decoder!
    quiet_decoder_enable_stats(d->dec);
}

void quiet_portaudio_decoder_disable_stats(quiet_portaudio_decoder *d) {
    // TODO lock decoder!
    quiet_decoder_disable_stats(d->dec);
}

void quiet_portaudio_decoder_close(quiet_portaudio_decoder *dec) {
    ring_writer_lock(dec->consume_ring);
    ring_close(dec->consume_ring);
    ring_writer_unlock(dec->consume_ring);
}

void quiet_portaudio_decoder_destroy(quiet_portaudio_decoder *d) {
    decoder_decref(d);
}
