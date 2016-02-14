#include "quiet/common.h"

demodulator *create_demodulator(const demodulator_options *opt);
size_t demodulate_symbol_len(const demodulator *d, size_t sample_len);
size_t demodulate(demodulator *d, sample_t *samples, size_t sample_len,
                  float complex *symbols);
size_t demodulate_flush_symbol_len(demodulator *d);
size_t demodulate_flush(demodulator *d, float complex *symbols);
void destroy_demodulator(demodulator *d);
