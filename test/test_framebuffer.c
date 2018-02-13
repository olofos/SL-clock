#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "unity.h"
#include "framebuffer.h"
#include "log.h"

//////// Constants used in tests ///////////////////////////////////////////////

#define BUFFER_MARGIN (FB_SIZE * 4)
#define CANARY 0x55

//////// Stubs needed by frambuffer.c //////////////////////////////////////////

const uint8_t paw_64x64[1];
uint8_t *framebuffer;

void log_log(enum log_level level, enum log_system system, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    printf("\n");
}


void fb_display(void)
{
}

//////// Global variables used for testing /////////////////////////////////////

static uint8_t raw_buffer[FB_SIZE + 2 * BUFFER_MARGIN];

static uint8_t icon8x8[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint8_t icon17x17[3*17] = {
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01
};



//////// Helper functions for testing //////////////////////////////////////////

void setUp(void)
{
    memset(raw_buffer, CANARY, FB_SIZE + 2 * BUFFER_MARGIN);
    framebuffer = &raw_buffer[BUFFER_MARGIN];
}

void tearDown(void)
{
}

static void clear_framebuffer(void)
{
    memset(framebuffer, 0, FB_SIZE);
}

static void assert_no_drawing_outside_framebuffer(void)
{
    for(int i = 0; i < BUFFER_MARGIN; i++) {
        TEST_ASSERT_EQUAL_HEX8(CANARY, raw_buffer[i]);
    }

    for(int i = BUFFER_MARGIN + FB_SIZE; i < 2 * BUFFER_MARGIN + FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_HEX8(CANARY, raw_buffer[i]);
    }
}

static void draw_icon_row_around_point(int x, int y, int w, int h, uint8_t *icon)
{
    fb_blit(x-32, y, w, h, icon, 0);
    fb_blit(x-16, y, w, h, icon, 0);
    fb_blit(x-8, y, w, h, icon, 0);
    fb_blit(x-4, y, w, h, icon, 0);
    fb_blit(x-1, y, w, h, icon, 0);
    fb_blit(x, y, w, h, icon, 0);
    fb_blit(x+1, y, w, h, icon, 0);
    fb_blit(x+4, y, w, h, icon, 0);
    fb_blit(x+8, y, w, h, icon, 0);
    fb_blit(x+16, y, w, h, icon, 0);
    fb_blit(x+32, y, w, h, icon, 0);
}

static void draw_icon_around_point(int x, int y, int w, int h, uint8_t *icon)
{
    draw_icon_row_around_point(x, y-32, w, h, icon);
    draw_icon_row_around_point(x, y-16, w, h, icon);
    draw_icon_row_around_point(x, y-8, w, h, icon);
    draw_icon_row_around_point(x, y-4, w, h, icon);
    draw_icon_row_around_point(x, y-1, w, h, icon);
    draw_icon_row_around_point(x, y, w, h, icon);
    draw_icon_row_around_point(x, y+1, w, h, icon);
    draw_icon_row_around_point(x, y+4, w, h, icon);
    draw_icon_row_around_point(x, y+8, w, h, icon);
    draw_icon_row_around_point(x, y+16, w, h, icon);
    draw_icon_row_around_point(x, y+32, w, h, icon);
}

static int get_pixel(int x, int y)
{
    return framebuffer[x + FB_WIDTH * (y / 8)] & (1 << (y % 8));
}

static void test__fb_blit__should__draw_icon_in_the_correct_spot_helper(int x, int y, int w, int h, uint8_t *icon)
{
    clear_framebuffer();
    fb_set_pen(FB_NORMAL);

    fb_blit(x, y, w, h, icon, 0);

    for(int i = 0; i < FB_WIDTH; i++) {
        for(int j = 0; j < FB_HEIGHT; j++) {
            if((x <= i) && (i < x + w) && (y <= j) && (j < y + h)) {
                TEST_ASSERT_TRUE(get_pixel(i, j));
            } else {
                TEST_ASSERT_FALSE(get_pixel(i, j));
            }
        }
    }
}


//////// Test //////////////////////////////////////////////////////////////////

static void test__fb_clear__should__clear_framebuffer(void)
{
    fb_clear();

    for(int i = 0; i < FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, framebuffer[i]);
    }
}

static void test__fb_clear__should__not_draw_outside_framebuffer(void)
{
    fb_clear();

    assert_no_drawing_outside_framebuffer();
}

static void test__fb_blit__should__not_draw_outside_framebuffer(void)
{
    int w = 8;
    int h = 8;
    uint8_t *icon = icon8x8;

    draw_icon_around_point(0, 0, w, h, icon);
    draw_icon_around_point(FB_WIDTH/2, 0, w, h, icon);
    draw_icon_around_point(FB_WIDTH, 0, w, h, icon);

    draw_icon_around_point(0, FB_HEIGHT / 2, w, h, icon);
    draw_icon_around_point(FB_WIDTH/2, FB_HEIGHT / 2, w, h, icon);
    draw_icon_around_point(FB_WIDTH, FB_HEIGHT / 2, w, h, icon);

    draw_icon_around_point(0, FB_HEIGHT, w, h, icon);
    draw_icon_around_point(FB_WIDTH/2, FB_HEIGHT, w, h, icon);
    draw_icon_around_point(FB_WIDTH, FB_HEIGHT, w, h, icon);

    assert_no_drawing_outside_framebuffer();
}

static void test__fb_blit__should__draw_small_icon_in_the_correct_spot(void)
{
    int w = 8;
    int h = 8;
    uint8_t *icon = icon8x8;

    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(0, 0, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH/2, 0, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH, 0, w, h, icon);

    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(0, FB_HEIGHT / 2, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH/2, FB_HEIGHT / 2, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH, FB_HEIGHT / 2, w, h, icon);

    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(0, FB_HEIGHT, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH/2, FB_HEIGHT, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH, FB_HEIGHT, w, h, icon);
}

static void test__fb_blit__should__draw_large_icon_in_the_correct_spot(void)
{
    int w = 17;
    int h = 17;
    uint8_t *icon = icon17x17;

    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(0, 0, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH/2, 0, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH, 0, w, h, icon);

    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(0, FB_HEIGHT / 2, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH/2, FB_HEIGHT / 2, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH, FB_HEIGHT / 2, w, h, icon);

    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(0, FB_HEIGHT, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH/2, FB_HEIGHT, w, h, icon);
    test__fb_blit__should__draw_icon_in_the_correct_spot_helper(FB_WIDTH, FB_HEIGHT, w, h, icon);
}


//////// Main //////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test__fb_clear__should__clear_framebuffer);
    RUN_TEST(test__fb_clear__should__not_draw_outside_framebuffer);

    RUN_TEST(test__fb_blit__should__not_draw_outside_framebuffer);
    RUN_TEST(test__fb_blit__should__draw_small_icon_in_the_correct_spot);
    RUN_TEST(test__fb_blit__should__draw_large_icon_in_the_correct_spot);

    return UNITY_END();
}
