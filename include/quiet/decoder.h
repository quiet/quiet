#include <assert.h>

#include "quiet/common.h"
#include "quiet/demodulator.h"
#include "quiet/ring.h"

const size_t decoder_default_buffer_len = 1 << 16;

static int decoder_on_decode(unsigned char *header, int header_valid, unsigned char *payload,
                             unsigned int payload_len, int payload_valid,
                             framesyncstats_s stats, void *dvoid);
static void decoder_ofdm_create(const decoder_options *opt, decoder *d);
static void decoder_modem_create(const decoder_options *opt, decoder *d);
static size_t decoder_max_len(decoder *d);
