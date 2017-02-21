#include <assert.h>

#include "quiet/common.h"
#include "quiet/modulator.h"
#if RING_ATOMIC
#include "quiet/ring_atomic.h"
#elif RING_BLOCKING
#include "quiet/ring_blocking.h"
#else
#include "quiet/ring.h"
#endif

const size_t encoder_default_buffer_len = 1 << 16;

typedef struct { ofdmflexframegen framegen; } ofdm_encoder;

typedef struct {
    flexframegen framegen;
    size_t symbols_remaining;
} modem_encoder;

typedef struct {
    gmskframegen framegen;
    size_t stride;
} gmsk_encoder;

struct quiet_encoder {
    encoder_options opt;
    union {
        ofdm_encoder ofdm;
        modem_encoder modem;
        gmsk_encoder gmsk;
    } frame;
    modulator *mod;
    float complex *symbolbuf;
    size_t symbolbuf_len;
    sample_t *samplebuf;
    size_t samplebuf_cap;
    size_t samplebuf_len;
    size_t samplebuf_offset;
    const uint8_t *payload;
    size_t payload_length;
    bool has_flushed;
    bool is_queue_closed;
    bool is_close_frame;
    float resample_rate;
    resamp_rrrf resampler;
    ring *buf;
    uint8_t *tempframe;
    uint8_t *readframe;
};

static void encoder_ofdm_create(const encoder_options *opt, encoder *e);
static void encoder_modem_create(const encoder_options *opt, encoder *e);
static int encoder_is_assembled(encoder *e);
static size_t encoder_fillsymbols(encoder *e, size_t requested_length);
static size_t quiet_encoder_sample_len(quiet_encoder *e, size_t data_len);
