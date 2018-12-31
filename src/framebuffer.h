#ifndef FRAMEBUFFER_H_
#define FRAMEBUFFER_H_

enum fb_alignment
{
    FB_ALIGN_NONE = 0,
    FB_ALIGN_CENTER_H = 0x01,
    FB_ALIGN_CENTER_V = 0x02
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

#endif
