#include <math.h>
#include <string.h>

#include "quiet.h"

#include <PortAudio.h>

int encode_to_soundcard(FILE *input, quiet_encoder_options *opt) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    size_t num_channels = 2;
    size_t sample_buffer_size = 1<<14;
    quiet_sample_t *sample_buffer = calloc(num_channels*sample_buffer_size, sizeof(quiet_sample_t));
    quiet_sample_t *mono_buffer = malloc(sample_buffer_size*sizeof(quiet_sample_t));
    PaStream *stream;
    PaDeviceIndex device = Pa_GetDefaultOutputDevice();
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    unsigned int desired_sample_rate = deviceInfo->defaultSampleRate;
    PaStreamParameters param = {
        .device = device,
        .channelCount = num_channels,
        .sampleFormat = paFloat32,
        .suggestedLatency = deviceInfo->defaultHighOutputLatency,
        .hostApiSpecificStreamInfo = NULL,
    };
    err = Pa_OpenStream(&stream, NULL, &param, desired_sample_rate,
                        sample_buffer_size, paNoFlag, NULL, NULL);
    if (err != paNoError) {
        printf("failed to open port audio stream, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(stream);
    quiet_encoder *e = quiet_encoder_create(opt, info->sampleRate);

    size_t read_buffer_size = 16384;
    uint8_t *read_buffer = malloc(read_buffer_size*sizeof(uint8_t));
    bool done = false;

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("failed to start port audio stream, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    while (!done) {
        size_t nread = fread(read_buffer, sizeof(uint8_t), read_buffer_size, input);
        if (nread == 0) {
            break;
        } else if (nread < read_buffer_size) {
            done = true;
        }

        size_t frame_len = quiet_encoder_get_frame_len(e);
        for (size_t i = 0; i < nread; i += frame_len) {
            frame_len = (frame_len > (nread - i)) ? (nread - i) : frame_len;
            quiet_encoder_send(e, read_buffer + i, frame_len);
        }

        size_t written = sample_buffer_size;
        while (written == sample_buffer_size) {
            written = quiet_encoder_emit(e, mono_buffer, sample_buffer_size);
            for (size_t i = 0; i < written; i++) {
                sample_buffer[num_channels*i] = mono_buffer[i];
            }
            err = Pa_WriteStream(stream, sample_buffer, written);
            if (err != paNoError) {
                printf("failed to write to port audio stream, %s\n", Pa_GetErrorText(err));
                return 1;
            }
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    quiet_encoder_destroy(e);
    free(sample_buffer);
    free(mono_buffer);
    free(read_buffer);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: encode_soundcard <profilename> [<input_source>]\n");
        exit(1);
    }
    quiet_encoder_options *encodeopt =
        quiet_encoder_profile_filename(QUIET_PROFILES_LOCATION, argv[1]);

    FILE *input;
    if ((argc == 2) || strncmp(argv[2], "-", 2) == 0) {
        input = stdin;
    } else {
        input = fopen(argv[2], "rb");
    }

    int code = encode_to_soundcard(input, encodeopt);

    fclose(input);
    free(encodeopt);

    return code;
}
