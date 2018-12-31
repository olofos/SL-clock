#ifndef OLED_FRAMEBUFFER_H_
#define OLED_FRAMEBUFFER_H_

#include "framebuffer.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

extern uint8_t *oled_framebuffer;

void oled_set_pixel(int16_t x, int16_t y);
void oled_display(void);

void oled_set_pen(enum fb_pen pen);

void oled_clear(void);
void oled_splash(void);

void oled_draw_string(int16_t x, int16_t y, const char *text, uint8_t len, const uint8_t *font_data, enum fb_alignment alignment);
uint16_t oled_string_length(const char *text, uint8_t len, const uint8_t *font_data);

void oled_blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len);

void oled_draw_icon(int16_t x, int16_t y, const struct icon *icon, enum fb_alignment alignment);

struct icon *oled_load_icon_pbm(const char *filename);
void oled_free_icon(struct icon *);

void oled_draw_rect(int16_t x, int16_t y, uint16_t w, uint16_t h);
void oled_draw_rect_round(int16_t x, int16_t y, uint16_t w, uint16_t h);
void oled_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h);
void oled_fill_rect_round(int16_t x, int16_t y, uint16_t w, uint16_t h);



#endif
