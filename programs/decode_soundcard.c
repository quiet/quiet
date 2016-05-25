#include <string.h>

#include "quiet-portaudio.h"

int decode_from_soundcard(FILE *output, quiet_decoder_options *opt) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio, %s\n", Pa_GetErrorText(err));
        return 1;
    }
    PaDeviceIndex device = Pa_GetDefaultInputDevice();

    quiet_portaudio_decoder *d = quiet_portaudio_decoder_create(opt, device, 16384);

    size_t write_buffer_size = 16384;
    uint8_t *write_buffer = malloc(write_buffer_size*sizeof(uint8_t));
    bool done = false;

    while (!done) {

        for (;;) {
            ssize_t read = quiet_portaudio_decoder_recv(d, write_buffer, write_buffer_size);
            if (read < 0) {
                break;
            }
            fwrite(write_buffer, 1, read, output);
        }
    }

    free(write_buffer);
    quiet_portaudio_decoder_destroy(d);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        printf("usage: encode_soundcard <profilename> [<output_destination>]\n");
        exit(1);
    }
    quiet_decoder_options *decodeopt =
        quiet_decoder_profile_filename(QUIET_PROFILES_LOCATION, argv[1]);

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

    PaTerminate();

    return code;
}
