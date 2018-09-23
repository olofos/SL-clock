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
#include "log.h"

#include "http-sm/http.h"

#define LOG_SYS LOG_SYS_HTTPD

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)


static void write_simple_response(struct http_request* request, int status, const char* content_type, const char* reply)
{
    http_begin_response(request, status, content_type);
    http_write_header(request, "Cache-Control", "no-cache");
    if(reply) {
        http_set_content_length(request, strlen(reply));
    } else {
        http_set_content_length(request, 0);
    }
    http_end_header(request);
    if(reply) {
        http_write_string(request, reply);
    }
    http_end_body(request);
}

enum http_cgi_state cgi_not_found(struct http_request* request)
{
    write_simple_response(request, 404, "text/plain", "Not found\r\n");

    return HTTP_CGI_DONE;
}

static const char *get_status_of_ap(struct wifi_ap *ap)
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
                    get_status_of_ap(ap)
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

                            write_simple_response(request, 204, NULL, NULL);

                            return HTTP_CGI_DONE;
                        }
                    }
                    free(ssid);
                }
            }
        }

        write_simple_response(request, 400, "application/json", "{\"message\" : \"Bad request\"}");

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

                    write_simple_response(request, 204, NULL, NULL);

                    return HTTP_CGI_DONE;
                }
            }
        }

        write_simple_response(request, 400, "application/json", "{\"message\" : \"Bad request\"}");

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

        http_begin_response(request, 200, "application/json");
        http_end_header(request);

        request->cgi_data = malloc(1);
        return HTTP_CGI_MORE;
    } else {
        if(!wifi_first_scan_ap) {
            return HTTP_CGI_MORE;
        }

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

enum http_cgi_state cgi_journey_config(struct http_request* request)
{
    if(request->method == HTTP_METHOD_GET) {
        http_begin_response(request, 200, "application/json");
        http_end_header(request);

        http_write_string(request, "[");

        char buf[16];

        for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
        {
            if(strlen(journies[i].line) > 0)
            {
                if(i > 0)
                {
                    http_write_string(request, ",");
                }
                http_write_string(request, "{");
                http_write_string(request, "\"line\":\"");
                http_write_string(request, journies[i].line);
                http_write_string(request, "\",");

                http_write_string(request, "\"stop\":\"");
                http_write_string(request, journies[i].stop);
                http_write_string(request, "\",");

                http_write_string(request, "\"destination\":\"");
                http_write_string(request, journies[i].destination);
                http_write_string(request, "\",");

                sprintf(buf, "%d,",journies[i].site_id);
                http_write_string(request, "\"site_id\":");
                http_write_string(request, buf);

                sprintf(buf, "%d,",journies[i].mode);
                http_write_string(request, "\"mode\":");
                http_write_string(request, buf);

                sprintf(buf, "%d",journies[i].direction);
                http_write_string(request, "\"direction\":");
                http_write_string(request, buf);

                http_write_string(request, "}");
            }
        }
        http_write_string(request, "]");

        http_end_body(request);

        return HTTP_CGI_DONE;
    } else {
        return HTTP_CGI_NOT_FOUND;
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

struct cgi_forward_data {
    char *host;
    char *path;
    uint16_t port;

    char *query;
};

enum http_cgi_state cgi_forward(struct http_request* request)
{
    struct http_request* req = request->cgi_data;

    if(!req) {
        if(request->method != HTTP_METHOD_GET) {
            return HTTP_CGI_NOT_FOUND;
        }

        const struct cgi_forward_data *data = request->cgi_arg;

        const char *query_data_raw = http_get_query_arg(request, data->query);

        if(!query_data_raw) {
            LOG("No query provided");

            write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"No query given\"}");

            return HTTP_CGI_DONE;
        }

        int query_data_len = http_urlencode(0, query_data_raw, 0);
        char *query_data = malloc(query_data_len + 1);

        if(!query_data) {
            LOG("malloc failed");

            write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"Out of memory when allocating query\"}");

            return HTTP_CGI_DONE;
        }

        http_urlencode(query_data, query_data_raw, query_data_len + 1);

        int path_len = strlen(data->path) + 1 + strlen(data->query) + 1 + strlen(query_data) + 1;
        char *path = malloc(path_len);

        if(!path) {
            LOG("malloc failed");

            write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"Out of memory when allocating path\"}");

            free(query_data);
            return HTTP_CGI_DONE;
        }

        req = malloc(sizeof(*req));

        if(!req) {
            LOG("malloc failed");

            write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"Out of memory when allocating request\"}");

            free(query_data);
            free(path);
            return HTTP_CGI_DONE;
        }

        http_request_init(req);

        strcpy(path, data->path);
        strcat(path, "&");
        strcat(path, data->query);
        strcat(path, "=");
        strcat(path, query_data);

        free(query_data);

        req->host = "api.sl.se";
        req->path = path;
        req->port = 80;

        if(http_get_request(req) < 0) {
            LOG("http_get_request failed");

            write_simple_response(request, 500, "application/json", "{\"StatusCode\":-1,\"Message\":\"Request failed\"}");

            free(path);
            free(req);
            return HTTP_CGI_DONE;
        }

        http_begin_response(request, req->status, req->content_type);
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

static void write_system_status(struct http_json_writer* json)
{
    http_json_begin_object(json, "system");
    http_json_write_int(json, "free-memory", xPortGetFreeHeapSize());
    http_json_end_object(json);
}

static const char *format_date(char *buf, int len, const time_t *t)
{
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", localtime(t));
    return buf;
}

static void write_time_status(struct http_json_writer* json)
{
    char buf[32];
    time_t now = time(0);

    LOG("next update: %d", *timezone_get_next_update());

    http_json_begin_object(json, "time");
    http_json_write_string(json, "now", format_date(buf, sizeof(buf), &now));
    http_json_begin_object(json, "timezone");
    http_json_write_string(json, "name", timezone_get_timezone());
    http_json_write_string(json, "abbrev", getenv("TZ"));
    http_json_write_string(json, "next-update", format_date(buf, sizeof(buf), timezone_get_next_update()));
    http_json_end_object(json);
    http_json_end_object(json);
}


static void format_ip(char* buf, uint32_t ip)
{
    sprintf(buf, "%d.%d.%d.%d", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
}

static void write_wifi_status(struct http_json_writer* json)
{
    uint8_t mode = wifi_get_opmode();
    char *mode_str;

    if(mode == 0x01) {
        mode_str = "Station";
    } else if(mode == 0x02) {
        mode_str = "SoftAP";
    } else if(mode == 0x03) {
        mode_str = "Station+SoftAP";
    } else {
        mode_str = "None";
    }

    http_json_begin_object(json, "wifi");

    http_json_write_string(json, "mode", mode_str);

    if(mode & 0x01) {
        struct	station_config station_config;

        wifi_station_get_config(&station_config);

        uint8_t status =  wifi_station_get_connect_status();

        const char *status_string[] = {
            [STATION_IDLE] = "idle",
            [STATION_CONNECTING] = "connecting",
            [STATION_WRONG_PASSWORD] = "wrong password",
            [STATION_NO_AP_FOUND] = "no ap",
            [STATION_CONNECT_FAIL] = "connect failed",
            [STATION_GOT_IP] = "connected",
        };

        struct ip_info ip_info;

        wifi_get_ip_info(0, &ip_info);

        int8_t rssi = wifi_station_get_rssi();

        http_json_begin_object(json, "station");

        http_json_write_string(json, "status", status_string[status]);
        http_json_write_string(json, "ssid", (char*)station_config.ssid);

        if(status == STATION_GOT_IP) {
            char buf[16];

            format_ip(buf, ip_info.ip.addr);
            http_json_write_string(json, "ip", buf);

            format_ip(buf, ip_info.netmask.addr);
            http_json_write_string(json, "netmask", buf);

            format_ip(buf, ip_info.gw.addr);
            http_json_write_string(json, "gateway", buf);
        }

        if(rssi < 31) {
            http_json_write_int(json, "rssi", rssi);
        }

        http_json_end_object(json);
    }

    http_json_end_object(json);
}

void write_journey_status(struct http_json_writer *json, const struct journey *journey)
{
    http_json_begin_object(json, NULL);
    if(journey->line[0]) {
        char buf[32];

        http_json_write_string(json, "line", journey->line);
        http_json_write_string(json, "stop", journey->stop);
        http_json_write_string(json, "destination", journey->destination);
        http_json_write_int(json, "site-id", journey->site_id);
        http_json_write_int(json, "mode", journey->mode);
        http_json_write_int(json, "direction", journey->direction);
        http_json_write_string(json, "next-update", format_date(buf, sizeof(buf), &journey->next_update));

        http_json_begin_array(json, "departures");
        for(int i = 0; (i < JOURNEY_MAX_DEPARTURES) && (journey->departures[i] > 0); i++) {
            http_json_write_string(json, NULL, format_date(buf, sizeof(buf), &journey->departures[i]));
        }
        http_json_end_array(json);
    }
    http_json_end_object(json);
}

void write_journies_status(struct http_json_writer *json)
{
    http_json_begin_array(json, "journies");

    for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++) {
        write_journey_status(json, &journies[i]);
    }

    http_json_end_array(json);
}

enum http_cgi_state cgi_status(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    http_begin_response(request, 200, "application/json");
    http_write_header(request, "Cache-Control", "no-cache");
    http_end_header(request);

    struct http_json_writer json;
    http_json_init(&json, request);

    http_json_begin_object(&json, NULL);

    write_system_status(&json);
    write_wifi_status(&json);
    write_time_status(&json);
    write_journies_status(&json);

    http_json_end_object(&json);

    http_end_body(request);
    return HTTP_CGI_DONE;
}

struct cgi_forward_data journies_forward_data = {
    .host = "api.sl.se",
    .path = "/api2/realtimedeparturesV4.json?key=" KEY_SL_REALTIME "&TimeWindow=60",
    .port = 80,
    .query = "siteId",
};

struct cgi_forward_data places_forward_data = {
    .host = "api.sl.se",
    .path = "/api2/typeahead.json?key=" KEY_SL_PLACES,
    .port = 80,
    .query = "SearchString",
};


static struct http_url_handler http_url_tab_[] = {
    {"/api/wifi-scan.json", cgi_wifi_scan, NULL},
    {"/api/wifi-list.json", cgi_wifi_list, NULL},
    {"/api/journies-config.json", cgi_journey_config, NULL},
    {"/api/places.json", cgi_forward, &places_forward_data},
    {"/api/journies.json", cgi_forward, &journies_forward_data},
    {"/api/status.json", cgi_status, NULL},
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
