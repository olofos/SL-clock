#ifndef MATRIX_FRAMEBUFFER_H_
#define MATRIX_FRAMEBUFFER_H_

#include "framebuffer.h"

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32
#define MATRIX_SIZE (MATRIX_WIDTH * MATRIX_HEIGHT / 8)

void matrix_set_pixel(int16_t x, int16_t y);
void matrix_display(void);

void matrix_clear(void);
void matrix_splash(void);

void matrix_blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len);

#endif
