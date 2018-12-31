#include <string.h>
#include <time.h>
#include <esp_common.h>

#include "i2c-master.h"

#include "sh1106.h"
#include "oled_framebuffer.h"
#include "fonts.h"
#include "display.h"
#include "display-message.h"
#include "journey.h"
#include "status.h"

#include "log.h"
#define LOG_SYS LOG_SYS_DISPLAY

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

#define STATE_DISPLAY 0
#define STATE_SHIFT_OUT 1
#define STATE_SHIFT_IN 2
#define STATE_NO_DISPLAY 3

#define STATE_SINGLE_DISPLAY 0
#define STATE_SINGLE_SHIFT_BOTH_OUT 1
#define STATE_SINGLE_SHIFT_BOTH_IN 2
#define STATE_SINGLE_SHIFT_ONE_OUT 3
#define STATE_SINGLE_SHIFT_ICON_UP_OUT 4
#define STATE_SINGLE_SHIFT_JOURNEY_UP 5
#define STATE_SINGLE_SHIFT_ONE_IN 6
#define STATE_SINGLE_SHIFT_ICON_UP_IN 7

#define BLINK 1
#define NOBLINK 0

#define X_OFFSET 20
#define X_OFFSET_LINE 40

#define Y_CLOCK 10
#define Y_JOURNEY_1 32
#define Y_JOURNEY_2 54

struct journey_display_state
{
    time_t current;
    time_t next;
    int16_t x_shift;
    int16_t y_shift;
    uint8_t state;

    portTickType anim_start;

    struct icon *icon;
};

struct journey_single_display_state
{
    time_t current[2];
    time_t next[2];
    int16_t x_shift;
    int16_t y_shift;
    uint8_t state;

    portTickType anim_start;

    struct icon *icon;
};

struct message_state
{
    uint8_t state;

    portTickType anim_start;

    int16_t y;

    enum display_message current;
    enum display_message next;
};

static struct icon *clock_icon;
static struct icon *noclock_icon;
static struct icon *nowifi_icons[4];
static struct icon *journey_icons[6];

static struct journey_display_state journey_display_states[2];
struct journey_single_display_state journey_single_display_state;

static uint8_t animation_running;

static struct message_state message_state = { .state = STATE_NO_DISPLAY, .current = DISPLAY_MESSAGE_NONE, .next = DISPLAY_MESSAGE_NONE, .y = 2*OLED_HEIGHT };

xQueueHandle display_message_queue;

static void draw_row(int16_t x, int16_t y, const struct icon *icon, const time_t *t, int blink)
{
    char str[6];

    if(*t)
    {
        if(blink && (*t & 0x01))
        {
            strftime(str, sizeof(str), "%H %M", localtime(t));
        } else {
            strftime(str, sizeof(str), "%H:%M", localtime(t));
        }
    } else {
        sprintf(str, "--:--");
    }

    if(icon) {
        fb_draw_icon(X_OFFSET + x, y, icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
    }

    fb_draw_string(X_OFFSET + x + 55, y, str, 0, Monospaced_bold_16, FB_ALIGN_CENTER_V);
}

static void update_journey_display_state(struct journey_display_state *state, const time_t *next)
{
    switch(state->state)
    {
    case STATE_DISPLAY:
        if(*next != state->current)
        {
            state->state = STATE_SHIFT_OUT;
            state->anim_start = xTaskGetTickCount();
            state->next = *next;

            animation_running++;
        }
        break;

    case STATE_SHIFT_OUT:

        state->x_shift = (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift >= 128)
        {
            state->x_shift = 128;
            state->state = STATE_SHIFT_IN;
            state->anim_start = xTaskGetTickCount();
            state->current = state->next;
        }
        break;

    case STATE_SHIFT_IN:
        state->x_shift = 128 - (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift <= 0)
        {
            state->x_shift = 0;
            state->state = STATE_DISPLAY;

            animation_running--;
        }
    }
}

static void update_journey_single_display_state(struct journey_single_display_state *state, const time_t *next)
{
    switch(state->state)
    {
    case STATE_SINGLE_DISPLAY:
        if(next[0] != state->current[0]) {
            if(next[0] != state->current[1]) {
                state->state = STATE_SINGLE_SHIFT_BOTH_OUT;

                state->anim_start = xTaskGetTickCount();
                state->next[0] = next[0];
                state->next[1] = next[1];

                animation_running++;
            } else {
                state->state = STATE_SINGLE_SHIFT_ICON_UP_OUT;
                state->y_shift = (Y_JOURNEY_1 + Y_JOURNEY_2) / 2;

                state->anim_start = xTaskGetTickCount();
                state->next[0] = next[0];
                state->next[1] = next[1];

                animation_running++;
            }
        }
        break;

    case STATE_SINGLE_SHIFT_BOTH_OUT:
        state->x_shift = (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift >= 128)
        {
            state->x_shift = 128;
            state->state = STATE_SINGLE_SHIFT_BOTH_IN;
            state->anim_start = xTaskGetTickCount();
            state->current[0] = state->next[0];
            state->current[1] = state->next[1];
        }
        break;

    case STATE_SINGLE_SHIFT_BOTH_IN:
        state->x_shift = 128 - (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift <= 0)
        {
            state->x_shift = 0;
            state->state = STATE_SINGLE_DISPLAY;

            animation_running--;
        }
        break;

    case STATE_SINGLE_SHIFT_ICON_UP_OUT:
        state->y_shift = (Y_JOURNEY_1 + Y_JOURNEY_2) / 2 - (xTaskGetTickCount() - state->anim_start)/2;
        if(state->y_shift <= Y_JOURNEY_1) {
            state->x_shift = 0;
            state->state = STATE_SINGLE_SHIFT_ONE_OUT;
            state->anim_start = xTaskGetTickCount();
        }
        break;

    case STATE_SINGLE_SHIFT_ONE_OUT:
        state->x_shift = (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift >= 128)
        {
            state->y_shift = Y_JOURNEY_2;
            state->state = STATE_SINGLE_SHIFT_JOURNEY_UP;
            state->anim_start = xTaskGetTickCount();

            state->current[0] = state->next[0];
            state->current[1] = state->next[1];
        }
        break;

    case STATE_SINGLE_SHIFT_JOURNEY_UP:
        state->y_shift = Y_JOURNEY_2 - (xTaskGetTickCount() - state->anim_start)/2;
        if(state->y_shift <= Y_JOURNEY_1) {
            state->x_shift = 128;
            state->state = STATE_SINGLE_SHIFT_ONE_IN;
            state->anim_start = xTaskGetTickCount();
        }
        break;

    case STATE_SINGLE_SHIFT_ONE_IN:
        state->x_shift = 128 - (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift <= 0)
        {
            state->y_shift = Y_JOURNEY_2;
            state->state = STATE_SINGLE_SHIFT_ICON_UP_IN;
            state->anim_start = xTaskGetTickCount();
        }
        break;

    case STATE_SINGLE_SHIFT_ICON_UP_IN:
        state->y_shift = Y_JOURNEY_2 - (xTaskGetTickCount() - state->anim_start)/2;
        if(state->y_shift <= (Y_JOURNEY_1 + Y_JOURNEY_2) / 2) {
            state->state = STATE_SINGLE_DISPLAY;
            animation_running--;
        }
        break;
    }
}


static void display_init()
{
    display_message_queue = xQueueCreate(4, 1);

    i2c_master_init();

    while(sh1106_init() !=0)
    {
        LOG("SH1106 is not responding");
        vTaskDelayMs(250);
    }
    oled_splash();

    clock_icon = fb_load_icon_pbm("/icons/clock.pbm");
    noclock_icon = fb_load_icon_pbm("/icons/noclock.pbm");

    nowifi_icons[0] = fb_load_icon_pbm("/icons/nowifi1.pbm");
    nowifi_icons[1] = fb_load_icon_pbm("/icons/nowifi2.pbm");
    nowifi_icons[2] = fb_load_icon_pbm("/icons/nowifi3.pbm");
    nowifi_icons[3] = fb_load_icon_pbm("/icons/nowifi4.pbm");

    journey_icons[TRANSPORT_MODE_UNKNOWN] = 0;
    journey_icons[TRANSPORT_MODE_BUS] = fb_load_icon_pbm("/icons/bus.pbm");
    journey_icons[TRANSPORT_MODE_METRO] = fb_load_icon_pbm("/icons/subway.pbm");
    journey_icons[TRANSPORT_MODE_TRAIN] = fb_load_icon_pbm("/icons/train.pbm");
    journey_icons[TRANSPORT_MODE_TRAM] = fb_load_icon_pbm("/icons/tram.pbm");
    journey_icons[TRANSPORT_MODE_SHIP] = fb_load_icon_pbm("/icons/boat.pbm");

    journey_display_states[0] = (struct journey_display_state) { .x_shift = 0, .y_shift = Y_JOURNEY_1, .state = STATE_DISPLAY, .current = 0, .next = 0, .icon = 0 };
    journey_display_states[1] = (struct journey_display_state) { .x_shift = 0, .y_shift = Y_JOURNEY_2, .state = STATE_DISPLAY, .current = 0, .next = 0, .icon = 0 };

    journey_single_display_state = (struct journey_single_display_state) { .x_shift = 0, .y_shift = 0, .state = STATE_SINGLE_DISPLAY };
    journey_single_display_state.current[0] = 0;
    journey_single_display_state.current[1] = 0;
}

static void draw_journey_row(const struct journey_display_state *state, const struct journey *journey)
{
    draw_row(state->x_shift, state->y_shift, state->icon, &state->current, 0);
    fb_draw_string(X_OFFSET_LINE + state->x_shift, state->y_shift, journey->line, 0, font_3x5, FB_ALIGN_CENTER_V);
}

static void draw_journey_single(struct journey_single_display_state *state)
{
    switch(state->state)
    {
    case STATE_SINGLE_DISPLAY:
        draw_row(0, Y_JOURNEY_1, 0, &state->current[0], 0);
        draw_row(0, Y_JOURNEY_2, 0, &state->current[1], 0);
        fb_draw_icon(X_OFFSET, (Y_JOURNEY_1 + Y_JOURNEY_2)/2, state->icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
        fb_draw_string(X_OFFSET_LINE, (Y_JOURNEY_1 + Y_JOURNEY_2)/2, journies[0].line, 0, font_3x5, FB_ALIGN_CENTER_V);
        break;

    case STATE_SINGLE_SHIFT_BOTH_IN:
    case STATE_SINGLE_SHIFT_BOTH_OUT:
        draw_row(state->x_shift, Y_JOURNEY_1, 0, &state->current[0], 0);
        draw_row(state->x_shift, Y_JOURNEY_2, 0, &state->current[1], 0);
        fb_draw_icon(X_OFFSET + state->x_shift, (Y_JOURNEY_1 + Y_JOURNEY_2)/2, state->icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
        fb_draw_string(X_OFFSET_LINE + state->x_shift, (Y_JOURNEY_1 + Y_JOURNEY_2)/2, journies[0].line, 0, font_3x5, FB_ALIGN_CENTER_V);
        break;

    case STATE_SINGLE_SHIFT_ICON_UP_OUT:
    case STATE_SINGLE_SHIFT_ICON_UP_IN:
        draw_row(0, Y_JOURNEY_1, 0, &state->current[0], 0);
        draw_row(0, Y_JOURNEY_2, 0, &state->current[1], 0);
        fb_draw_icon(X_OFFSET, state->y_shift, state->icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
        fb_draw_string(X_OFFSET_LINE, state->y_shift, journies[0].line, 0, font_3x5, FB_ALIGN_CENTER_V);
        break;

    case STATE_SINGLE_SHIFT_ONE_OUT:
        draw_row(state->x_shift, Y_JOURNEY_1, 0, &state->current[0], 0);
        draw_row(0, Y_JOURNEY_2, 0, &state->current[1], 0);
        fb_draw_icon(X_OFFSET + state->x_shift, Y_JOURNEY_1, state->icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
        fb_draw_string(X_OFFSET_LINE + state->x_shift, Y_JOURNEY_1, journies[0].line, 0, font_3x5, FB_ALIGN_CENTER_V);
        break;

    case STATE_SINGLE_SHIFT_JOURNEY_UP:
        draw_row(0, state->y_shift, 0, &state->current[0], 0);
        break;

    case STATE_SINGLE_SHIFT_ONE_IN:
        draw_row(0, Y_JOURNEY_1, 0, &state->current[0], 0);
        draw_row(state->x_shift, Y_JOURNEY_2, 0, &state->current[1], 0);
        fb_draw_icon(X_OFFSET + state->x_shift, Y_JOURNEY_2, state->icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
        fb_draw_string(X_OFFSET_LINE + state->x_shift, Y_JOURNEY_2, journies[0].line, 0, font_3x5, FB_ALIGN_CENTER_V);
        break;
    }
}

static void draw_clock_row(void)
{
    if(!app_status.wifi_connected)
    {
        time_t now;

        if(app_status.obtained_time && app_status.obtained_tz)
        {
            now = time(0);
        } else {
            now = 0;
        }

        uint32_t n = xTaskGetTickCount() / 25;

        draw_row(0, Y_CLOCK, nowifi_icons[n & 0x03], &now, BLINK);
    } else if(!(app_status.obtained_time && app_status.obtained_tz)) {
        time_t now = 0;
        draw_row(0, Y_CLOCK, noclock_icon, &now, BLINK);
    } else {
        time_t now = time(0);
        draw_row(0, Y_CLOCK, clock_icon, &now, BLINK);
    }
}

static void draw_message_box(int16_t x, int16_t y, const char *text, const uint8_t *font_data)
{
    uint8_t num_lines = 0;
    const char *s;

    uint8_t line_len[4];

    line_len[0] = 0;

    for(uint8_t i = 0; i < strlen(text); i++) {
        if(num_lines >= sizeof(line_len)/sizeof(line_len[0])) {
            num_lines = sizeof(line_len)/sizeof(line_len[0]) - 1;
            break;
        }

        if(text[i] == '\n') {
            num_lines++;
            line_len[num_lines] = 0;
        } else {
            line_len[num_lines]++;
        }
    }
    num_lines++;

    const uint8_t line_h = font_data[FONT_HEIGHT_POS];
    const uint8_t h = num_lines * line_h;

    uint8_t w = 0;
    s = text;
    for(uint8_t i = 0; i < num_lines; i++)  {
        uint8_t lw = fb_string_length(s, line_len[i], font_data);
        if(lw > w) {
            w = lw;
        }

        s += line_len[i] + 1;
    }

    x -= w/2;
    y -= h/2;

    fb_set_pen(FB_INVERSE);
    oled_fill_rect_round(x-1-4, y-1, w+2+8, h+2);
    fb_set_pen(FB_NORMAL);
    oled_draw_rect_round(x-4, y, w+8, h);

    s = text;
    for(uint8_t i = 0; i < num_lines; i++)  {
        fb_draw_string(OLED_WIDTH/2, y + line_h * i + line_h / 2, s, line_len[i], font_data, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
        s += line_len[i] + 1;
    }
}


static void show_message_animation(void)
{
    struct message_state * state = &message_state;

    switch(state->state) {
    case STATE_DISPLAY:
        if(state->next != state->current) {
            state->state = STATE_SHIFT_OUT;
            state->anim_start = xTaskGetTickCount();

            animation_running++;
        }
        break;

    case STATE_SHIFT_IN:
        state->y = 2*OLED_HEIGHT - (xTaskGetTickCount() - state->anim_start) / 2;

        if(state->y <= OLED_HEIGHT/2) {
            state->y = OLED_HEIGHT/2;
            state->state = STATE_DISPLAY;
            animation_running--;
        }
    break;

    case STATE_SHIFT_OUT:
        state->y = OLED_HEIGHT/2 - (xTaskGetTickCount() - state->anim_start) / 2;

        if(state->y <= -OLED_HEIGHT) {
            state->state = STATE_NO_DISPLAY;
            animation_running--;

            state->current = DISPLAY_MESSAGE_NONE;
        }

        break;

     case STATE_NO_DISPLAY:
         if(state->next != DISPLAY_MESSAGE_NONE) {
             state->state = STATE_SHIFT_IN;
             state->anim_start = xTaskGetTickCount();
             state->y = 2*OLED_HEIGHT;
             animation_running++;

             state->current = state->next;
         }
         break;
    }

    if(message_state.state != STATE_NO_DISPLAY) {
        draw_message_box(OLED_WIDTH/2, state->y, display_get_message(state->current), ArialMT_Plain_10);
    }
}

void display_task(void *pvParameters)
{
    display_init();

    for(;;)
    {
        oled_clear();

        fb_set_pen(FB_NORMAL);

        draw_clock_row();

        int num_journies = 0;

        if(journies[1].line[0]) {
            num_journies = 2;
        } else if(journies[0].line[0]) {
            num_journies = 1;
        }

        if(num_journies == 2) {
            for(int i = 0; i < 2; i++) {
                journey_display_states[i].icon = journey_icons[journies[i].mode];
                update_journey_display_state(&journey_display_states[i], &journies[i].departures[0]);

                for(int i = 0; i < 2; i++) {
                    draw_journey_row(&journey_display_states[i], &journies[i]);
                }
            }
        } else if(num_journies == 1) {
            journey_single_display_state.icon = journey_icons[journies[0].mode];

            update_journey_single_display_state(&journey_single_display_state, journies[0].departures);
            draw_journey_single(&journey_single_display_state);
        }

        if(message_state.current == message_state.next) {
            uint8_t message;
            if(xQueueReceive(display_message_queue, &message, 0)) {
                message_state.next = message;
            }
        }

        show_message_animation();

        oled_display();

        if(animation_running) {
            vTaskDelayMs(25);
        } else {
            vTaskDelayMs(125);
        }
    }
}
