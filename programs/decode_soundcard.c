#include <string.h>
#include <signal.h>

#include "quiet-portaudio.h"

static quiet_portaudio_decoder *decoder = NULL;
static void sig_handler(int signal) {
    if (decoder) {
        quiet_portaudio_decoder_close(decoder);
    }
}

int decode_from_soundcard(FILE *output, quiet_decoder_options *opt) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    PaDeviceIndex device = Pa_GetDefaultInputDevice();
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(device);
    double sample_rate = deviceInfo->defaultSampleRate;
    PaTime latency = deviceInfo->defaultLowInputLatency;

    decoder = quiet_portaudio_decoder_create(opt, device, latency, sample_rate);
    quiet_portaudio_decoder_set_blocking(decoder, 0, 0);

    size_t write_buffer_size = 16384;
    uint8_t *write_buffer = malloc(write_buffer_size*sizeof(uint8_t));

    while (true) {
        ssize_t read = quiet_portaudio_decoder_recv(decoder, write_buffer, write_buffer_size);
        if (read <= 0) {
            break;
        }
        fwrite(write_buffer, 1, read, output);
    }

    free(write_buffer);
    quiet_portaudio_decoder_destroy(decoder);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: encode_soundcard <profilename> [<output_destination>]\n");
        exit(1);
    }
    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_filename(QUIET_PROFILES_LOCATION, argv[1]);

    if (!decodeopt) {
        printf("failed to read profile %s from %s\n", argv[1], QUIET_PROFILES_LOCATION);
        exit(1);
    }

    FILE *output;
    if ((argc == 2) || strncmp(argv[2], "-", 2) == 0) {
        output = stdout;
        setvbuf(stdout, NULL, _IONBF, 0);  // in order to get interactive let's make stdout unbuffered
    } else {
        output = fopen(argv[2], "wb");
        if (!output) {
            fprintf(stderr, "failed to open %s: ", argv[2]);
            perror(NULL);
            exit(1);
        }
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    int code = decode_from_soundcard(output, decodeopt);

    fclose(output);
    free(decodeopt);

    Pa_Terminate();

    return code;
}
