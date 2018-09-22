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
#include "json.h"
#include "json-http.h"
#include "json-util.h"


#include "http-sm/http.h"

#include "log.h"

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

static const char *get_status(struct wifi_ap *ap)
{
    if(ap != wifi_current_ap) {
        return "";
    }

    if(wifi_state == WIFI_STATE_AP_CONNECTED) {
        return "connected";
    } else if(wifi_state == WIFI_STATE_AP_CONNECTING) {
        return "connecting";
    }

    return "";
}

enum http_cgi_state cgi_wifi_list(struct http_request* request)
{
    if(request->method == HTTP_METHOD_GET) {
        http_begin_response(request, 200, "application/json");
        http_end_header(request);

        http_write_string(request, "[");

        struct wifi_ap *ap;
        for(ap = wifi_first_ap; ap; ap = ap->next) {
            char buf[64];

            sprintf(buf, "%s{\"ssid\":\"%s\",\"saved\":true,\"status\":\"%s\"}",
                    ap == wifi_first_ap ? "" : ",",
                    ap->ssid,
                    get_status(ap)
                );
            http_write_string(request, buf);
        }

        http_write_string(request, "]");

        http_end_body(request);

        return HTTP_CGI_DONE;
    } else if(request->method == HTTP_METHOD_POST) {
        json_stream json;
        json_open_http(&json, request);

        if(json_expect(&json, JSON_OBJECT)) {
            if(json_find_name(&json, "ssid")) {
                if(json_expect(&json, JSON_STRING)) {
                    const char *s = json_get_string(&json, 0);
                    char *ssid = 0;
                    if(s) {
                        ssid = malloc(strlen(s) + 1);
                        strcpy(ssid, s);
                    }

                    if(json_find_name(&json, "password")) {
                        if(json_expect(&json, JSON_STRING)) {
                            const char *p = json_get_string(&json, 0);
                            char *password = 0;
                            if(p) {
                                password = malloc(strlen(p) + 1);
                                strcpy(password, p);
                            }


                            json_skip_until_end_of_object(&json);

                            if (json_get_error(&json)) {
                                LOG("JSON error %s", json_get_error(&json));
                            }

                            wifi_ap_add(ssid, password);

                            LOG("Added %s", ssid);

                            free(password);
                            free(ssid);

                            http_begin_response(request, 200, "text/plain");
                            http_end_header(request);
                            http_write_string(request, "OK");
                            http_end_body(request);

                            return HTTP_CGI_DONE;
                        }
                    }
                    free(ssid);
                }
            }
        }

        http_begin_response(request, HTTP_STATUS_BAD_REQUEST, "application/json");
        http_end_header(request);
        http_write_string(request, "{\"message\" : \"Bad request\"}");
        http_end_body(request);

        return HTTP_CGI_DONE;
    } else if(request->method == HTTP_METHOD_DELETE) {
        json_stream json;
        json_open_http(&json, request);

        if(json_expect(&json, JSON_OBJECT)) {
            if(json_find_name(&json, "ssid")) {
                if(json_expect(&json, JSON_STRING)) {
                    const char *s = json_get_string(&json, 0);
                    char *ssid = 0;
                    if(s) {
                        ssid = malloc(strlen(s) + 1);
                        strcpy(ssid, s);
                    }
                    json_skip_until_end_of_object(&json);

                    if (json_get_error(&json)) {
                        LOG("JSON error %s", json_get_error(&json));
                    }

                    if(wifi_current_ap && (strcmp(wifi_current_ap->ssid, ssid) == 0)) {
                        if(wifi_state == WIFI_STATE_AP_CONNECTED) {
                            wifi_ap_disconnect();
                            }
                    }

                    wifi_ap_remove(ssid);

                    LOG("Removed %s", ssid);

                    free(ssid);

                    http_begin_response(request, HTTP_STATUS_NO_CONTENT, NULL);
                    http_set_content_length(request, 0);
                    http_end_header(request);
                    http_end_body(request);

                    return HTTP_CGI_DONE;
                }
            }
        }

        http_begin_response(request, HTTP_STATUS_BAD_REQUEST, "application/json");
        http_end_header(request);
        http_write_string(request, "{\"message\" : \"Bad request\"}");
        http_end_body(request);

        return HTTP_CGI_DONE;
    } else {
        return HTTP_CGI_NOT_FOUND;
    }
}

static const char* format_authmode(uint8_t authmode)
{
    switch(authmode) {
    case AUTH_OPEN:
        return "null";
    case AUTH_WEP:
        return "\"WEP\"";
    case AUTH_WPA_PSK:
        return "\"WPA PSK\"";
    case AUTH_WPA2_PSK:
        return "\"WPA2 PSK\"";
    case AUTH_WPA_WPA2_PSK:
        return "\"WPA WPA2 PSK\"";
    default:
        LOG("Unkown authmode: %02x", authmode);
        return "?";
    }
}

enum http_cgi_state cgi_wifi_scan(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    if(!request->cgi_data) {
        wifi_start_scan();

        request->cgi_data = malloc(1);
        return HTTP_CGI_MORE;
    } else {
        if(!wifi_first_scan_ap) {
            return HTTP_CGI_MORE;
        }

        http_begin_response(request, 200, "application/json");
        http_end_header(request);

        http_write_string(request, "[");

        for(struct wifi_scan_ap *ap = wifi_first_scan_ap; ap; ap = ap->next) {
            char buf[64];

            sprintf(buf, "%s{\"ssid\":\"%s\",\"rssi\":%d, \"encryption\":%s}",
                    ap == wifi_first_scan_ap ? "" : ",",
                    ap->ssid,
                    ap->rssi,
                    format_authmode(ap->authmode)
                );
            http_write_string(request, buf);
        }

        http_write_string(request, "]");

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
    {"svg", "image/svg+xml"},
    {"json", "application/json"},
    {NULL, "text/plain"},
};

const char *http_get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');

    const struct http_mime_map *p;

    if(!ext) {
        return "text/plain";
    }

    ext++;

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

        if(request->cgi_arg) {
            base_filename = request->cgi_arg;
            www_prefix = "";
        } else {
            base_filename = request->path;
            www_prefix = WWW_DIR;
        }

        char *filename;

        filename = malloc(strlen(WWW_DIR) + strlen(base_filename) + strlen(GZIP_EXT) + 1);

        if(!filename) {
            return HTTP_CGI_NOT_FOUND;
        }

        strcpy(filename, www_prefix);
        strcat(filename, base_filename);

        int is_gzip = 0;
        int fd = -1;

        if(request->flags & HTTP_FLAG_ACCEPT_GZIP) {
            strcat(filename, GZIP_EXT);

            fd = open(filename, O_RDONLY);

            if(fd >= 0) {
                LOG("Opened file \"%s\"", filename);

                is_gzip = 1;
            } else {
                LOG("File \"%s\" not found", filename);
            }

            filename[strlen(filename) - strlen(GZIP_EXT)] = 0;
        }

        if(!is_gzip) {
            fd = open(filename, O_RDONLY);

            if(fd >= 0) {
                LOG("Opened file \"%s\"", filename);
            } else {
                LOG("File \"%s\" not found", filename);
            }
        }

        if(fd < 0) {
            free(filename);
            return HTTP_CGI_NOT_FOUND;
        }

        struct stat s;
        fstat(fd, &s);
        LOG("File size: %d", s.st_size);

        const char *mime_type = http_get_mime_type(filename);

        free(filename);


        request->cgi_data = malloc(sizeof(struct http_spiffs_response));

        if(!request->cgi_data) {
            return HTTP_CGI_NOT_FOUND;
        }

        struct http_spiffs_response *resp = request->cgi_data;

        resp->fd = fd;

        http_begin_response(request, 200, mime_type);
        http_write_header(request, "Cache-Control", "max-age=3600, must-revalidate");
        http_set_content_length(request, s.st_size);
        if(is_gzip) {
            http_write_header(request, "Content-Encoding", "gzip");
        }
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

enum http_cgi_state cgi_journies_json(struct http_request* request)
{
    struct http_request* req = request->cgi_data;

    if(!req) {
        if(request->method != HTTP_METHOD_GET) {
            return HTTP_CGI_NOT_FOUND;
        }

        const char *site_id = http_get_query_arg(request, "siteId");

        if(!site_id) {
            LOG("No siteId provided");
            http_begin_response(request, 200, "application/json");
            http_write_header(request, "Cache-Control", "no-cache");
            http_end_header(request);

            http_write_string(request, "{\"StatusCode\":-1,\"Message\":\"No siteId given\"}");
            http_end_body(request);

            return HTTP_CGI_DONE;
        }

        char *path = malloc(256);

        if(!path) {
            LOG("malloc failed");
            http_begin_response(request, 200, "application/json");
            http_write_header(request, "Cache-Control", "no-cache");
            http_end_header(request);

            http_write_string(request, "{\"StatusCode\":-1,\"Message\":\"Out of memory when allocating path\"}");
            http_end_body(request);

            return HTTP_CGI_DONE;
        }

        req = malloc(sizeof(*req));

        if(!req) {
            LOG("malloc failed");
            http_begin_response(request, 200, "application/json");
            http_write_header(request, "Cache-Control", "no-cache");
            http_end_header(request);

            http_write_string(request, "{\"StatusCode\":-1,\"Message\":\"Out of memory when allocating request\"}");
            http_end_body(request);

            free(path);
            return HTTP_CGI_DONE;
        }

        http_request_init(req);

        snprintf(path, 256, "/api2/realtimedeparturesV4.json?key=%s&TimeWindow=60&SiteId=%s", KEY_SL_REALTIME, site_id);
        req->host = "api.sl.se";
        req->path = path;
        req->port = 80;

        if(http_get_request(req) < 0) {
            LOG("http_get_request failed");

            http_begin_response(request, 200, "application/json");
            http_write_header(request, "Cache-Control", "no-cache");
            http_end_header(request);

            http_write_string(request, "{\"StatusCode\":-1,\"Message\":\"Request failed\"}");
            http_end_body(request);

            free(path);
            free(req);
            return HTTP_CGI_DONE;
        }

        http_begin_response(request, 200, "application/json");
        http_write_header(request, "Cache-Control", "no-cache");
        http_set_content_length(request, req->read_content_length);
        http_end_header(request);


        request->cgi_data = req;
        return HTTP_CGI_MORE;
    } else {

        char buf[64];
        int n = http_read(req, buf, sizeof(buf));

        if(n > 0) {
            http_write_bytes(request, buf, n);
            return HTTP_CGI_MORE;
        } else {
            http_end_body(request);
            http_close(req);
            free(req->path);
            free(req);
            return HTTP_CGI_DONE;
        }
    }
}

static struct http_url_handler http_url_tab_[] = {
    {"/api/wifi-scan.json", cgi_wifi_scan, NULL},
    {"/api/wifi-list.json", cgi_wifi_list, NULL},
    {"/simple", cgi_simple, NULL},
    {"/stream", cgi_stream, NULL},
    {"/query", cgi_query, NULL},
    {"/journies.json", cgi_journies_json, NULL},
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
    TaskCreate(&journey_test_task, "journey_test_task", 1024, NULL, 4, NULL);
    TaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
#else
    TaskCreate(&wifi_task, "wifi_task", 384, NULL, 6, NULL);
    // TaskCreate(&http_test_task, "http_test_task", 384, NULL, 6, NULL);
    // TaskCreate(&display_task, "display_task", 384, NULL, 3, NULL);
    // TaskCreate(&sntp_task, "sntp_task", 384, NULL, 6, NULL);
    // TaskCreate(&timezone_db_task, "timezone_db_task", 512, NULL, 5, NULL);
    // TaskCreate(&journey_task, "journey_task", 1024, NULL, 4, NULL);
    TaskCreate(&http_server_test_task, "http_server_test_task", 1024, NULL, 4, NULL);

#endif

    // TaskCreate(&tz_test_task, "tz_test_task", 384, NULL, 6, NULL);
}
