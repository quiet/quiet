#include <math.h>
#include <string.h>
#include <unistd.h>

#include "quiet-portaudio.h"

int encode_to_soundcard(FILE *input, quiet_encoder_options *opt) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    PaDeviceIndex device = Pa_GetDefaultOutputDevice();
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    double sample_rate = deviceInfo->defaultSampleRate;
    PaTime latency = deviceInfo->defaultLowOutputLatency;

    size_t sample_buffer_size = 16384;
    quiet_portaudio_encoder *e = quiet_portaudio_encoder_create(opt, device, latency, sample_rate, sample_buffer_size);

    size_t read_buffer_size = 16384;
    uint8_t *read_buffer = malloc(read_buffer_size*sizeof(uint8_t));
    bool done = false;

    while (!done) {
        size_t nread = fread(read_buffer, sizeof(uint8_t), read_buffer_size, input);
        if (nread == 0) {
            break;
        } else if (nread < read_buffer_size) {
            done = true;
        }

        size_t frame_len = quiet_portaudio_encoder_get_frame_len(e);
        for (size_t i = 0; i < nread; i += frame_len) {
            frame_len = (frame_len > (nread - i)) ? (nread - i) : frame_len;
            quiet_portaudio_encoder_send(e, read_buffer + i, frame_len);
        }
    }

    quiet_portaudio_encoder_close(e);

    free(read_buffer);

    quiet_portaudio_encoder_destroy(e);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: encode_soundcard <profilename> [<input_source>]\n");
        exit(1);
    }
    quiet_encoder_options *encodeopt =
        quiet_encoder_profile_filename(QUIET_PROFILES_LOCATION, argv[1]);

    if (!encodeopt) {
        printf("failed to read profile %s from %s\n", argv[1], QUIET_PROFILES_LOCATION);
        exit(1);
    }

    FILE *input;
    if ((argc == 2) || strncmp(argv[2], "-", 2) == 0) {
        input = stdin;
    } else {
        input = fopen(argv[2], "rb");
        if (!input) {
            fprintf(stderr, "failed to open %s: ", argv[2]);
            perror(NULL);
            exit(1);
        }
    }

    int code = encode_to_soundcard(input, encodeopt);

    fclose(input);
    free(encodeopt);

    Pa_Terminate();

    return code;
}
