#include <stdint.h>
#include <string.h>

#include "matrix_framebuffer.h"
#include "fonts.h"

static void matrix_set_pixel_row(uint8_t n, uint8_t m)
{
    if(fb_current_pen == FB_NORMAL) {
        framebuffer[n] |= m;
    } else {
        framebuffer[n] &= ~m;
    }
}

void matrix_set_pixel(int16_t x,int16_t y)
{
    if((0 <= x) && (x < MATRIX_WIDTH) && (0 <= y) && (y < MATRIX_HEIGHT))
    {
        const uint8_t n = (x / 8) + (MATRIX_WIDTH / 8) * y;
        const uint8_t m = 1 << (7 - (x % 8));

        matrix_set_pixel_row(n, m);
    }
}

void matrix_clear(void)
{
    memset(framebuffer, 0, MATRIX_SIZE);
}

void matrix_blit(int16_t x0, int16_t y0, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len)
{
    uint8_t raster_height = 1 + ((h - 1) / 8);

    if(!len)
    {
        len = w * raster_height;
    }

    for(int16_t x = 0; x < w; x++) {
        for(int16_t y = 0; y < h; y++) {
            uint8_t n = (y / 8) + x * raster_height;
            if((n < len) && (data[n] & (1 << (y % 8)))) {
                matrix_set_pixel(x0+x,y0+y);
            }
        }
    }
}
