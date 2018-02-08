#include <string.h>
#include <time.h>
#include <esp_common.h>

#include "brzo_i2c.h"
#include "ssd1306.h"
#include "framebuffer.h"
#include "fonts.h"
#include "display.h"
#include "journey.h"
#include "status.h"

#include "log.h"
#define LOG_SYS LOG_SYS_DISPLAY

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

#define STATE_DISPLAY 0
#define STATE_SHIFT_OUT 1
#define STATE_SHIFT_IN 2

#define BLINK 1
#define NOBLINK 0

struct journey_display_state
{
    time_t curr;
    time_t next;
    int16_t x_shift;
    int16_t y_shift;
    uint8_t state;

    portTickType anim_start;

    struct icon *icon;
};

static struct icon *clock_icon;
static struct icon *noclock_icon;
static struct icon *nowifi_icons[4];
static struct icon *journey_icons[2];

static struct journey_display_state journey_display_states[2];


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
        if(*next != state->curr)
        {
            state->state = STATE_SHIFT_OUT;
            state->anim_start = xTaskGetTickCount();
            state->next = *next;
        }
        break;

    case STATE_SHIFT_OUT:

        state->x_shift = (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift >= 128)
        {
            state->x_shift = 128;
            state->state = STATE_SHIFT_IN;
            state->anim_start = xTaskGetTickCount();
            state->curr = state->next;
        }
        break;

    case STATE_SHIFT_IN:
        state->x_shift = 128 - (xTaskGetTickCount() - state->anim_start);

        if(state->x_shift <= 0)
        {
            state->x_shift = 0;
            state->state = STATE_DISPLAY;
        }
    }
}

static void display_init()
{
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

    journey_display_states[0] = (struct journey_display_state) { .x_shift = 0, .y_shift = 35, .state = STATE_DISPLAY, .curr = 0, .next = 0, .icon = journey_icons[0] };
    journey_display_states[1] = (struct journey_display_state) { .x_shift = 0, .y_shift = 56, .state = STATE_DISPLAY, .curr = 0, .next = 0, .icon = journey_icons[1] };
}

static void draw_journey_row(int16_t x, struct journey_display_state *state)
{
    draw_row(x + state->x_shift, state->y_shift, state->icon, &state->curr, 0);
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

        uint32_t n = xTaskGetTickCount() / 33;

        draw_row(x, 10, nowifi_icons[n & 0x03], &now, BLINK);
    } else if(!(app_status.obtained_time && app_status.obtained_tz)) {
        time_t now = 0;
        draw_row(x, 10, noclock_icon, &now, BLINK);
    } else {
        time_t now = time(0);
        draw_row(x, 10, clock_icon, &now, BLINK);
    }
}

void display_task(void *pvParameters)
{
    display_init();

    const int16_t x_base = 20;

    for(;;)
    {
        for(int i = 0; i < 2; i++)
        {
            update_state(&journey_display_states[i], &journies[i].departures[journies[i].next_departure]);
        }

        fb_clear();

        fb_set_pen(FB_NORMAL);

        draw_clock_row(x_base);

        for(int i = 0; i < 2; i++)
        {
            draw_journey_row(x_base, &journey_display_states[i]);
        }

        fb_display();
        vTaskDelayMs(25);
    }
}
