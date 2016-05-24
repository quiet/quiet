#include "quiet/common.h"
#include <math.h>

osc *osc_create(float freq);
void osc_destroy(osc *o);
void osc_mix_down_16384(osc *o, float *input, float complex *output);
void osc_mix_down(osc *o, float *input, float complex *output, size_t n);
