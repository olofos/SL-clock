#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "oled_framebuffer.h"
#include "matrix_framebuffer.h"
#include "fonts.h"
#include "log.h"

#define LOG_SYS LOG_SYS_DISPLAY

#define FB_SIZE ((OLED_SIZE > MATRIX_SIZE) ? OLED_SIZE : MATRIX_SIZE)

static uint8_t the_framebuffer[FB_SIZE];
uint8_t *framebuffer = the_framebuffer;

void (*fb_blit)(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len);

enum fb_pen fb_current_pen = FB_NORMAL;

void fb_set_pen(enum fb_pen pen)
{
    fb_current_pen = pen;
}

void fb_draw_icon(int16_t x, int16_t y, const struct icon *icon, enum fb_alignment alignment)
{
    if(!icon) {
        return;
    }

    if(alignment & FB_ALIGN_CENTER_H)
    {
        x -= icon->width/2;
    } else if(alignment & FB_ALIGN_END_H) {
        x -= icon->width;
    }

    if(alignment & FB_ALIGN_CENTER_V)
    {
        y -= icon->height / 2;
    } else if(alignment & FB_ALIGN_END_V)
    {
        y -= icon->height;
    }

    fb_blit(x, y, icon->width, icon->height, icon->data, 0);
}

void fb_draw_string(int16_t x, int16_t y, const char *text, uint8_t len, const uint8_t *font_data, enum fb_alignment alignment)
{
    const uint8_t char_height = font_data[FONT_HEIGHT_POS];
    const uint8_t first_char = font_data[FONT_FIRST_CHAR_POS];
    const uint8_t char_num = font_data[FONT_CHAR_NUM_POS];
    const uint16_t jump_table_size = char_num * FONT_JUMPTABLE_BYTES;

    if(!text) {
        return;
    }

    if(!len) {
        len = strlen(text);
    }

    if(alignment & FB_ALIGN_CENTER_H) {
        x -= fb_string_length(text, len, font_data) / 2;
    } else if(alignment & FB_ALIGN_END_H) {
        x -= fb_string_length(text, len, font_data);
    }

    if(alignment & FB_ALIGN_CENTER_V) {
        y -= char_height / 2;
    } else if(alignment & FB_ALIGN_END_V) {
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
    if(fd < 0)
    {
        LOG("File '%s' not found", filename);
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
