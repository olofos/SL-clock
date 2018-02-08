#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "framebuffer.h"
#include "fonts.h"
#include "logo-paw-64x64.h"

#include "log.h"
#define LOG_SYS LOG_SYS_DISPLAY

static enum fb_pen current_pen = FB_NORMAL;

void fb_set_pen(enum fb_pen pen)
{
    current_pen = pen;
}

static void fb_set_pixel_row(uint16_t n, uint8_t m)
{
    if(current_pen == FB_NORMAL) {
        framebuffer[n] |= m;
    } else {
        framebuffer[n] &= ~m;
    }
}


void fb_set_pixel(int16_t x,int16_t y)
{
    if((0 <= x) && (x < FB_WIDTH) && (0 <= y) && (y < FB_HEIGHT))
    {
        const uint16_t n = x + FB_WIDTH * (y / 8);
        const uint8_t m = 1 << (y % 8);

        fb_set_pixel_row(n, m);
    }
}

void fb_clear(void)
{
    memset(framebuffer, 0, FB_SIZE);
}

void fb_blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len)
{
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
        int16_t data_pos = x_pos + ((y >> 3) + (i % raster_height)) * FB_WIDTH;

        if((0 <= x_pos) && (x_pos < FB_WIDTH))
        {
            if((0 <= data_pos) && (data_pos < FB_SIZE))
            {
                fb_set_pixel_row(data_pos, c << offset);
            }
            if((0 <= data_pos + FB_WIDTH) && (data_pos + FB_WIDTH))
            {
                fb_set_pixel_row(data_pos + FB_WIDTH, c >> (8 - offset));
            }
        }
    }
}

void fb_draw_icon(int16_t x, int16_t y, const struct icon *icon, enum fb_alignment alignment)
{
    if(alignment & FB_ALIGN_CENTER_H)
    {
        x -= icon->width/2;
    }

    if(alignment & FB_ALIGN_CENTER_V)
    {
        y -= icon->height / 2;
    }

    fb_blit(x, y, icon->width, icon->height, icon->data, 0);
}

void fb_draw_string(int16_t x, int16_t y, const char *text, uint8_t len, const uint8_t *font_data, enum fb_alignment alignment)
{
    const uint8_t char_height = font_data[FONT_HEIGHT_POS];
    const uint8_t first_char = font_data[FONT_FIRST_CHAR_POS];
    const uint8_t char_num = font_data[FONT_CHAR_NUM_POS];
    const uint16_t jump_table_size = char_num * FONT_JUMPTABLE_BYTES;

    if(!len) {
        len = strlen(text);
    }

    if(alignment & FB_ALIGN_CENTER_H) {
        x -= fb_string_length(text, len, font_data) / 2;
    }

    if(alignment & FB_ALIGN_CENTER_V) {
        y -= char_height / 2;
    }

    for(uint8_t i = 0; i < len; i++) {
        uint8_t c = text[i];

        if(c >= first_char && (c - first_char) < char_num) {
            c -= first_char;

            const uint8_t *jump_table_entry = font_data + FONT_JUMPTABLE_START + c * FONT_JUMPTABLE_BYTES;

            const uint16_t char_index = (jump_table_entry[FONT_JUMPTABLE_MSB] << 8) | jump_table_entry[FONT_JUMPTABLE_LSB];
            const uint8_t char_bytes = jump_table_entry[FONT_JUMPTABLE_SIZE];
            const uint8_t char_width = jump_table_entry[FONT_JUMPTABLE_WIDTH];

            if(char_index != 0xFFFF) {
                const uint8_t *char_p = font_data + FONT_JUMPTABLE_START + jump_table_size + char_index;
                fb_blit(x, y, char_width, char_height, char_p, char_bytes);
            }

            x += char_width;
        }
    }
}

uint16_t fb_string_length(const char *text, uint8_t n, const uint8_t *font_data)
{
    const uint8_t first_char = font_data[FONT_FIRST_CHAR_POS];
    const uint8_t char_num = font_data[FONT_CHAR_NUM_POS];

    if(!n) {
        n = strlen(text);
    }

    uint16_t len = 0;
    for(uint8_t i = 0; i < n; i++) {
        uint8_t c = text[i];
        if(c >= first_char && (c - first_char) < char_num) {
            c -= first_char;
            const uint8_t *jump_table_entry = font_data + FONT_JUMPTABLE_START + c * FONT_JUMPTABLE_BYTES;
            len += jump_table_entry[FONT_JUMPTABLE_WIDTH];
        }
    }
    return len;
}

static uint8_t reverse(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

struct icon *fb_load_icon_pbm(const char *filename)
{
    LOG("Loading icon from file %s", filename);

    int fd = open(filename, O_RDONLY);
    if(!fd)
    {
        return 0;
    }

    char buf[8];

    read(fd, buf, 3);

    if((buf[0] != 'P') || (buf[1] != '4') || (buf[2] != '\n'))
    {
        buf[3] = 0;
        LOG("Unexpected PBM magic '%s'", buf);
        close(fd);
        return 0;
    }

    int i = 0;

    do {
        read(fd, &buf[i++], 1);
    } while((i < sizeof(buf)-1) && (buf[i-1] != ' '));

    buf[i] = 0;
    if(buf[i-1] != ' ')
    {
        LOG("Could not find width of PBM (got '%s')", buf);
        close(fd);
        return 0;
    }

    int h = strtol(buf, 0, 10);

    i = 0;

    do {
        read(fd, &buf[i++], 1);
    } while((i < sizeof(buf)-1) && (buf[i-1] != '\n'));

    buf[i] = 0;
    if(buf[i-1] != '\n')
    {
        LOG("Could not find height of PBM (got '%s')", buf);
        close(fd);
        return 0;
    }

    int w = strtol(buf, 0, 10);

    struct icon *icon = malloc(sizeof(struct icon));

    int h_bytes = 1 + (h - 1) / 8;

    icon->width = w;
    icon->height = h;

    uint8_t *data = malloc(h_bytes * w);

    if(data)
    {
        read(fd, data, h_bytes * w);

        for(int j = 0; j < h_bytes * w; j++)
        {
            data[j] = reverse(data[j]);
        }
    }

    icon->data = data;

    close(fd);
    return icon;
}

void fb_free_icon(struct icon *icon)
{
    if(icon)
    {
        if(icon->data)
        {
            free((char *)icon->data);
        }
        free(icon);
    }
}

void fb_splash(void)
{
    fb_clear();

    for(int y = 0; y < 8; y++)
    {
        fb_blit(32, y * 8, 64, 8, &paw_64x64[y * 64], 64);
    }

    fb_display();
}

static void fb_draw_line_v(int16_t x, int16_t y1, int16_t y2)
{
    if((0 > x) || (x >= FB_WIDTH)) {
        return;
    }

    if(y1 < 0) {
        y1 = 0;
    }

    if(y2 >= FB_HEIGHT) {
        y2 = FB_HEIGHT - 1;
    }

    if(y2 < y1) {
        return;
    }

    const uint8_t site1 = y1 / 8;
    const uint8_t site2 = y2 / 8;

    const uint8_t m1 = 0xFF << (y1 & 0x07);
    const uint8_t m2 = 0xFF >> (7 - (y2 & 0x07));

    if(site2 > site1) {
        fb_set_pixel_row(x + site1 * FB_WIDTH, m1);
        fb_set_pixel_row(x + site2 * FB_WIDTH, m2);

        for(uint8_t s = site1 + 1; s < site2; s++) {
            fb_set_pixel_row(x + s * FB_WIDTH, 0xFF);
        }
    } else {
        fb_set_pixel_row(x + site1 * FB_WIDTH, m1 & m2);
    }
}

static void fb_draw_line_h(int16_t x1, int16_t x2, int16_t y)
{
    if((0 > y) || (y >= FB_HEIGHT)) {
        return;
    }

    if(x1 < 0) {
        x1 = 0;
    }

    if(x2 >= FB_WIDTH) {
        x2 = FB_WIDTH - 1;
    }

    if(x2 < x1) {
        return;
    }

    uint8_t s = y / 8;
    uint8_t m = 1 << (y & 0x07);

    for(uint8_t x = x1; x <= x2; x++) {
        fb_set_pixel_row(x + s * FB_WIDTH, m);
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

void fb_draw_rect(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    fb_draw_line_h(x,x+w-1,y);
    fb_draw_line_h(x,x+w-1,y+h-1);

    fb_draw_line_v(x,    y,y+h-1);
    fb_draw_line_v(x+w-1,y,y+h-1);
}


void fb_draw_rect_round(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    fb_blit(x,     y,     8, 8, corner_nw, 0);
    fb_blit(x+w-8, y,     8, 8, corner_ne, 0);
    fb_blit(x,     y+h-8, 8, 8, corner_sw, 0);
    fb_blit(x+w-8, y+h-8, 8, 8, corner_se, 0);

    fb_draw_line_h(x+8,x+w-8-1,y);
    fb_draw_line_h(x+8,x+w-8-1,y+h-1);

    fb_draw_line_v(x,    y+8,y+h-8-1);
    fb_draw_line_v(x+w-1,y+8,y+h-8-1);
}

void fb_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    for(uint16_t i = 0; i < w; i++) {
        fb_draw_line_v(x + i, y, y + h - 1);
    }
}

void fb_fill_rect_round(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    fb_blit(x,     y,     8, 8, corner_fill_nw, 0);
    fb_blit(x+w-8, y,     8, 8, corner_fill_ne, 0);
    fb_blit(x,     y+h-8, 8, 8, corner_fill_sw, 0);
    fb_blit(x+w-8, y+h-8, 8, 8, corner_fill_se, 0);

    for(uint8_t i = 0; i < 8; i++) {
        fb_draw_line_v(x+i,     y+8, y+h-9);
        fb_draw_line_v(x+w-8+i, y+8, y+h-9);
    }

    for(uint16_t i = x + 8; i < x + w - 8; i++) {
        fb_draw_line_v(i, y, y+h-1);
    }
}
