#include <assert.h>

#include "quiet/common.h"

demodulator *demodulator_create(const demodulator_options *opt);
size_t demodulator_recv(demodulator *d, const sample_t *samples, size_t sample_len,
                               float complex *symbols);
size_t demodulator_flush(demodulator *d, float complex *symbols);
void demodulator_destroy(demodulator *d);
size_t demodulator_flush_symbol_len(const demodulator *d);
