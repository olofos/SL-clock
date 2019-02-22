#ifndef FRAMEBUFFER_H_
#define FRAMEBUFFER_H_

enum fb_alignment
{
    FB_ALIGN_NONE     = 0,
    FB_ALIGN_CENTER_H = 0x01,
    FB_ALIGN_CENTER_V = 0x02,
    FB_ALIGN_END_H    = 0x04,
    FB_ALIGN_END_V    = 0x08,
};

enum fb_pen
{
    FB_NORMAL = 0,
    FB_INVERSE = 1
};

struct icon {
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
};

extern uint8_t *framebuffer;

extern void (*fb_blit)(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len);
extern enum fb_pen fb_current_pen;

void fb_set_pen(enum fb_pen pen);

void fb_draw_string(int16_t x, int16_t y, const char *text, uint8_t len, const uint8_t *font_data, enum fb_alignment alignment);
uint16_t fb_string_length(const char *text, uint8_t len, const uint8_t *font_data);

void fb_draw_icon(int16_t x, int16_t y, const struct icon *icon, enum fb_alignment alignment);

struct icon *fb_load_icon_pbm(const char *filename);
void fb_free_icon(struct icon *);


#endif
