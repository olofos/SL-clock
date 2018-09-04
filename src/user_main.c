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
//#include "http-client.h"
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

#include "http-sm/http.h"

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
    journies[1].line[0] = 0;

    journies[0].mode = TRANSPORT_MODE_SHIP;
    journies[1].mode = TRANSPORT_MODE_BUS;

    for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
    {
        journies[i].departures[0] = 300;
        journies[i].departures[1] = journies[i].departures[0] + 300;
    }

    vTaskDelayMs(5000);
    journies[0].departures[0] += 300;
    journies[0].departures[1] = journies[0].departures[0] + 300;


    for(;;)
    {
        for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
        {
            if(!(rand() & 0x03))
            {
                journies[i].departures[0] += 300;
                journies[i].departures[1] = journies[i].departures[0] + 300;
            }
        }

#ifdef TEST_DISPLAY_MESSAGE
        static int msg = 0;
        if(!(rand() & 0x03)) {
            if(msg) {
                display_post_message(DISPLAY_MESSAGE_NONE);
                msg = 0;
            } else {
                display_post_message(DISPLAY_MESSAGE_NO_WIFI);
                msg = 1;
            }
        }
#endif

        vTaskDelayMs(5000);
    }
}

void http_test_task(void *pvParameters)
{
    for(;;)
    {
        while(!app_status.wifi_connected)
        {
            LOG("WIFI not ready");
            vTaskDelayMs(1000);
        }

        struct http_request request = {
            .host = "www.google.com",
            .path = "/",
            .port = 80,
        };

        if(http_get_request(&request) > 0) {
            LOG("http_get_request succeeded with status %d", request.status);

            int c;
            while((c = http_getc(&request)) > 0) {
                printf("%c", c);
            }
            printf("\r\n");
        } else {
            LOG("http_get_request failed");
        }

        http_close(&request);

        vTaskDelayMs(5000);
    }
}

enum http_cgi_state cgi_not_found(struct http_request* request)
{
    const char *response = "Not found\r\n";

    http_begin_response(request, 404, "text/plain");
    http_set_content_length(request, strlen(response));
    http_end_header(request);
    http_write_string(request, response);
    http_end_body(request);

    return HTTP_CGI_DONE;
}


enum http_cgi_state cgi_simple(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    const char *response = "This is a response from \'cgi_simple\'\r\n";

    http_begin_response(request, 200, "text/plain");
    http_set_content_length(request, strlen(response));
    http_end_header(request);

    http_write_string(request, response);

    http_end_body(request);

    return HTTP_CGI_DONE;
}

enum http_cgi_state cgi_query(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    http_begin_response(request, 200, "text/plain");
    http_end_header(request);

    http_write_string(request, "This is a response from \'cgi_query\'\r\n");
    http_write_string(request, "The parameters were:\r\n");

    const char *sa = http_get_query_arg(request, "a");
    const char *sb = http_get_query_arg(request, "b");

    if(sa) {
        http_write_string(request, "a = ");
        http_write_string(request, sa);
        http_write_string(request, "\r\n");
    }

    if(sb) {
        http_write_string(request, "b = ");
        http_write_string(request, sb);
        http_write_string(request, "\r\n");
    }

    http_end_body(request);

    return HTTP_CGI_DONE;
}

enum http_cgi_state cgi_stream(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    if(!request->cgi_data) {
        http_begin_response(request, 200, "text/plain");
        http_end_header(request);

        request->cgi_data = malloc(1);

        return HTTP_CGI_MORE;
    } else {
        const char response[] = "This is a response from \'cgi_stream\'\r\n";

        http_write_string(request, response);
        http_end_body(request);

        free(request->cgi_data);

        return HTTP_CGI_DONE;
    }
}

static const char *WWW_DIR = "/www";
static const char *GZIP_EXT = ".gz";

struct http_spiffs_response
{
    int fd;
    char buf[128];
};

struct http_mime_map
{
    const char *ext;
    const char *type;
};

const struct http_mime_map mime_tab[] = {
    {"html", "text/html"},
    {"css", "text/css"},
    {"js", "text/javascript"},
    {"png", "image/png"},
    {"json", "application/json"},
    {NULL, "text/plain"},
};

const char *http_get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');

    const struct http_mime_map *p;

    if(ext) {
        ext++;
    }

    for(p = mime_tab; p->ext; p++) {
        if(!strcmp(ext, p->ext)) {
            break;
        }
    }
    return p->type;
}

enum http_cgi_state cgi_spiffs(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    if(!request->cgi_data) {
        const char *base_filename;
        const char *www_prefix;

        LOG("path: \"%s\"", request->path);

        if(request->cgi_arg) {
            base_filename = request->cgi_arg;
            www_prefix = "";
        } else {
            base_filename = request->path;
            www_prefix = WWW_DIR;
        }

        if(base_filename[strlen(base_filename)-1] == '/') {
            return HTTP_CGI_NOT_FOUND;
        }

        char *filename;

        filename = malloc(strlen(WWW_DIR) + strlen(base_filename) + strlen(GZIP_EXT) + 1);

        if(!filename) {
            return HTTP_CGI_NOT_FOUND;
        }

        strcpy(filename, www_prefix);
        strcat(filename, base_filename);

        LOG("Opening file \"%s\"", filename);
        int fd = open(filename, O_RDONLY);

        const char *mime_type = http_get_mime_type(filename);

        free(filename);

        if(fd < 0) {
            return HTTP_CGI_NOT_FOUND;
        }

        request->cgi_data = malloc(sizeof(struct http_spiffs_response));

        if(!request->cgi_data) {
            return HTTP_CGI_NOT_FOUND;
        }

        struct http_spiffs_response *resp = request->cgi_data;

        resp->fd = fd;

        http_begin_response(request, 200, mime_type);
        http_write_header(request, "Cache-Control", "max-age=3600, must-revalidate");
        http_end_header(request);

        return HTTP_CGI_MORE;
    } else {
        struct http_spiffs_response *resp = request->cgi_data;

        int n = read(resp->fd, resp->buf, sizeof(resp->buf));

        if(n > 0) {
            http_write_bytes(request, resp->buf, n);
        }

        if(n < sizeof(resp->buf)) {
            http_end_body(request);

            close(resp->fd);
            free(request->cgi_data);

            return HTTP_CGI_DONE;
        } else {
            return HTTP_CGI_MORE;
        }
    }
}

static struct http_url_handler http_url_tab_[] = {
    {"/simple", cgi_simple, NULL},
    {"/stream", cgi_stream, NULL},
    {"/query", cgi_query, NULL},
    {"/", cgi_spiffs, "/www/index.html"},
    {"/*", cgi_spiffs, NULL},
    {NULL, NULL, NULL}
};

struct http_url_handler *http_url_tab = http_url_tab_;

void http_server_test_task(void *pvParameters)
{
    for(;;) {
        http_server_main(80);
        LOG("The http server has returned!");
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

#if 0
    http_mutex = xSemaphoreCreateMutex();
#endif

#ifdef TEST_JOURNEY_TASK
    xTaskCreate(&journey_test_task, "journey_test_task", 1024, NULL, 4, NULL);
    xTaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
#else
    xTaskCreate(&wifi_task, "wifi_task", 384, NULL, 6, NULL);
    // xTaskCreate(&http_test_task, "http_test_task", 384, NULL, 6, NULL);
    // xTaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
    // xTaskCreate(&sntp_task, "sntp_task", 384, NULL, 6, NULL);
    // xTaskCreate(&timezone_db_task, "timezone_db_task", 512, NULL, 5, NULL);
    // xTaskCreate(&journey_task, "journey_task", 1024, NULL, 4, NULL);
    xTaskCreate(&http_server_test_task, "http_server_test_task", 1024, NULL, 4, NULL);

#endif

    // xTaskCreate(&tz_test_task, "tz_test_task", 384, NULL, 6, NULL);
}
