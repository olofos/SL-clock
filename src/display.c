#include <string.h>
#include <time.h>
#include <esp_common.h>

#include "brzo_i2c.h"
#include "ssd1306.h"
#include "framebuffer.h"
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

#define BLINK 1
#define NOBLINK 0

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
static struct icon *journey_icons[2];

static struct journey_display_state journey_display_states[2];

static uint8_t animation_running;

static struct message_state message_state = { .state = STATE_NO_DISPLAY, .current = DISPLAY_MESSAGE_NONE, .next = DISPLAY_MESSAGE_NONE, .y = 2*FB_HEIGHT };

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

    fb_draw_icon(x, y, icon, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
    fb_draw_string(x + 55, y, str, 0, Monospaced_bold_16, FB_ALIGN_CENTER_V);
}

static void update_state(struct journey_display_state *state, const time_t *next)
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


static void display_init()
{
    display_message_queue = xQueueCreate(4, 1);

    brzo_i2c_setup(200);

    while(ssd1306_init() !=0)
    {
        LOG("SSD1306 is not responding");
        vTaskDelayMs(250);
    }
    fb_splash();

    clock_icon = fb_load_icon_pbm("/icons/clock.pbm");
    noclock_icon = fb_load_icon_pbm("/icons/noclock.pbm");

    nowifi_icons[0] = fb_load_icon_pbm("/icons/nowifi1.pbm");
    nowifi_icons[1] = fb_load_icon_pbm("/icons/nowifi2.pbm");
    nowifi_icons[2] = fb_load_icon_pbm("/icons/nowifi3.pbm");
    nowifi_icons[3] = fb_load_icon_pbm("/icons/nowifi4.pbm");

    journey_icons[0] = fb_load_icon_pbm("/icons/boat.pbm");
    journey_icons[1] = fb_load_icon_pbm("/icons/bus.pbm");

    journey_display_states[0] = (struct journey_display_state) { .x_shift = 0, .y_shift = 35, .state = STATE_DISPLAY, .current = 0, .next = 0, .icon = journey_icons[0] };
    journey_display_states[1] = (struct journey_display_state) { .x_shift = 0, .y_shift = 56, .state = STATE_DISPLAY, .current = 0, .next = 0, .icon = journey_icons[1] };
}

static void draw_journey_row(int16_t x, struct journey_display_state *state)
{
    draw_row(x + state->x_shift, state->y_shift, state->icon, &state->current, 0);
}

static void draw_clock_row(int16_t x)
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

        draw_row(x, 10, nowifi_icons[n & 0x03], &now, BLINK);
    } else if(!(app_status.obtained_time && app_status.obtained_tz)) {
        time_t now = 0;
        draw_row(x, 10, noclock_icon, &now, BLINK);
    } else {
        time_t now = time(0);
        draw_row(x, 10, clock_icon, &now, BLINK);
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
    fb_fill_rect_round(x-1-4, y-1, w+2+8, h+2);
    fb_set_pen(FB_NORMAL);
    fb_draw_rect_round(x-4, y, w+8, h);

    s = text;
    for(uint8_t i = 0; i < num_lines; i++)  {
        fb_draw_string(FB_WIDTH/2, y + line_h * i + line_h / 2, s, line_len[i], font_data, FB_ALIGN_CENTER_V | FB_ALIGN_CENTER_H);
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
        state->y = 2*FB_HEIGHT - (xTaskGetTickCount() - state->anim_start) / 2;

        if(state->y <= FB_HEIGHT/2) {
            state->y = FB_HEIGHT/2;
            state->state = STATE_DISPLAY;
            animation_running--;
        }
    break;

    case STATE_SHIFT_OUT:
        state->y = FB_HEIGHT/2 - (xTaskGetTickCount() - state->anim_start) / 2;

        if(state->y <= -FB_HEIGHT) {
            state->state = STATE_NO_DISPLAY;
            animation_running--;

            state->current = DISPLAY_MESSAGE_NONE;
        }

        break;

     case STATE_NO_DISPLAY:
         if(state->next != DISPLAY_MESSAGE_NONE) {
             state->state = STATE_SHIFT_IN;
             state->anim_start = xTaskGetTickCount();
             state->y = 2*FB_HEIGHT;
             animation_running++;

             state->current = state->next;
         }
         break;
    }

    if(message_state.state != STATE_NO_DISPLAY) {
        draw_message_box(FB_WIDTH/2, state->y, display_get_message(state->current), ArialMT_Plain_10);
    }
}

void display_task(void *pvParameters)
{
    display_init();

    const int16_t x_base = 20;

    for(;;)
    {
        for(int i = 0; i < 2; i++) {
            update_state(&journey_display_states[i], &journies[i].departures[journies[i].next_departure]);
        }

        fb_clear();

        fb_set_pen(FB_NORMAL);

        draw_clock_row(x_base);

        for(int i = 0; i < 2; i++) {
            draw_journey_row(x_base, &journey_display_states[i]);
        }


        if(message_state.current == message_state.next) {
            uint8_t message;
            if(xQueueReceive(display_message_queue, &message, 0)) {
                message_state.next = message;
            }
        }

        show_message_animation();

        fb_display();

        if(animation_running) {
            vTaskDelayMs(25);
        } else {
            vTaskDelayMs(125);
        }
    }
}
