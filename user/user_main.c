#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
//#include <apps/sntp.h>
//#include <apps/sntp_time.h>
#include <esp_common.h>
#include <uart.h>

#include "http-client.h"
#include "timezone-db.h"
#include "journey.h"
#include "journey-task.h"
#include "sntp.h"
#include "keys.h"
#include "cbuf.h"
#include "console.h"
#include "i2c-master.h"
#include "ssd1306.h"
#include "log.h"

#define LOG_SYS LOG_SYS_MAIN


#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

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



void display_task(void *pvParameters)
{
    i2c_master_init();

    ssd1306_init();
    ssd1306_splash();

    vTaskDelayMs(3000);

    ssd1306_clear();

    uint8_t n = 0;
    for(;;)
    {
        char str[22];
        char tstr[9];
        time_t now = time(0);

        strftime(tstr, sizeof(tstr), "%H:%M:%S", localtime(&now));
        sprintf(str, "%s", tstr);
        ssd1306_puts(str, 0, 0);

        for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
        {
            struct journey *journey = &journies[i];
            time_t t = journey->departures[journey->next_departure];
            if(t > 0)
            {

                strftime(tstr, sizeof(tstr), "%H:%M:%S", localtime(&t));
            } else {
                strcpy(tstr,"No departure");
            }

            int n = sprintf(str, "%s: %s", journey->line, tstr);
            for(; n < sizeof(str)-1; n++)
            {
                str[n] = ' ';
            }
            str[sizeof(str)-1] = 0;

            ssd1306_puts(str,0,i+1);
        }

        vTaskDelayMs(500);
    }
}

void user_init(void)
{
    uart_init_new();
    UART_SetBaudrate(0, 115200);
    UART_SetPrintPort(0);

    LOG("SDK version:%s\n", system_get_sdk_version());

    struct station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    wifi_set_opmode(STATION_MODE);
    wifi_station_set_config(&config);

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
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


    xTaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
    xTaskCreate(&sntp_task, "sntp_task", 384, NULL, 6, NULL);
    xTaskCreate(&timezone_db_task, "timezone_db_task", 512, NULL, 5, NULL);
    xTaskCreate(&journey_task, "journey_task", 1024, NULL, 4, NULL);
}
