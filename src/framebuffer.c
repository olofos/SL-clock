#include <stdint.h>
#include "oled_framebuffer.h"

static uint8_t the_framebuffer[OLED_SIZE];
uint8_t *framebuffer = the_framebuffer;
