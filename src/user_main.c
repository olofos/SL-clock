#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <esp_common.h>

#include "uart.h"
#include "brzo_i2c.h"
#include "http-client.h"
#include "timezone-db.h"
#include "journey.h"
#include "journey-task.h"
#include "sntp.h"
#include "keys.h"
#include "ssd1306.h"
#include "fonts.h"
#include "framebuffer.h"
#include "status.h"
#include "wifi-task.h"

#include "icon-boat-large.h"
#include "icon-bus-large.h"
#include "icon-clock-large.h"
#include "icon-noclock-large.h"
#include "icon-nowifi1-large.h"
#include "icon-nowifi2-large.h"
#include "icon-nowifi3-large.h"
#include "icon-nowifi4-large.h"

#include "log.h"

#define LOG_SYS LOG_SYS_MAIN


#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

struct app_status app_status = {
    .wifi_connected = 0,
    .obtained_time = 0,
    .obtained_tz = 0,
};

uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/

void test_task(void *pvParameters)
{
    for(;;)
    {
        LOG("%ld", xTaskGetTickCount());
        vTaskDelayMs(5000);
    }
}

void journey_test_task(void *pvParameters)
{
    for(;;)
    {
        for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
        {
            if(!(rand() & 0x03))
            {
                journies[i].next_departure = 0;
                journies[i].departures[0] += 300;
            }
        }

        vTaskDelayMs(5000);
    }
}

#define STATE_DISPLAY 0
#define STATE_SHIFT_OUT 1
#define STATE_SHIFT_IN 2



void draw_row(int16_t x, int16_t y, const struct icon *icon, const time_t *t, int blink)
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
    fb_draw_string(x + 55, y, str, Monospaced_bold_16, FB_ALIGN_CENTER_V);
}


struct journey_display_state
{
    time_t curr;
    time_t next;
    int16_t x_shift;
    int16_t y_shift;
    uint8_t state;

    portTickType anim_start;
};

void update_state(struct journey_display_state *state, const time_t *t)
{
    switch(state->state)
    {
    case STATE_DISPLAY:
        if(*t != state->curr)
        {
            state->state = STATE_SHIFT_OUT;
            state->anim_start = xTaskGetTickCount();
            state->next = *t;
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

void display_task(void *pvParameters)
{
    brzo_i2c_setup(200);

    while(ssd1306_init() !=0)
    {
        LOG("SSD1306 is not responding");
        vTaskDelayMs(250);
    }
    fb_splash();

    const struct icon clock_icon = { .width = ICON_CLOCK_LARGE_WIDTH, .height = ICON_CLOCK_LARGE_HEIGHT, .data = icon_clock_large, };
    const struct icon noclock_icon = { .width = ICON_NOCLOCK_LARGE_WIDTH, .height = ICON_NOCLOCK_LARGE_HEIGHT, .data = icon_noclock_large, };
    const struct icon nowifi_icons[] = {
        { .width = ICON_NOWIFI1_LARGE_WIDTH, .height = ICON_NOWIFI1_LARGE_HEIGHT, .data = icon_nowifi1_large, },
        { .width = ICON_NOWIFI2_LARGE_WIDTH, .height = ICON_NOWIFI2_LARGE_HEIGHT, .data = icon_nowifi2_large, },
        { .width = ICON_NOWIFI3_LARGE_WIDTH, .height = ICON_NOWIFI3_LARGE_HEIGHT, .data = icon_nowifi3_large, },
        { .width = ICON_NOWIFI4_LARGE_WIDTH, .height = ICON_NOWIFI4_LARGE_HEIGHT, .data = icon_nowifi4_large, },
    };

    const struct icon journey_icons[2] = {
        { .width = ICON_BOAT_LARGE_WIDTH, .height = ICON_BOAT_LARGE_HEIGHT, .data = icon_boat_large, },
        { .width = ICON_BUS_LARGE_WIDTH, .height = ICON_BUS_LARGE_HEIGHT, .data = icon_bus_large, },
    };

    struct journey_display_state states[2] = {
        { .x_shift = 0, .y_shift = 35, .state = STATE_DISPLAY },
        { .x_shift = 0, .y_shift = 56, .state = STATE_DISPLAY },
    };

    for(int i = 0; i < 2; i++)
    {
        states[i].curr = journies[0].departures[journies[0].next_departure];
        states[i].next = journies[0].departures[journies[0].next_departure];
    }

    for(;;)
    {
        for(int i = 0; i < 2; i++)
        {
            update_state(&states[i], &journies[i].departures[journies[i].next_departure]);
        }

        fb_clear();

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

            draw_row(20, 10, &nowifi_icons[n & 0x03], &now, 1);
        } else if(!(app_status.obtained_time && app_status.obtained_tz)) {
            time_t now = 0;
            draw_row(20, 10, &noclock_icon, &now, 1);
        } else {
            time_t now = time(0);
            draw_row(20, 10, &clock_icon, &now, 1);
        }

        for(int i = 0; i < 2; i++)
        {
            draw_row(20 + states[i].x_shift, states[i].y_shift, &journey_icons[i], &states[i].curr, 1);
        }

        fb_display();
        vTaskDelayMs(25);
    }
}

void user_init(void)
{
    uart_init_new();
    UART_SetBaudrate(0, 115200);
    UART_SetPrintPort(0);

    LOG("SDK version:%s\n", system_get_sdk_version());

    //setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    setenv("TZ", "GMT0", 1);
    tzset();

    struct journey journies[] = {
        {
            .line = "80",
            .stop = "Saltsjöqvarn",
            .destination = "Nybroplan",
            .site_id = 1442,
            .mode = TRANSPORT_MODE_SHIP,
            .direction = 2,
        },
        {
            .line = "53",
            .stop = "Henriksdalsberget",
            .destination = "Karolinska institutet",
            .site_id = 1450,
            .mode = TRANSPORT_MODE_BUS,
            .direction = 2,
        }
    };

    for(int i = 0; i < sizeof(journies)/sizeof(journies[0]); i++)
    {
        journey_set_journey(i, &journies[i]);
    }

    http_mutex = xSemaphoreCreateMutex();


    xTaskCreate(&wifi_task, "wifi_task", 384, NULL, 6, NULL);
    xTaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
    xTaskCreate(&sntp_task, "sntp_task", 384, NULL, 6, NULL);
    xTaskCreate(&timezone_db_task, "timezone_db_task", 512, NULL, 5, NULL);
    xTaskCreate(&journey_task, "journey_task", 1024, NULL, 4, NULL);

    // xTaskCreate(&test_task, "test_task", 384, NULL, 6, NULL);
    // xTaskCreate(&journey_test_task, "journey_test_task", 1024, NULL, 4, NULL);
}
