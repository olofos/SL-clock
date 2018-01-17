#ifndef FRAMEBUFFER_H_
#define FRAMEBUFFER_H_


#define FB_WIDTH 128
#define FB_HEIGHT 64
#define FB_SIZE (FB_WIDTH * FB_HEIGHT / 8)

enum alignment
{
    FB_ALIGN_NONE = 0,
    FB_ALIGN_CENTER_H = 0x01,
    FB_ALIGN_CENTER_V = 0x02
};

struct icon {
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
};

void fb_set_pixel(int16_t x, int16_t y, int8_t color);
void fb_display(void);

void fb_clear(void);
void fb_splash(void);

void fb_draw_string(int16_t x, int16_t y, const char *text, const uint8_t *font_data, enum alignment alignment);
uint16_t fb_string_length(const char *text, const uint8_t *font_data);

void fb_blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len);

void fb_draw_icon(int16_t x, int16_t y, const struct icon *icon, enum alignment alignment);

struct icon *fb_load_icon_pbm(const char *filename);
void fb_free_icon(struct icon *);

#endif
