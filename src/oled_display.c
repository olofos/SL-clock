#include <string.h>
#include <time.h>
#include <esp_common.h>

#include "i2c-master.h"

#include "sh1106.h"
#include "oled_framebuffer.h"
#include "fonts.h"
#include "display.h"
#include "display-message.h"
#include "status.h"
#include "humidity.h"

#include "log.h"
#define LOG_SYS LOG_SYS_DISPLAY

#define MS_TO_TICKS(ms)  ((ms)/portTICK_RATE_MS)
#define vTaskDelayMs(ms) vTaskDelay(MS_TO_TICKS(ms))

enum {
    STATE_MSG_DISPLAY,
    STATE_MSG_SHIFT_OUT,
    STATE_MSG_SHIFT_IN,
    STATE_MSG_NO_DISPLAY,
};

enum {
    STATE_INIT = 0,
    STATE_CLOCK = 0x10,
    STATE_CLOCK_IN = 0x11,
    STATE_CLOCK_OUT = 0x12,
    STATE_MEASUREMENT = 0x20,
    STATE_MEASUREMENT_IN = 0x21,
    STATE_MEASUREMENT_OUT = 0x22,

    STATE_SHIFT_IN = 0x01,
    STATE_SHIFT_OUT = 0x02,
};

const struct measurement *current_measurement;
int state = STATE_INIT;
portTickType state_start;



struct display_state {
    struct measurement *measurement;
    portTickType start;
    portTickType duration;
    uint8_t state;
};

struct message_state
{
    uint8_t state;

    portTickType anim_start;

    int16_t y;

    enum display_message current;
    enum display_message next;
};

static uint8_t animation_running;

static struct icon *clock_icon;
static struct icon *noclock_icon;
static struct icon *nowifi_icons[4];

static struct message_state message_state = { .state = STATE_MSG_NO_DISPLAY, .current = DISPLAY_MESSAGE_NONE, .next = DISPLAY_MESSAGE_NONE, .y = 2*OLED_HEIGHT };
static struct display_state display_state = { .state = STATE_INIT, .measurement = 0, .start = 0, .duration = 0};

static void display_init()
{
    i2c_master_init();

    while(sh1106_init() !=0)
    {
        LOG("SH1106 is not responding");
        vTaskDelayMs(250);
    }
    oled_clear();
    oled_splash();
    oled_display();

    clock_icon = fb_load_icon_pbm("/icons/clock.pbm");
    noclock_icon = fb_load_icon_pbm("/icons/noclock.pbm");

    nowifi_icons[0] = fb_load_icon_pbm("/icons/nowifi1.pbm");
    nowifi_icons[1] = fb_load_icon_pbm("/icons/nowifi2.pbm");
    nowifi_icons[2] = fb_load_icon_pbm("/icons/nowifi3.pbm");
    nowifi_icons[3] = fb_load_icon_pbm("/icons/nowifi4.pbm");
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
    case STATE_MSG_DISPLAY:
        if(state->next != state->current) {
            state->state = STATE_MSG_SHIFT_OUT;
            state->anim_start = xTaskGetTickCount();

            animation_running++;
        }
        break;

    case STATE_MSG_SHIFT_IN:
        state->y = 2*OLED_HEIGHT - (xTaskGetTickCount() - state->anim_start) / 2;

        if(state->y <= OLED_HEIGHT/2) {
            state->y = OLED_HEIGHT/2;
            state->state = STATE_MSG_DISPLAY;
            animation_running--;
        }
    break;

    case STATE_MSG_SHIFT_OUT:
        state->y = OLED_HEIGHT/2 - (xTaskGetTickCount() - state->anim_start) / 2;

        if(state->y <= -OLED_HEIGHT) {
            state->state = STATE_MSG_NO_DISPLAY;
            animation_running--;

            state->current = DISPLAY_MESSAGE_NONE;
        }

        break;

     case STATE_MSG_NO_DISPLAY:
         if(state->next != DISPLAY_MESSAGE_NONE) {
             state->state = STATE_MSG_SHIFT_IN;
             state->anim_start = xTaskGetTickCount();
             state->y = 2*OLED_HEIGHT;
             animation_running++;

             state->current = state->next;
         }
         break;
    }

    if(message_state.state != STATE_MSG_NO_DISPLAY) {
        draw_message_box(OLED_WIDTH/2, state->y, display_get_message(state->current), ArialMT_Plain_10);
    }
}

void display_measurement(const struct measurement *m, int frame)
{
    char buf[64];

    if(m->name) {
        fb_draw_string(64, 0 - frame, m->name, 0, ArialMT_Plain_24, FB_ALIGN_CENTER_H);
    } else {
        snprintf(buf, sizeof(buf), "#%d", m->node_id);
        fb_draw_string(64, 0 - frame, buf, 0, ArialMT_Plain_24, FB_ALIGN_CENTER_H);
    }
    snprintf(buf, sizeof(buf), "%.1f\260C", m->temperature);
    fb_draw_string(32 - frame, 28, buf, 0, ArialMT_Plain_16, FB_ALIGN_CENTER_H);

    snprintf(buf, sizeof(buf), "%.1f%%", m->humidity);
    fb_draw_string(32 - frame, 48, buf, 0, ArialMT_Plain_16, FB_ALIGN_CENTER_H);

    snprintf(buf, sizeof(buf), "%.2fV", m->battery1_level);
    fb_draw_string(96 + frame, 28, buf, 0, ArialMT_Plain_16, FB_ALIGN_CENTER_H);

    snprintf(buf, sizeof(buf), "%.2fV", m->battery2_level);
    fb_draw_string(96 + frame, 48, buf, 0, ArialMT_Plain_16, FB_ALIGN_CENTER_H);
}

void display_clock(int frame)
{
    char time_str[6] = "--:--";
    char date_str[12] = "";
    time_t now;

    if(app_status.obtained_time && app_status.obtained_tz) {
        now = time(0);

        if(now & 0x01) {
            strftime(time_str, sizeof(time_str), "%H %M", localtime(&now));
        } else {
            strftime(time_str, sizeof(time_str), "%H:%M", localtime(&now));
        }
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&now));
    }

    fb_draw_string(64, 24 - frame, time_str, 0, ArialMT_Plain_24, FB_ALIGN_CENTER_H | FB_ALIGN_CENTER_V);
    fb_draw_string(64, 48 + frame, date_str, 0, ArialMT_Plain_16, FB_ALIGN_CENTER_H | FB_ALIGN_CENTER_V);
}

#define SHIFT_DURATION 32

void draw_display(void)
{
    int frame = 0;

    if(display_state.state & STATE_SHIFT_IN) {
        frame = 64 - (xTaskGetTickCount() - display_state.start) * (64 / SHIFT_DURATION);
        if(frame < 0) {
            frame = 0;
        }
    } else if(display_state.state & STATE_SHIFT_OUT) {
        frame = (xTaskGetTickCount() - display_state.start) * (64 / SHIFT_DURATION);
        if(frame > 64) {
            frame = 64;
        }
    }

    if(display_state.state & STATE_CLOCK) {
        display_clock(frame);
    } else if(display_state.state & STATE_MEASUREMENT) {
        display_measurement(display_state.measurement, frame);
    } else {
        oled_splash();
    }
}

void update_state(void)
{
    if((display_state.duration > 0) && ((xTaskGetTickCount() - display_state.start) < display_state.duration)) {
        return;
    }

    switch(display_state.state) {
    case STATE_INIT: {
        if(app_status.obtained_time && app_status.obtained_tz) {
            display_state.state = STATE_CLOCK_IN;
            display_state.duration = SHIFT_DURATION;
            display_state.measurement = 0;
            animation_running++;
        }
        break;
    }

    case STATE_CLOCK_IN: {
        display_state.state = STATE_CLOCK;
        display_state.duration = MS_TO_TICKS(10000);
        animation_running--;
        break;
    }

    case STATE_CLOCK: {
        if(!humidity_take_mutex_noblock()) {
            return;
        }

        if(humidity_first_measurement) {
            display_state.state = STATE_CLOCK_OUT;
            display_state.duration = SHIFT_DURATION;
            display_state.measurement = humidity_first_measurement;
            animation_running++;
        } else {
            humidity_give_mutex();
            return;
        }
        break;
    }

    case STATE_CLOCK_OUT: {
        display_state.state = STATE_MEASUREMENT_IN;
        display_state.duration = SHIFT_DURATION;
        break;
    }

    case STATE_MEASUREMENT_IN: {
        display_state.state = STATE_MEASUREMENT;
        display_state.duration = MS_TO_TICKS(5000);
        animation_running--;
        break;
    }

    case STATE_MEASUREMENT: {
        display_state.state = STATE_MEASUREMENT_OUT;
        display_state.duration = SHIFT_DURATION;
        animation_running++;
        break;
    }

    case STATE_MEASUREMENT_OUT: {
        display_state.measurement = display_state.measurement->next;
        display_state.duration = SHIFT_DURATION;
        if(display_state.measurement) {
            display_state.state = STATE_MEASUREMENT_IN;
        } else {
            humidity_give_mutex();
            display_state.state = STATE_CLOCK_IN;
        }
        break;
    }

    }
    display_state.start = xTaskGetTickCount();
}

void oled_display_main(void)
{
    display_init();

    for(;;)
    {
        oled_clear();

        fb_set_pen(FB_NORMAL);

        update_state();
        draw_display();

        if(message_state.current == message_state.next) {
            uint8_t message;
            if(xQueueReceive(display_message_queue, &message, 0)) {
                message_state.next = message;
            }
        }

        show_message_animation();

        oled_display();

        if(animation_running) {
            vTaskDelayMs(20);
        } else {
            vTaskDelayMs(200);
        }
    }
}
