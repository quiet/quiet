#include "quiet/common.h"
#include "quiet/modulator.h"

static void encoder_ofdm_create(const encoder_options *opt, encoder *e);
static void encoder_modem_create(const encoder_options *opt, encoder *e);
static int encoder_is_assembled(encoder *e);
static void encoder_consume(encoder *e);
static size_t encoder_fillsymbols(encoder *e, size_t requested_length);
