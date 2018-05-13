#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <esp_common.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "uart.h"
#include "http-client.h"
#include "timezone-db.h"
#include "journey.h"
#include "journey-task.h"
#include "sntp.h"
#include "keys.h"
#include "status.h"
#include "wifi-task.h"
#include "config.h"
#include "display.h"

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


#define FS_FLASH_SIZE (3*1024*1024-16*1024)
#define FS_FLASH_ADDR (1*1024*1024)
#define SECTOR_SIZE (4*1024)
#define LOG_BLOCK SECTOR_SIZE
#define LOG_PAGE (128)
#define FD_BUF_SIZE (32*4)
#define CACHE_BUF_SIZE (LOG_PAGE+32)*8

void spiffs_fs_init(void)
{
    struct esp_spiffs_config config;
    config.phys_size = FS_FLASH_SIZE;
    config.phys_addr = FS_FLASH_ADDR;
    config.phys_erase_block = SECTOR_SIZE;
    config.log_block_size = LOG_BLOCK;
    config.log_page_size = LOG_PAGE;
    config.fd_buf_size = FD_BUF_SIZE * 2;
    config.cache_buf_size = CACHE_BUF_SIZE;

    int ret = esp_spiffs_init(&config);

    if(ret < 0)
    {
        LOG("Could not mount SPIFFS volume (%d)", ret);
    }
}


void tz_test_task(void *pvParameters)
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

void user_init(void)
{
    uart_init_new();
    UART_SetBaudrate(0, 115200);
    UART_SetPrintPort(0);

    printf("\n");
    LOG("SDK version:%s\n", system_get_sdk_version());

    spiffs_fs_init();

    config_load("/config.json");

    setenv("TZ", "GMT0", 1);
    tzset();

    http_mutex = xSemaphoreCreateMutex();

    xTaskCreate(&wifi_task, "wifi_task", 384, NULL, 6, NULL);
    xTaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
    xTaskCreate(&sntp_task, "sntp_task", 384, NULL, 6, NULL);
    xTaskCreate(&timezone_db_task, "timezone_db_task", 512, NULL, 5, NULL);
    xTaskCreate(&journey_task, "journey_task", 1024, NULL, 4, NULL);

    // xTaskCreate(&tz_test_task, "tz_test_task", 384, NULL, 6, NULL);
    // xTaskCreate(&journey_test_task, "journey_test_task", 1024, NULL, 4, NULL);
}
