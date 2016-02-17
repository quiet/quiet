#include <stdio.h>

#include "quiet.h"

#include <PortAudio.h>

int encode_callback(const void *inputBuffer, void *outputBuffer,
                    unsigned long framesPerBuffer,
                    const PaStreamCallbackTimeInfo *timeInfo,
                    PaStreamCallbackFlags statusFlags,
                    void *data)
{
    float *buf = (float*)outputBuffer;
    for (size_t i = 0; i < framesPerBuffer; i++) {
        buf[i] = 0;
        buf[i + 1] = 0;
    }
    return 0;
}

int encode_to_soundcard(FILE *input, encoder_options *opt) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio\n");
        return 1;
    }

    size_t sample_buffer_size = 16384;
    size_t desired_sample_rate = 44100;
    PaStream *stream;
    err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, desired_sample_rate,
                               sample_buffer_size, encode_callback, NULL);
    if (err != paNoError) {
        printf("failed to open port audio stream\n");
        return 1;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(stream);
    encoder_opt_set_sample_rate(opt, info->sampleRate);

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("failed to start port audio stream\n");
        return 1;
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: encode_soundcard <profilename> <input_source>\n");
        exit(1);
    }
    encoder_options *encodeopt =
        get_encoder_profile_file("profiles.json", argv[1]);

    FILE *input;
    if (strncmp(argv[2], "-", 2)) {
        input = stdin;
    } else {
        input = fopen(argv[2], "rb");
    }

    int code = encode_to_soundcard(input, encodeopt);

    fclose(input);

    free(encodeopt);

    return code;
}
