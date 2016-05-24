#include "quiet/portaudio_decoder.h"

portaudio_decoder *quiet_portaudio_decoder_create(const decoder_options *opt, PaDeviceIndex device, size_t sample_buffer_size) {
    PaStream *stream;
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    unsigned int desired_sample_rate = deviceInfo->defaultSampleRate;
    PaStreamParameters param = {
        .device = device,
        .channelCount = 1,
        .sampleFormat = paFloat32,
        .suggestedLatency = deviceInfo->defaultHighInputLatency,
        .hostApiSpecificStreamInfo = NULL,
    };
    PaError err = Pa_OpenStream(&stream, &param, NULL, desired_sample_rate,
                        sample_buffer_size, paNoFlag, NULL, NULL);
    if (err != paNoError) {
        printf("failed to open port audio stream, %s\n", Pa_GetErrorText(err));
        return NULL;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("failed to start port audio stream, %s\n", Pa_GetErrorText(err));
        return NULL;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(stream);
    decoder *d = quiet_decoder_create(opt, info->sampleRate);

    quiet_sample_t *sample_buffer = malloc(sample_buffer_size * sizeof(quiet_sample_t));
    portaudio_decoder *decoder = malloc(1 * sizeof(portaudio_decoder));
    decoder->dec = d;
    decoder->stream = stream;
    decoder->sample_buffer = sample_buffer;
    decoder->sample_buffer_size = sample_buffer_size;

    return decoder;
}

ssize_t quiet_portaudio_decoder_recv(quiet_portaudio_decoder *d, uint8_t *data, size_t len) {
    return quiet_decoder_recv(d->dec, data, len);
}

void quiet_portaudio_decoder_consume(quiet_portaudio_decoder *d) {
    PaError err = Pa_ReadStream(d->stream, d->sample_buffer, d->sample_buffer_size);
    if (err != paNoError) {
        printf("failed to write to port audio stream, %s\n", Pa_GetErrorText(err));
        return;
    }
    quiet_decoder_consume(d->dec, d->sample_buffer, d->sample_buffer_size);
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
    Pa_Terminate();

    quiet_decoder_destroy(d->dec);
    free(d->sample_buffer);
    free(d);
}

