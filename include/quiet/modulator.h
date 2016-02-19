#include "quiet/common.h"

modulator *modulator_create(const modulator_options *opt);
size_t modulator_sample_len(const modulator *m, size_t symbol_len);
size_t modulator_symbol_len(const modulator *m, size_t sample_len);
// modulator_emit assumes that samples is large enough to store symbol_len *
// samples_per_symbol samples
// returns number of samples written to *samples
size_t modulator_emit(modulator *m, const float complex *symbols, size_t symbol_len,
                             sample_t *samples);
size_t modulator_flush_sample_len(modulator *m);
size_t modulator_flush(modulator *m, sample_t *samples);
void modulator_reset(modulator *m);
void modulator_destroy(modulator *m);
