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
#include "http-client.h"
#include "httpd/httpd.h"
#include "httpd/cgiwifi.h"

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


static const char *WWW_DIR = "/www";
static const char *GZIP_EXT = ".gz";

static const char *gzipNotSupportedMessage = "HTTP/1.0 501 Not implemented\r\nServer: esp8266-httpd/"HTTPDVER"\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: 52\r\n\r\nYour browser does not accept gzip-compressed data.\r\n";

int cgiSPIFFSHook(HttpdConnData *connData) {
    int *fdp = connData->cgiData;
    int len;
    char buff[256];
    char acceptEncodingBuffer[64];
    int acceptGzip = 0;
    int isGzip = 0;
    char *filename;

    if(fdp == NULL) {
        fdp = malloc(sizeof(*fdp));
        if(!fdp) {
            return HTTPD_CGI_NOTFOUND;
        }

        if (connData->cgiArg != NULL) {
            //Open a different file than provided in http request.
            //Common usage: {"/", cgiSPIFFSHook, "/www/index.html"} will show content of index.html without actual redirect to that file if host root was requested
            const char *raw_filename = connData->cgiArg;
            filename = malloc(strlen(raw_filename) + strlen(GZIP_EXT)+ 1);

            if(!filename) {
                free(fdp);
                return HTTPD_CGI_NOTFOUND;
            }

            strcpy(filename, raw_filename);
        } else {
            //Open the file so we can read it.
            const char *raw_filename = connData->url;
            filename = malloc(strlen(WWW_DIR) + strlen(raw_filename) + strlen(GZIP_EXT)+ 1);

            if(!filename) {
                free(fdp);
                return HTTPD_CGI_NOTFOUND;
            }

            strcpy(filename, WWW_DIR);
            strcat(filename, raw_filename);
        }
        strcat(filename, GZIP_EXT);

        LOG("Filename: '%s'\n", filename);

        *fdp = open(filename, O_RDONLY);

        if(*fdp >= 0) {
            isGzip = 1;

            httpdGetHeader(connData, "Accept-Encoding", acceptEncodingBuffer, sizeof(acceptEncodingBuffer));

            LOG("Accept-Encoding: %s", acceptEncodingBuffer);

            if (strstr(acceptEncodingBuffer, "gzip")) {
                acceptGzip = 1;
            }
        } else {
            LOG("Could not open '%s'", filename);
        }

        LOG("isGzip = %d, acceptGzip = %d", isGzip, acceptGzip);

        if(!(isGzip && acceptGzip)) {
            if(*fdp >= 0)
            {
                close(*fdp);
            }

            filename[strlen(filename) - strlen(GZIP_EXT)] = 0;
            LOG("Filename: '%s'\n", filename);
            *fdp = open(filename, O_RDONLY);
        }

        free(filename);

        if (*fdp < 0) {
            LOG("Could not open '%s'", filename);
            free(fdp);
            if(isGzip)
            {
                // We found a gzip file but no uncompressed file, and the client doesn't support gzip
                httpdSend(connData, gzipNotSupportedMessage, strlen(gzipNotSupportedMessage));
                return HTTPD_CGI_DONE;
            } else {
                return HTTPD_CGI_NOTFOUND;
            }
        }

        connData->cgiData = fdp;

        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
        if(isGzip && acceptGzip) {
            httpdHeader(connData, "Content-Encoding", "gzip");
        }
        httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
        httpdEndHeaders(connData);
        return HTTPD_CGI_MORE;
    }

    len = read(*fdp, buff, sizeof(buff));

    if (len > 0) {
        httpdSend(connData, buff, len);
    }

    if(len != sizeof(buff)) {
        close(*fdp);
        free(fdp);
        return HTTPD_CGI_DONE;
    } else {
        return HTTPD_CGI_MORE;
    }
}

int cgiJourniesJSON(HttpdConnData *connData) {
    struct HTTPRequest *request = connData->cgiData;

    if(!request)
    {
        char path[256];
        char site_id_str[16];
        int len;

        if (connData->conn==NULL) {
            //Connection aborted. Clean up.
            return HTTPD_CGI_DONE;
        }

        if (connData->requestType!=HTTPD_METHOD_GET) {
            //Sorry, we only accept GET requests.
            httpdStartResponse(connData, 406);  //http error code 'unacceptable'
            httpdEndHeaders(connData);
            return HTTPD_CGI_DONE;
        }

        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", "application/json");
        httpdHeader(connData, "Cache-Control", "no-cache");
        httpdEndHeaders(connData);

        //Look for the 'name' GET value. If found, urldecode it and return it into the 'name' var.
        len=httpdFindArg(connData->getArgs, "siteId", site_id_str, sizeof(site_id_str));
        if (len < 0) {
            httpdSend(connData, "{\"StatusCode\":-1,\"Message\":\"No site id supplied\"}", -1);
            return HTTPD_CGI_DONE;
        }

        request = malloc(sizeof(*request));

        snprintf(path, sizeof(path), "/api2/realtimedeparturesV4.json?key=%s&TimeWindow=60&SiteId=%s", KEY_SL_REALTIME, site_id_str);
        request->host = "api.sl.se";
        request->path = path;
        request->port = 80;

        LOG("http_open");

        if(http_open(request) < 0)
        {
            free(request);

            httpdSend(connData, "{\"StatusCode\":-1,\"Message\":\"Could not open connection\"}", -1);
            LOG("http_open failed");
            return HTTPD_CGI_DONE;
        }

        if(http_get(request) < 0) {
            http_close(request);
            free(request);

            httpdSend(connData, "{\"StatusCode\":-1,\"Message\":\"Could not send get request\"}", -1);
            LOG("http_get failed");
            return HTTPD_CGI_DONE;
        }

        connData->cgiData = request;
        return HTTPD_CGI_MORE;
    } else {
        char output[256];
        int len;

        len = http_read(request, output, sizeof(output));

        if(len > 0)
        {
            httpdSend(connData, output, len);
            return HTTPD_CGI_MORE;
        } else {

            http_close(request);
            free(request);
            return HTTPD_CGI_DONE;
        }
    }
}

const HttpdBuiltInUrl builtInUrls[] = {
    {"/", cgiSPIFFSHook, "/www/index.html"},
    {"/journies.json", cgiJourniesJSON, NULL},
    {"/wifi.json", cgiWiFiScan, NULL},
    {"*", cgiSPIFFSHook, NULL},
    {NULL, NULL, NULL}
};

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

    httpdInit(builtInUrls, 80);

    // xTaskCreate(&tz_test_task, "tz_test_task", 384, NULL, 6, NULL);
    // xTaskCreate(&journey_test_task, "journey_test_task", 1024, NULL, 4, NULL);
}
