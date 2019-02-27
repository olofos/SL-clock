#include <string.h>
#include <time.h>
#include <esp_common.h>
#include <freertos/semphr.h>

#include "i2c-master.h"
#include "display.h"
#include "matrix_framebuffer.h"
#include "matrix_display.h"
#include "fonts.h"
#include "journey.h"
#include "status.h"
#include "log.h"

#include "../avr/avr-i2c-led-matrix.h"

#define LOG_SYS LOG_SYS_DISPLAY

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

#define STATE_DISPLAY 0
#define STATE_SHIFT_OUT 1
#define STATE_SHIFT_IN 2
#define STATE_NO_DISPLAY 3

#define Y_CLOCK 7
#define Y_JOURNEY_1 19
#define Y_JOURNEY_2 28

#define X_ICON 1
#define X_TIME 12

static int16_t journey_shift[2];

static struct icon *journey_icons[6];

struct animation
{
    int16_t *variable;
    portTickType start_tick;
    int16_t start;
    int16_t end;
    uint8_t tick_rate_shift;
};

struct journey_display_state
{
    time_t current;
    time_t next;
    int16_t shift;
    uint8_t state;
    uint8_t y;

    struct animation anim;
};


static struct animation journey_1_shift_out = { .variable = &journey_shift[0], .start = 0, .end = 32, .tick_rate_shift = 4 };
static struct animation journey_1_shift_in = { .variable = &journey_shift[0], .start = 32, .end = 0, .tick_rate_shift = 4 };

static struct journey_display_state journey_display_states[2] = { { .y = Y_JOURNEY_1 }, { .y = Y_JOURNEY_2 } };


static void animation_start(struct animation *anim)
{
    anim->start_tick = xTaskGetTickCount();
    *anim->variable = anim->start;
}

static int animation_run(struct animation *anim)
{
    if(anim->start < anim->end) {
        *anim->variable = anim->start + (xTaskGetTickCount() - anim->start_tick) / anim->tick_rate_shift;
        if(*anim->variable >= anim->end) {
            *anim->variable = anim->end;
            return 1;
        }
    } else {
        *anim->variable = anim->start - (xTaskGetTickCount() - anim->start_tick) / anim->tick_rate_shift;
        if(*anim->variable <= anim->end) {
            *anim->variable = anim->end;
            return 1;
        }
    }
    return 0;
}

static uint8_t animation_running;

static void update_journey_display_state(struct journey_display_state *state, const time_t *next)
{
    switch(state->state)
    {
    case STATE_DISPLAY:
        if(*next != state->current) {
            LOG("current = %ld, next = %ld", state->current, *next);
            state->state = STATE_SHIFT_OUT;
            state->next = *next;
            state->anim.variable = &state->shift;
            state->anim.start = 0;
            state->anim.end = 32;
            state->anim.tick_rate_shift = 4;

            animation_start(&state->anim);
            animation_running++;
        }
        break;

    case STATE_SHIFT_OUT:
        if(animation_run(&state->anim)) {
            state->state = STATE_SHIFT_IN;
            state->current = state->next;
            state->anim.start = 32;
            state->anim.end = 0;

            animation_start(&state->anim);
        }
        break;

    case STATE_SHIFT_IN:
        if(animation_run(&state->anim)) {
            state->state = STATE_DISPLAY;
            animation_running--;
        }
    }
}


uint16_t matrix_intensity_low[AVR_I2C_NUM_LEVELS] = { 0, 8, 16, 32, 64, 128, 256, 512 };
uint16_t matrix_intensity_high[AVR_I2C_NUM_LEVELS] = { 12, 23, 46, 91, 182, 363, 725, 1023 };
uint8_t matrix_intensity_override = 0;
uint8_t matrix_intensity_override_level = 0;
uint8_t matrix_intensity_updated;

uint16_t matrix_intensity_adc;
uint8_t matrix_intensity_level;


xSemaphoreHandle matrix_intensity_mutex = NULL;

static void send_intensity(uint8_t command, uint16_t *tab)
{
    while(i2c_start(MATRIX_I2C_ADDRESS, I2C_WRITE) != I2C_ACK) {
        LOG("No ACK");
        vTaskDelayMs(5);
    }

    i2c_write_byte(command);
    for(int i = 0; i < AVR_I2C_NUM_LEVELS; i++) {
        i2c_write_byte(tab[i] & 0xFF);
        i2c_write_byte((tab[i] >> 8) & 0xFF);
    }

    i2c_stop();
}

static void send_fixed_intensity(uint8_t command, uint8_t level)
{
    while(i2c_start(MATRIX_I2C_ADDRESS, I2C_WRITE) != I2C_ACK) {
        LOG("No ACK");
        vTaskDelayMs(5);
    }

    i2c_write_byte(command);
    for(int i = 0; i < level; i++) {
        i2c_write_byte(0);
        i2c_write_byte(0);
    }
    for(int i = level; i < AVR_I2C_NUM_LEVELS; i++) {
        i2c_write_byte(0xFF);
        i2c_write_byte(0x03);
    }
    i2c_stop();
}

void matrix_display_main(void)
{
    matrix_init();

    matrix_intensity_mutex = xSemaphoreCreateMutex();

    fb_blit = matrix_blit;

    journey_icons[TRANSPORT_MODE_UNKNOWN] = 0;
    journey_icons[TRANSPORT_MODE_BUS] = fb_load_icon_pbm("/icons-small/bus.pbm");
    journey_icons[TRANSPORT_MODE_METRO] = fb_load_icon_pbm("/icons-small/subway.pbm");
    journey_icons[TRANSPORT_MODE_TRAIN] = fb_load_icon_pbm("/icons-small/train.pbm");
    journey_icons[TRANSPORT_MODE_TRAM] = fb_load_icon_pbm("/icons-small/tram.pbm");
    journey_icons[TRANSPORT_MODE_SHIP] = fb_load_icon_pbm("/icons-small/boat.pbm");

    int anim = 0;
    animation_start(&journey_1_shift_out);

    char buf[6];
    for(;;) {
        matrix_clear();

        fb_set_pen(FB_NORMAL);

        time_t now = time(0);
        time_t *t = &now;

        if(!(app_status.obtained_time && app_status.obtained_tz)) {
            now = 0;
        }

        if(*t) {
            strftime(buf, sizeof(buf), "%H:%M", localtime(t));
            fb_draw_string(1, Y_CLOCK, buf, 0, font_6x12, FB_ALIGN_CENTER_V);
        }


        if(anim == 0) {
            if(animation_run(&journey_1_shift_out)) {
                animation_start(&journey_1_shift_in);
                anim = 1;
            }
        } else {
            if(animation_run(&journey_1_shift_in)) {
                animation_start(&journey_1_shift_out);
                anim = 0;
            }
        }

        for(int i = 0; i < 2; i++) {
            update_journey_display_state(&journey_display_states[i], &journies[i].departures[0]);

            if(journey_display_states[i].current) {
                strftime(buf, sizeof(buf), "%H:%M", localtime(&journey_display_states[i].current));
            } else {
                sprintf(buf, "--:--");
            }
            fb_draw_string(X_TIME + journey_display_states[i].shift, journey_display_states[i].y, buf, 0, font_3x5, FB_ALIGN_CENTER_V);
            fb_draw_icon(X_ICON + journey_display_states[i].shift, journey_display_states[i].y, journey_icons[journies[i].mode], FB_ALIGN_CENTER_V);
        }

        if(i2c_start(MATRIX_I2C_ADDRESS, I2C_READ) != I2C_ACK) {
            LOG("Reading ADC: No ACK");
        } else {

            uint8_t lo = i2c_read_byte(I2C_ACK);
            uint8_t hi = i2c_read_byte(I2C_ACK);
            uint8_t intensity = i2c_read_byte(I2C_NACK);
            uint16_t adc = (((uint16_t)hi)<<8)+((uint16_t)lo);

            if((matrix_intensity_mutex != NULL) && (xSemaphoreTake(matrix_intensity_mutex, 0) == pdTRUE)) {
                matrix_intensity_adc = adc;
                matrix_intensity_level = intensity;
                xSemaphoreGive(matrix_intensity_mutex);
            }

            LOG("ADC: %-4d Intensity: %d", adc, intensity);

        }
        i2c_stop();

        if(matrix_intensity_updated) {
            if((matrix_intensity_mutex != NULL) && (xSemaphoreTake(matrix_intensity_mutex, 0) == pdTRUE)) {
                if(matrix_intensity_override && (matrix_intensity_override_level < AVR_I2C_NUM_LEVELS)) {
                    send_fixed_intensity(AVR_I2C_CMD_INTENSITY_LOW, matrix_intensity_override_level+1);
                    send_fixed_intensity(AVR_I2C_CMD_INTENSITY_HIGH, matrix_intensity_override_level);
                } else {
                    send_intensity(AVR_I2C_CMD_INTENSITY_LOW, matrix_intensity_low);
                    send_intensity(AVR_I2C_CMD_INTENSITY_HIGH, matrix_intensity_high);
                }

                matrix_intensity_updated = 0;

                xSemaphoreGive(matrix_intensity_mutex);
            }
        }

        while(i2c_start(MATRIX_I2C_ADDRESS, I2C_WRITE) != I2C_ACK) {
            LOG("No ACK");
            vTaskDelayMs(5);
        }

        i2c_write_byte(AVR_I2C_CMD_FRAMEBUFFER);
        i2c_write_lsb_first(framebuffer, 128 );
        i2c_stop();

        if(animation_running) {
            vTaskDelayMs(25);
        } else {
            vTaskDelayMs(250);
        }
    }
}
