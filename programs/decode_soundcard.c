#include <stdio.h>

#include "quiet.h"

#include <PortAudio.h>

int decode_from_soundcard(FILE *output, quiet_decoder_options *opt) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    size_t sample_buffer_size = 1<<14;
    quiet_sample_t *sample_buffer = malloc(sample_buffer_size*sizeof(quiet_sample_t));
    PaStream *stream;
    PaDeviceIndex device = Pa_GetDefaultInputDevice();
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    unsigned int desired_sample_rate = deviceInfo->defaultSampleRate;
    PaStreamParameters param = {
        .device = device,
        .channelCount = 1,
        .sampleFormat = paFloat32,
        .suggestedLatency = deviceInfo->defaultHighInputLatency,
        .hostApiSpecificStreamInfo = NULL,
    };
    err = Pa_OpenStream(&stream, &param, NULL, desired_sample_rate,
                        sample_buffer_size, paNoFlag, NULL, NULL);
    if (err != paNoError) {
        printf("failed to open port audio stream, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(stream);
    quiet_decoder *d = quiet_decoder_create(opt, info->sampleRate);

    size_t write_buffer_size = 16384;
    uint8_t *write_buffer = malloc(write_buffer_size*sizeof(uint8_t));
    bool done = false;

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("failed to start port audio stream, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    while (!done) {
        err = Pa_ReadStream(stream, sample_buffer, sample_buffer_size);
        if (err != paNoError) {
            printf("failed to write to port audio stream, %s\n", Pa_GetErrorText(err));
            return 1;
        }
        size_t accum = quiet_decoder_recv(d, sample_buffer, sample_buffer_size);

        if (accum > 0) {
            size_t want = (accum > write_buffer_size) ? write_buffer_size : accum;
            size_t read = quiet_decoder_readbuf(d, write_buffer, want);
            fwrite(write_buffer, 1, read, output);
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    quiet_decoder_destroy(d);
    free(sample_buffer);
    free(write_buffer);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: encode_soundcard <profilename> [<output_destination>]\n");
        exit(1);
    }
    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_filename("profiles.json", argv[1]);

    FILE *output;
    if ((argc == 2) || strncmp(argv[2], "-", 2) == 0) {
        output = stdout;
        setvbuf(stdout, NULL, _IONBF, 0);  // in order to get interactive let's make stdout unbuffered
    } else {
        output = fopen(argv[2], "wb");
    }

    int code = decode_from_soundcard(output, decodeopt);

    fclose(output);
    free(decodeopt);

    return code;
}
