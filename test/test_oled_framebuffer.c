#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include "oled_framebuffer.h"
#include "log.h"

//////// Constants used in tests ///////////////////////////////////////////////

#define BUFFER_MARGIN (OLED_SIZE * 4)
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


void oled_display(void)
{
}

//////// Global variables used for testing /////////////////////////////////////

static uint8_t raw_buffer[OLED_SIZE + 2 * BUFFER_MARGIN];

static uint8_t icon8x8[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint8_t icon17x17[3*17] = {
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x01,
    0xFF, 0xFF, 0x01
};



//////// Helper functions for testing //////////////////////////////////////////

static int setup(void **state)
{
    memset(raw_buffer, CANARY, OLED_SIZE + 2 * BUFFER_MARGIN);
    framebuffer = &raw_buffer[BUFFER_MARGIN];

    return 0;
}

static void clear_framebuffer(void)
{
    memset(framebuffer, 0, OLED_SIZE);
}

static void assert_no_drawing_outside_framebuffer(void)
{
    for(int i = 0; i < BUFFER_MARGIN; i++) {
        assert_int_equal(CANARY, raw_buffer[i]);
    }

    for(int i = BUFFER_MARGIN + OLED_SIZE; i < 2 * BUFFER_MARGIN + OLED_SIZE; i++) {
        assert_int_equal(CANARY, raw_buffer[i]);
    }
}

static void draw_icon_row_around_point(int x, int y, int w, int h, uint8_t *icon)
{
    oled_blit(x-32, y, w, h, icon, 0);
    oled_blit(x-16, y, w, h, icon, 0);
    oled_blit(x-8, y, w, h, icon, 0);
    oled_blit(x-4, y, w, h, icon, 0);
    oled_blit(x-1, y, w, h, icon, 0);
    oled_blit(x, y, w, h, icon, 0);
    oled_blit(x+1, y, w, h, icon, 0);
    oled_blit(x+4, y, w, h, icon, 0);
    oled_blit(x+8, y, w, h, icon, 0);
    oled_blit(x+16, y, w, h, icon, 0);
    oled_blit(x+32, y, w, h, icon, 0);
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
    return framebuffer[x + OLED_WIDTH * (y / 8)] & (1 << (y % 8));
}

static void test__oled_blit__should__draw_icon_in_the_correct_spot_helper(int x, int y, int w, int h, uint8_t *icon)
{
    clear_framebuffer();
    oled_set_pen(FB_NORMAL);

    oled_blit(x, y, w, h, icon, 0);

    for(int i = 0; i < OLED_WIDTH; i++) {
        for(int j = 0; j < OLED_HEIGHT; j++) {
            if((x <= i) && (i < x + w) && (y <= j) && (j < y + h)) {
                assert_true(get_pixel(i, j));
            } else {
                assert_false(get_pixel(i, j));
            }
        }
    }
}


//////// Tests /////////////////////////////////////////////////////////////////

static void test__oled_clear__should__clear_framebuffer(void **state)
{
    oled_clear();

    for(int i = 0; i < OLED_SIZE; i++) {
        assert_int_equal(0x00, framebuffer[i]);
    }
}

static void test__oled_clear__should__not_draw_outside_framebuffer(void **state)
{
    oled_clear();

    assert_no_drawing_outside_framebuffer();
}

static void test__oled_blit__should__not_draw_outside_framebuffer(void **state)
{
    int w = 8;
    int h = 8;
    uint8_t *icon = icon8x8;

    draw_icon_around_point(0, 0, w, h, icon);
    draw_icon_around_point(OLED_WIDTH/2, 0, w, h, icon);
    draw_icon_around_point(OLED_WIDTH, 0, w, h, icon);

    draw_icon_around_point(0, OLED_HEIGHT / 2, w, h, icon);
    draw_icon_around_point(OLED_WIDTH/2, OLED_HEIGHT / 2, w, h, icon);
    draw_icon_around_point(OLED_WIDTH, OLED_HEIGHT / 2, w, h, icon);

    draw_icon_around_point(0, OLED_HEIGHT, w, h, icon);
    draw_icon_around_point(OLED_WIDTH/2, OLED_HEIGHT, w, h, icon);
    draw_icon_around_point(OLED_WIDTH, OLED_HEIGHT, w, h, icon);

    assert_no_drawing_outside_framebuffer();
}

static void test__oled_blit__should__draw_small_icon_in_the_correct_spot(void **state)
{
    int w = 8;
    int h = 8;
    uint8_t *icon = icon8x8;

    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(0, 0, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH/2, 0, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH, 0, w, h, icon);

    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(0, OLED_HEIGHT / 2, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH/2, OLED_HEIGHT / 2, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH, OLED_HEIGHT / 2, w, h, icon);

    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(0, OLED_HEIGHT, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH/2, OLED_HEIGHT, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH, OLED_HEIGHT, w, h, icon);
}

static void test__oled_blit__should__draw_large_icon_in_the_correct_spot(void **state)
{
    int w = 17;
    int h = 17;
    uint8_t *icon = icon17x17;

    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(0, 0, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH/2, 0, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH, 0, w, h, icon);

    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(0, OLED_HEIGHT / 2, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH/2, OLED_HEIGHT / 2, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH, OLED_HEIGHT / 2, w, h, icon);

    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(0, OLED_HEIGHT, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH/2, OLED_HEIGHT, w, h, icon);
    test__oled_blit__should__draw_icon_in_the_correct_spot_helper(OLED_WIDTH, OLED_HEIGHT, w, h, icon);
}

const struct CMUnitTest tests_for_oled[] = {
    cmocka_unit_test_setup(test__oled_clear__should__clear_framebuffer, setup),
    cmocka_unit_test_setup(test__oled_clear__should__not_draw_outside_framebuffer, setup),
    cmocka_unit_test_setup(test__oled_blit__should__not_draw_outside_framebuffer, setup),
    cmocka_unit_test_setup(test__oled_blit__should__draw_small_icon_in_the_correct_spot, setup),
    cmocka_unit_test_setup(test__oled_blit__should__draw_large_icon_in_the_correct_spot, setup),
};

//////// Main //////////////////////////////////////////////////////////////////

int main(void)
{
    int fails = 0;
    fails += cmocka_run_group_tests(tests_for_oled, NULL, NULL);

    return fails;
}
