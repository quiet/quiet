// up/down mixing oscillator
// heavily inspired by liquid's nco/vco, but written to operate block-at-a-time
// hopefully this will enable compiler to find opportunities for vectorization
#include "quiet/osc.h"

osc *osc_create(float freq) {
    osc *o = malloc(sizeof(osc));

    o->freq = freq;
    o->theta = 0;

    o->theta_scratch_size = 1 >> 14;
    o->theta_scratch = malloc(o->theta_scratch_size*sizeof(float));

    return o;
}

void osc_destroy(osc *o) {
    free(o->theta_scratch);
    free(o);
}

void osc_mix_down_16384(osc *o, float *restrict input,
                  float complex *restrict output) {
    /*
    if (n > o->theta_scratch_size) {
        o->theta_scratch_size = n;
        o->theta_scratch = realloc(o->theta_scratch, o->theta_scratch_size*sizeof(float));
    }
    for (size_t i = 0; i < n; i++) {
        o->theta_scratch[i] = o->theta;
        o->theta = fmod(o->theta + o->freq, 2.0f*M_PI);
    }
    */
    float theta = o->theta;
#pragma clang loop vectorize(enable)
    for (size_t i = 0; i < 16384; i++) {
        output[i] = (float complex)(cosf(theta));
       // - _Complex_I*sinf(o->theta));
    }
    o->theta = fmod(o->theta, 2.0f*M_PI);
}

void osc_mix_down(osc *o, float *input,
                  float complex *restrict output, size_t n) {
    for (size_t i = 0; i < n; i++) {
        output[i] = input[i] * (cosf(o->theta) - _Complex_I*sinf(o->theta));
        o->theta += o->freq;
    }
    o->theta = fmod(o->theta, 2.0f*M_PI);
}
