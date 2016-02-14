#include "quiet/common.h"

modulator *create_modulator(const modulator_options *opt);
size_t modulate_sample_len(const modulator *m, size_t symbol_len);
size_t modulate_symbol_len(const modulator *m, size_t sample_len);
// modulate assumes that samples is large enough to store symbol_len *
// samples_per_symbol samples
// returns number of samples written to *samples
size_t modulate(modulator *m, const float complex *symbols, size_t symbol_len,
                sample_t *samples);
size_t modulate_flush_sample_len(modulator *m);
size_t modulate_flush(modulator *m, sample_t *samples);
void modulate_reset(modulator *m);
void destroy_modulator(modulator *m);
