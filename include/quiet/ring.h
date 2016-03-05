#include "quiet/common.h"

ring *ring_create(size_t length);
void ring_destroy(ring *r);
size_t ring_calculate_distance(const ring *r, const uint8_t *src, const uint8_t *dst);
uint8_t *ring_calculate_advance(const ring *r, uint8_t *p, size_t adv);
ssize_t ring_write(ring *r, const uint8_t *buf, size_t len);
ssize_t ring_read(ring *r, uint8_t *dst, size_t len);
