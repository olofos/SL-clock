#ifndef FRAMEBUFFER_H_
#define FRAMEBUFFER_H_


#define FB_WIDTH 128
#define FB_HEIGHT 64
#define FB_SIZE (FB_WIDTH * FB_HEIGHT / 8)

void fb_set_pixel(int16_t x, int16_t y, int8_t color);
void fb_display(void);

void fb_clear(void);
void fb_splash(void);

void fb_draw_string(int16_t x, int16_t y, const char *text, const uint8_t *font_data);
uint16_t fb_string_length(const char *text, const uint8_t *font_data);

#endif
