#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "oled_framebuffer.h"
#include "fonts.h"
#include "logo-paw-64x64.h"

#include "log.h"
#define LOG_SYS LOG_SYS_DISPLAY

static void oled_set_pixel_row(uint16_t n, uint8_t m)
{
    if(fb_current_pen == FB_NORMAL) {
        framebuffer[n] |= m;
    } else {
        framebuffer[n] &= ~m;
    }
}


void oled_set_pixel(int16_t x,int16_t y)
{
    if((0 <= x) && (x < OLED_WIDTH) && (0 <= y) && (y < OLED_HEIGHT))
    {
        const uint16_t n = x + OLED_WIDTH * (y / 8);
        const uint8_t m = 1 << (y % 8);

        oled_set_pixel_row(n, m);
    }
}

void oled_clear(void)
{
    memset(framebuffer, 0, OLED_SIZE);
}

void oled_blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len)
{
    if(-h >= y || y > OLED_HEIGHT) {
        return;
    }
    uint8_t raster_height = 1 + ((h - 1) / 8);
    int8_t offset = y & 0x07;

    if(!len)
    {
        len = w * raster_height;
    }

    for(uint16_t i = 0; i < len; i++)
    {
        uint8_t c = *data++;

        int16_t x_pos = x + (i / raster_height);
        int16_t data_pos = x_pos + ((y >> 3) + (i % raster_height)) * OLED_WIDTH;

        if((0 <= x_pos) && (x_pos < OLED_WIDTH))
        {
            if((0 <= data_pos) && (data_pos < OLED_SIZE))
            {
                oled_set_pixel_row(data_pos, c << offset);
            }
            if((0 <= data_pos + OLED_WIDTH) && (data_pos + OLED_WIDTH < OLED_SIZE))
            {
                oled_set_pixel_row(data_pos + OLED_WIDTH, c >> (8 - offset));
            }
        }
    }
}

void oled_splash(void)
{
    oled_clear();

    for(int y = 0; y < 8; y++)
    {
        oled_blit(32, y * 8, 64, 8, &paw_64x64[y * 64], 64);
    }

    oled_display();
}

static void oled_draw_line_v(int16_t x, int16_t y1, int16_t y2)
{
    if((0 > x) || (x >= OLED_WIDTH)) {
        return;
    }

    if(y1 < 0) {
        y1 = 0;
    }

    if(y2 >= OLED_HEIGHT) {
        y2 = OLED_HEIGHT - 1;
    }

    if(y2 < y1) {
        return;
    }

    const uint8_t site1 = y1 / 8;
    const uint8_t site2 = y2 / 8;

    const uint8_t m1 = 0xFF << (y1 & 0x07);
    const uint8_t m2 = 0xFF >> (7 - (y2 & 0x07));

    if(site2 > site1) {
        oled_set_pixel_row(x + site1 * OLED_WIDTH, m1);
        oled_set_pixel_row(x + site2 * OLED_WIDTH, m2);

        for(uint8_t s = site1 + 1; s < site2; s++) {
            oled_set_pixel_row(x + s * OLED_WIDTH, 0xFF);
        }
    } else {
        oled_set_pixel_row(x + site1 * OLED_WIDTH, m1 & m2);
    }
}

static void oled_draw_line_h(int16_t x1, int16_t x2, int16_t y)
{
    if((0 > y) || (y >= OLED_HEIGHT)) {
        return;
    }

    if(x1 < 0) {
        x1 = 0;
    }

    if(x2 >= OLED_WIDTH) {
        x2 = OLED_WIDTH - 1;
    }

    if(x2 < x1) {
        return;
    }

    uint8_t s = y / 8;
    uint8_t m = 1 << (y & 0x07);

    for(uint8_t x = x1; x <= x2; x++) {
        oled_set_pixel_row(x + s * OLED_WIDTH, m);
    }
}


const uint8_t corner_sw[] = { 0b00000111, 0b00011000, 0b00100000, 0b01000000, 0b01000000, 0b10000000, 0b10000000, 0b10000000, };
const uint8_t corner_nw[] = { 0b11100000, 0b00011000, 0b00000100, 0b00000010, 0b00000010, 0b00000001, 0b00000001, 0b00000001, };
const uint8_t corner_se[] = { 0b10000000, 0b10000000, 0b10000000, 0b01000000, 0b01000000, 0b00100000, 0b00011000, 0b00000111, };
const uint8_t corner_ne[] = { 0b00000001, 0b00000001, 0b00000001, 0b00000010, 0b00000010, 0b00000100, 0b00011000, 0b11100000, };
static const uint8_t corner_fill_sw[] = { 0b00000111, 0b00011111, 0b00111111, 0b01111111, 0b01111111, 0b11111111, 0b11111111, 0b11111111, };
static const uint8_t corner_fill_nw[] = { 0b11100000, 0b11111000, 0b11111100, 0b11111110, 0b11111110, 0b11111111, 0b11111111, 0b11111111, };
static const uint8_t corner_fill_se[] = { 0b11111111, 0b11111111, 0b11111111, 0b01111111, 0b01111111, 0b00111111, 0b00011111, 0b00000111, };
static const uint8_t corner_fill_ne[] = { 0b11111111, 0b11111111, 0b11111111, 0b11111110, 0b11111110, 0b11111100, 0b11111000, 0b11100000, };

void oled_draw_rect(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    oled_draw_line_h(x,x+w-1,y);
    oled_draw_line_h(x,x+w-1,y+h-1);

    oled_draw_line_v(x,    y,y+h-1);
    oled_draw_line_v(x+w-1,y,y+h-1);
}


void oled_draw_rect_round(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    oled_blit(x,     y,     8, 8, corner_nw, 0);
    oled_blit(x+w-8, y,     8, 8, corner_ne, 0);
    oled_blit(x,     y+h-8, 8, 8, corner_sw, 0);
    oled_blit(x+w-8, y+h-8, 8, 8, corner_se, 0);

    oled_draw_line_h(x+8,x+w-8-1,y);
    oled_draw_line_h(x+8,x+w-8-1,y+h-1);

    oled_draw_line_v(x,    y+8,y+h-8-1);
    oled_draw_line_v(x+w-1,y+8,y+h-8-1);
}

void oled_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    for(uint16_t i = 0; i < w; i++) {
        oled_draw_line_v(x + i, y, y + h - 1);
    }
}

void oled_fill_rect_round(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    oled_blit(x,     y,     8, 8, corner_fill_nw, 0);
    oled_blit(x+w-8, y,     8, 8, corner_fill_ne, 0);
    oled_blit(x,     y+h-8, 8, 8, corner_fill_sw, 0);
    oled_blit(x+w-8, y+h-8, 8, 8, corner_fill_se, 0);

    for(uint8_t i = 0; i < 8; i++) {
        oled_draw_line_v(x+i,     y+8, y+h-9);
        oled_draw_line_v(x+w-8+i, y+8, y+h-9);
    }

    for(uint16_t i = x + 8; i < x + w - 8; i++) {
        oled_draw_line_v(i, y, y+h-1);
    }
}
