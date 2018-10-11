#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <esp_common.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <lwip/api.h>

#include "uart.h"
#include "timezone-db.h"
#include "journey.h"
#include "journey-task.h"
#include "sntp.h"
#include "keys.h"
#include "status.h"
#include "wifi-task.h"
#include "config.h"
#include "display.h"
#include "display-message.h"
#include "http-server-task.h"
#include "json.h"
#include "json-http.h"
#include "json-util.h"

#include "log.h"
#include "syslog.h"

#define LOG_SYS LOG_SYS_MAIN

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

#define TaskCreate(a,b,c,d,e,f) xTaskCreate(a,(signed char*)(b),c,d,e,f)

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
        ERROR("Could not mount SPIFFS volume (%d)", ret);
    }
}

#define MAX_TASKS 7

#define TASK_WIFI 0
#define TASK_DISPLAY 1
#define TASK_SNTP 2
#define TASK_TZDB 3
#define TASK_JOURNEY 4
#define TASK_HTTPD 5
#define TASK_SYSLOG 6

xTaskHandle task_handle[MAX_TASKS];
const char *task_names[MAX_TASKS+1] = {
    [TASK_WIFI] = "wifi",
    [TASK_DISPLAY] = "display",
    [TASK_SNTP] = "sntp",
    [TASK_TZDB] = "tzdb",
    [TASK_JOURNEY] = "journey",
    [TASK_HTTPD] = "httpd",
    [TASK_SYSLOG] = "syslog",
    NULL
};

void user_init(void)
{
    uart_init_new();
    UART_SetBaudrate(0, 115200);
    UART_SetPrintPort(0);

    printf("\n");
    log_init();
    LOG("SDK version:%s", system_get_sdk_version());

    spiffs_fs_init();

    config_load("/config.json");

    setenv("TZ", "GMT0", 1);
    tzset();

    TaskCreate(&wifi_task, task_names[TASK_WIFI], 512, NULL, 6, &task_handle[TASK_WIFI]);
    TaskCreate(&display_task, task_names[TASK_DISPLAY], 384, NULL, 3, &task_handle[TASK_DISPLAY]);
    TaskCreate(&sntp_task, task_names[TASK_SNTP], 384, NULL, 6, &task_handle[TASK_SNTP]);
    TaskCreate(&timezone_db_task, task_names[TASK_TZDB], 512, NULL, 5, &task_handle[TASK_TZDB]);
    TaskCreate(&journey_task, task_names[TASK_JOURNEY], 1024, NULL, 4, &task_handle[TASK_JOURNEY]);
    TaskCreate(&http_server_task, task_names[TASK_HTTPD], 1024, NULL, 4, &task_handle[TASK_HTTPD]);
    TaskCreate(&syslog_task, task_names[TASK_SYSLOG], 384, NULL, 2, &task_handle[TASK_SYSLOG]);
}
