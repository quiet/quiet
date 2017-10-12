#include "quiet/portaudio_decoder.h"

static int decoder_callback(const void *input_buffer_v, void *output_buffer_v,
                            unsigned long frame_count, const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags, void *decoder_v) {
    portaudio_decoder *dec = (portaudio_decoder *)decoder_v;
    const quiet_sample_t* input_buffer = (const quiet_sample_t *)input_buffer_v;
    if (frame_count > dec->sample_buffer_size) {
        return -1;
    }
    for (size_t i = 0; i < frame_count; i++) {
        dec->mono_buffer[i] = 0;
        for (size_t j = 0; j < dec->num_channels; j++) {
            dec->mono_buffer[i] += input_buffer[(i * dec->num_channels) + j];
        }
    }
    quiet_decoder_consume(dec->dec, dec->mono_buffer, frame_count);
    return 0;
    // XXX have some way of closing the quiet decoder - maybe flags?
    // check other flags?
}

portaudio_decoder *quiet_portaudio_decoder_create(const decoder_options *opt, PaDeviceIndex device, PaTime latency, double sample_rate, size_t sample_buffer_size) {
    PaStream *stream;
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    size_t num_channels = 2 < deviceInfo->maxInputChannels ? 2 : deviceInfo->maxInputChannels;
    PaStreamParameters param = {
        .device = device,
        .channelCount = num_channels,
        .sampleFormat = paFloat32,
        .suggestedLatency = latency,
        .hostApiSpecificStreamInfo = NULL,
    };
    portaudio_decoder *dec = malloc(1 * sizeof(portaudio_decoder));
    PaError err = Pa_OpenStream(&stream, &param, NULL, sample_rate,
                        sample_buffer_size, paNoFlag, decoder_callback, dec);
    if (err != paNoError) {
        printf("failed to open port audio stream, %s\n", Pa_GetErrorText(err));
        return NULL;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(stream);
    decoder *d = quiet_decoder_create(opt, info->sampleRate);

    sample_buffer_size = sample_buffer_size ? sample_buffer_size : 16384;
    quiet_sample_t *sample_buffer = malloc(num_channels * sample_buffer_size * sizeof(quiet_sample_t));
    quiet_sample_t *mono_buffer = malloc(sample_buffer_size * sizeof(quiet_sample_t));
    dec->dec = d;
    dec->stream = stream;
    dec->sample_buffer = sample_buffer;
    dec->mono_buffer = mono_buffer;
    dec->sample_buffer_size = sample_buffer_size;
    dec->num_channels = num_channels;

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("failed to start port audio stream, %s\n", Pa_GetErrorText(err));
        return NULL;
    }

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

void quiet_portaudio_decoder_consume(quiet_portaudio_decoder *d) {
    PaError err = Pa_ReadStream(d->stream, d->sample_buffer, d->sample_buffer_size);
    if (err != paNoError) {
        printf("failed to read port audio stream, %s\n", Pa_GetErrorText(err));
        return;
    }
    for (size_t i = 0; i < d->sample_buffer_size; i++) {
        d->mono_buffer[i] = 0;
        for (size_t j = 0; j < d->num_channels; j++) {
            d->mono_buffer[i] += d->sample_buffer[(i * d->num_channels) + j];
        }
    }
    quiet_decoder_consume(d->dec, d->mono_buffer, d->sample_buffer_size);
}

bool quiet_portaudio_decoder_frame_in_progress(quiet_portaudio_decoder *d) {
    return quiet_decoder_frame_in_progress(d->dec);
}

unsigned int quiet_portaudio_decoder_checksum_fails(const quiet_portaudio_decoder *d) {
    return quiet_decoder_checksum_fails(d->dec);
}

const quiet_decoder_frame_stats *quiet_portaudio_decoder_consume_stats(quiet_portaudio_decoder *d, size_t *num_frames) {
    return quiet_decoder_consume_stats(d->dec, num_frames);
}

void quiet_portaudio_decoder_enable_stats(quiet_portaudio_decoder *d) {
    quiet_decoder_enable_stats(d->dec);
}

void quiet_portaudio_decoder_disable_stats(quiet_portaudio_decoder *d) {
    quiet_decoder_disable_stats(d->dec);
}

void quiet_portaudio_decoder_destroy(quiet_portaudio_decoder *d) {
    Pa_StopStream(d->stream);
    Pa_CloseStream(d->stream);

    quiet_decoder_destroy(d->dec);
    free(d->sample_buffer);
    free(d->mono_buffer);
    free(d);
}

