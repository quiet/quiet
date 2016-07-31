#include "quiet/portaudio_encoder.h"

portaudio_encoder *quiet_portaudio_encoder_create(const quiet_encoder_options *opt, PaDeviceIndex device, PaTime latency, double sample_rate, size_t sample_buffer_size) {
    size_t num_channels = 2;
    PaStreamParameters param = {
        .device = device,
        .channelCount = num_channels,
        .sampleFormat = paFloat32,
        .suggestedLatency = latency,
        .hostApiSpecificStreamInfo = NULL,
    };

    PaError err;
    PaStream *stream;
    err = Pa_OpenStream(&stream, NULL, &param, sample_rate,
                        sample_buffer_size, paNoFlag, NULL, NULL);
    if (err != paNoError) {
        printf("failed to open port audio stream, %s\n", Pa_GetErrorText(err));
        return NULL;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(stream);
    quiet_encoder *e = quiet_encoder_create(opt, info->sampleRate);

    quiet_sample_t *sample_buffer = calloc(num_channels * sample_buffer_size,
                                           sizeof(quiet_sample_t));
    quiet_sample_t *mono_buffer = malloc(sample_buffer_size * sizeof(quiet_sample_t));

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("failed to start port audio stream, %s\n", Pa_GetErrorText(err));
        return NULL;
    }

    portaudio_encoder *enc = malloc(1 * sizeof(portaudio_encoder));
    enc->enc = e;
    enc->sample_buffer = sample_buffer;
    enc->mono_buffer = mono_buffer;
    enc->stream = stream;
    enc->sample_buffer_size = sample_buffer_size;
    enc->num_channels = num_channels;

    return enc;
}

size_t quiet_portaudio_encoder_get_frame_len(const portaudio_encoder *e) {
    return quiet_encoder_get_frame_len(e->enc);
}

size_t quiet_portaudio_encoder_clamp_frame_len(portaudio_encoder *e, size_t sample_len) {
    return quiet_encoder_clamp_frame_len(e->enc, sample_len);
}

ssize_t quiet_portaudio_encoder_send(portaudio_encoder *enc, uint8_t *buf, size_t len) {
    return quiet_encoder_send(enc->enc, buf, len);
}

ssize_t quiet_portaudio_encoder_emit(portaudio_encoder *enc) {
    memset(enc->sample_buffer, 0, enc->sample_buffer_size * enc->num_channels * sizeof(quiet_sample_t));
    memset(enc->mono_buffer, 0, enc->sample_buffer_size * sizeof(quiet_sample_t));
    ssize_t written = quiet_encoder_emit(enc->enc, enc->mono_buffer, enc->sample_buffer_size);
    for (size_t i = 0; i < enc->sample_buffer_size; i++) {
        enc->sample_buffer[enc->num_channels * i] = enc->mono_buffer[i];
    }
    PaError err;
    err = Pa_WriteStream(enc->stream, enc->sample_buffer, enc->sample_buffer_size);
    if (err == paOutputUnderflowed) {
        printf("output audio stream underflowed\n");
    } else if (err != paNoError) {
        printf("failed to write to port audio stream, %s\n", Pa_GetErrorText(err));
        return -1;
    }
    return written;
}

void quiet_portaudio_encoder_emit_empty(portaudio_encoder *enc) {
    memset(enc->sample_buffer, 0, enc->sample_buffer_size * enc->num_channels * sizeof(quiet_sample_t));
    PaError err;
    err = Pa_WriteStream(enc->stream, enc->sample_buffer, enc->sample_buffer_size);
    if (err == paOutputUnderflowed) {
        printf("output audio stream underflowed\n");
    } else if (err != paNoError) {
        printf("failed to write to port audio stream, %s\n", Pa_GetErrorText(err));
        return;
    }
}

void quiet_portaudio_encoder_destroy(portaudio_encoder *enc) {
    for (int i = 0; i < 5; i++) {
        quiet_portaudio_encoder_emit_empty(enc);
    }
    Pa_StopStream(enc->stream);
    Pa_CloseStream(enc->stream);

    free(enc->sample_buffer);
    free(enc->mono_buffer);
    quiet_encoder_destroy(enc->enc);
    free(enc);
}
