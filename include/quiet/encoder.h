#include <assert.h>

#include "quiet/common.h"
#include "quiet/modulator.h"
#include "quiet/ring.h"

const size_t encoder_default_buffer_len = 1 << 16;

static void encoder_ofdm_create(const encoder_options *opt, encoder *e);
static void encoder_modem_create(const encoder_options *opt, encoder *e);
static int encoder_is_assembled(encoder *e);
static size_t encoder_fillsymbols(encoder *e, size_t requested_length);
