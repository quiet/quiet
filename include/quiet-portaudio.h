#ifndef QUIET_PORTAUDIO_H
#define QUIET_PORTAUDIO_H
#include <quiet.h>
#include <portaudio.h>

// Sound encoder
struct quiet_portaudio_encoder;
typedef struct quiet_portaudio_encoder quiet_portaudio_encoder;

quiet_portaudio_encoder *quiet_portaudio_encoder_create(const quiet_encoder_options *opt, PaDeviceIndex device, PaTime latency, double sample_rate, size_t sample_buffer_size);

void quiet_portaudio_encoder_set_blocking(quiet_portaudio_encoder *e, time_t sec, long nano);

void quiet_portaudio_encoder_set_nonblocking(quiet_portaudio_encoder *e);

size_t quiet_portaudio_encoder_get_frame_len(const quiet_portaudio_encoder *e);

size_t quiet_portaudio_encoder_clamp_frame_len(quiet_portaudio_encoder *e, size_t sample_len);

ssize_t quiet_portaudio_encoder_send(quiet_portaudio_encoder *enc, const uint8_t *buf, size_t len);

ssize_t quiet_portaudio_encoder_emit(quiet_portaudio_encoder *enc);

void quiet_portaudio_encoder_emit_empty(quiet_portaudio_encoder *enc);

void quiet_portaudio_encoder_close(quiet_portaudio_encoder *enc);

void quiet_portaudio_encoder_destroy(quiet_portaudio_encoder *enc);

// Sound decoder
struct quiet_portaudio_decoder;
typedef struct quiet_portaudio_decoder quiet_portaudio_decoder;

quiet_portaudio_decoder *quiet_portaudio_decoder_create(const quiet_decoder_options *opt, PaDeviceIndex device, PaTime latency, double sample_rate);

ssize_t quiet_portaudio_decoder_recv(quiet_portaudio_decoder *d, uint8_t *data, size_t len);

void quiet_portaudio_decoder_set_blocking(quiet_portaudio_decoder *d, time_t sec, long nano);

void quiet_portaudio_decoder_set_nonblocking(quiet_portaudio_decoder *d);

bool quiet_portaudio_decoder_frame_in_progress(quiet_portaudio_decoder *d);

unsigned int quiet_portaudio_decoder_checksum_fails(const quiet_portaudio_decoder *d);

const quiet_decoder_frame_stats *quiet_portaudio_decoder_consume_stats(quiet_portaudio_decoder *d, size_t *num_frames);

void quiet_portaudio_decoder_enable_stats(quiet_portaudio_decoder *d);

void quiet_portaudio_decoder_disable_stats(quiet_portaudio_decoder *d);

void quiet_portaudio_decoder_close(quiet_portaudio_decoder *d);

void quiet_portaudio_decoder_destroy(quiet_portaudio_decoder *d);

#endif
