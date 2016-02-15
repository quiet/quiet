#ifndef QUIET_COMMON_H
#define QUIET_COMMON_H
#include "quiet.h"

static const unsigned int SAMPLE_RATE = 44100;
unsigned char *create_ofdm_subcarriers(const ofdm_options *opt);
#endif  // QUIET_COMMON_H
