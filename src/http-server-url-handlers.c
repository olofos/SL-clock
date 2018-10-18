#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <esp_common.h>
#include <lwip/api.h>

#include "timezone-db.h"
#include "journey.h"
#include "keys.h"
#include "wifi-task.h"
#include "json.h"
#include "json-http.h"
#include "json-util.h"
#include "json-writer.h"
#include "log.h"
#include "http-sm/http.h"
#include "http-server-task.h"
#include "http-server-url-handlers.h"


#define LOG_SYS LOG_SYS_HTTPD

static const char *get_status_of_ap(const struct wifi_ap *ap)
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
        http_write_header(request, "Cache-Control", "no-cache");
        http_end_header(request);

        struct json_writer json;
        json_writer_http_init(&json, request);

        json_writer_begin_array(&json, NULL);

        for(const struct wifi_ap *ap = wifi_first_ap; ap; ap = ap->next) {
            json_writer_begin_object(&json, NULL);
            json_writer_write_string(&json, "ssid", ap->ssid);
            json_writer_write_bool(&json, "saved", 1);
            json_writer_write_string(&json, "status", get_status_of_ap(ap));
            json_writer_end_object(&json);
        }

        json_writer_end_array(&json);

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
                                WARNING("JSON error %s", json_get_error(&json));
                            }

                            wifi_ap_add(ssid, password);

                            LOG("Added new AP %s", ssid);

                            if(wifi_state == WIFI_STATE_AP_CONNECTED) {
                                LOG("Disconnecting from current AP");
                                wifi_ap_disconnect();
                            }

                            free(password);
                            free(ssid);

                            http_server_write_simple_response(request, 204, NULL, NULL);

                            return HTTP_CGI_DONE;
                        }
                    }
                    free(ssid);
                }
            }
        }

        http_server_write_simple_response(request, 400, "application/json", "{\"message\" : \"Bad request\"}");

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
                        WARNING("JSON error %s", json_get_error(&json));
                    }

                    if(wifi_current_ap && (strcmp(wifi_current_ap->ssid, ssid) == 0)) {
                        if(wifi_state == WIFI_STATE_AP_CONNECTED) {
                            wifi_ap_disconnect();
                        }
                    }

                    wifi_ap_remove(ssid);

                    LOG("Removed AP %s", ssid);

                    free(ssid);

                    http_server_write_simple_response(request, 204, NULL, NULL);

                    return HTTP_CGI_DONE;
                }
            }
        }

        http_server_write_simple_response(request, 400, "application/json", "{\"message\" : \"Bad request\"}");

        return HTTP_CGI_DONE;
    } else {
        return HTTP_CGI_NOT_FOUND;
    }
}

static const char* format_authmode(uint8_t authmode)
{
    switch(authmode) {
    case AUTH_OPEN:
        return NULL;
    case AUTH_WEP:
        return "WEP";
    case AUTH_WPA_PSK:
        return "WPA PSK";
    case AUTH_WPA2_PSK:
        return "WPA2 PSK";
    case AUTH_WPA_WPA2_PSK:
        return "WPA WPA2 PSK";
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

        struct json_writer json;
        json_writer_http_init(&json, request);

        json_writer_begin_array(&json, NULL);

        for(struct wifi_scan_ap *ap = wifi_first_scan_ap; ap; ap = ap->next) {
            json_writer_begin_object(&json, NULL);
            json_writer_write_string(&json, "ssid", ap->ssid);
            json_writer_write_int(&json, "rssi", ap->rssi);
            json_writer_write_string(&json, "encryption", format_authmode(ap->authmode));
            json_writer_end_object(&json);
        }

        json_writer_end_array(&json);

        http_end_body(request);

        free(request->cgi_data);

        return HTTP_CGI_DONE;
    }
}

void config_load_journies(json_stream *json);
void config_save(const char *filename);

enum http_cgi_state cgi_journey_config(struct http_request* request)
{
    if(request->method == HTTP_METHOD_GET) {
        http_begin_response(request, 200, "application/json");
        http_end_header(request);

        struct json_writer json;
        json_writer_http_init(&json, request);

        json_writer_begin_array(&json, NULL);

        for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++) {
            if(journies[i].line[0]) {
                json_writer_begin_object(&json, NULL);
                json_writer_write_string(&json, "line", journies[i].line);
                json_writer_write_string(&json, "stop", journies[i].stop);
                json_writer_write_string(&json, "destination", journies[i].destination);
                json_writer_write_int(&json, "site-id", journies[i].site_id);
                json_writer_write_int(&json, "mode", journies[i].mode);
                json_writer_write_int(&json, "direction", journies[i].direction);
                json_writer_end_object(&json);
            }
        }

        json_writer_end_array(&json);
        http_end_body(request);

        return HTTP_CGI_DONE;
    } else if(request->method == HTTP_METHOD_POST) {
        json_stream json;
        json_open_http(&json, request);

        config_load_journies(&json);

        http_server_write_simple_response(request, 204, NULL, NULL);

        config_save("/config.json");

        return HTTP_CGI_DONE;
    } else {
        return HTTP_CGI_NOT_FOUND;
    }
}

extern xTaskHandle task_handle[];
extern const char *task_names[];

static void write_system_status(struct json_writer* json)
{
    json_writer_begin_object(json, "system");
    json_writer_write_int(json, "heap", xPortGetFreeHeapSize());
    json_writer_begin_array(json, "tasks");

    for(int i = 0; task_names[i]; i++) {
        if(task_handle[i]) {
            json_writer_begin_object(json, NULL);
            json_writer_write_string(json, "name", task_names[i]);
            json_writer_write_int(json, "stack", uxTaskGetStackHighWaterMark(task_handle[i]));
            json_writer_end_object(json);
        }
    }

    json_writer_end_array(json);

    json_writer_end_object(json);
}

static const char *format_date(char *buf, int len, const time_t *t)
{
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", localtime(t));
    return buf;
}

static void write_time_status(struct json_writer* json)
{
    char buf[32];
    time_t now = time(0);

    json_writer_begin_object(json, "time");
    json_writer_write_string(json, "now", format_date(buf, sizeof(buf), &now));
    json_writer_begin_object(json, "timezone");
    json_writer_write_string(json, "name", timezone_get_timezone());
    json_writer_write_string(json, "abbrev", getenv("TZ"));
    json_writer_write_string(json, "next-update", format_date(buf, sizeof(buf), timezone_get_next_update()));
    json_writer_end_object(json);
    json_writer_end_object(json);
}


static void write_wifi_status(struct json_writer* json)
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

    json_writer_begin_object(json, "wifi");

    json_writer_write_string(json, "mode", mode_str);

    json_writer_begin_object(json, "station");
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

        wifi_get_ip_info(STATION_IF, &ip_info);

        int8_t rssi = wifi_station_get_rssi();

        json_writer_write_string(json, "status", status_string[status]);
        json_writer_write_string(json, "ssid", (char*)station_config.ssid);

        if(status == STATION_GOT_IP) {
            char buf[16];

            json_writer_write_string(json, "ip", ipaddr_ntoa_r(&ip_info.ip, buf, sizeof(buf)));
            json_writer_write_string(json, "netmask", ipaddr_ntoa_r(&ip_info.netmask, buf, sizeof(buf)));
            json_writer_write_string(json, "gateway", ipaddr_ntoa_r(&ip_info.gw, buf, sizeof(buf)));
        }

        if(rssi < 31) {
            json_writer_write_int(json, "rssi", rssi);
        }
    }
    json_writer_end_object(json);

    json_writer_begin_object(json, "softAP");
    if(mode & 0x02) {
        struct softap_config softap_config;
        wifi_softap_get_config(&softap_config);

        uint8_t num_connected = wifi_softap_get_station_num();

        struct ip_info ip_info;

        wifi_get_ip_info(SOFTAP_IF, &ip_info);

        json_writer_write_string(json, "ssid", (char*)softap_config.ssid);
        json_writer_write_int(json, "connected-stations", num_connected);

        char buf[16];

        json_writer_write_string(json, "ip", ipaddr_ntoa_r(&ip_info.ip, buf, sizeof(buf)));
        json_writer_write_string(json, "netmask", ipaddr_ntoa_r(&ip_info.netmask, buf, sizeof(buf)));
        json_writer_write_string(json, "gateway", ipaddr_ntoa_r(&ip_info.gw, buf, sizeof(buf)));
    }
    json_writer_end_object(json);


    json_writer_begin_array(json, "known-networks");
    for(const struct wifi_ap *ap = wifi_first_ap; ap; ap = ap->next) {
        json_writer_write_string(json, NULL, ap->ssid);
    }
    json_writer_end_array(json);

    json_writer_end_object(json);
}

void write_journey_status(struct json_writer *json, const struct journey *journey)
{
    json_writer_begin_object(json, NULL);
    if(journey->line[0]) {
        char buf[32];

        json_writer_write_string(json, "line", journey->line);
        json_writer_write_string(json, "stop", journey->stop);
        json_writer_write_string(json, "destination", journey->destination);
        json_writer_write_int(json, "site-id", journey->site_id);
        json_writer_write_int(json, "mode", journey->mode);
        json_writer_write_int(json, "direction", journey->direction);
        json_writer_write_string(json, "next-update", format_date(buf, sizeof(buf), &journey->next_update));

        json_writer_begin_array(json, "departures");
        for(int i = 0; (i < JOURNEY_MAX_DEPARTURES) && (journey->departures[i] > 0); i++) {
            json_writer_write_string(json, NULL, format_date(buf, sizeof(buf), &journey->departures[i]));
        }
        json_writer_end_array(json);
    }
    json_writer_end_object(json);
}

void write_journies_status(struct json_writer *json)
{
    json_writer_begin_array(json, "journies");

    for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++) {
        write_journey_status(json, &journies[i]);
    }

    json_writer_end_array(json);
}

enum http_cgi_state cgi_status(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    http_begin_response(request, 200, "application/json");
    http_write_header(request, "Cache-Control", "no-cache");
    http_end_header(request);

    struct json_writer json;
    json_writer_http_init(&json, request);

    json_writer_begin_object(&json, NULL);

    write_system_status(&json);
    write_wifi_status(&json);
    write_time_status(&json);
    write_journies_status(&json);

    json_writer_end_object(&json);

    http_end_body(request);
    return HTTP_CGI_DONE;
}

enum http_cgi_state cgi_log(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    http_begin_response(request, 200, "application/json");
    http_write_header(request, "Cache-Control", "no-cache");
    http_end_header(request);

    struct json_writer json;
    json_writer_http_init(&json, request);

    json_writer_begin_array(&json, NULL);

    for(int tail = (log_cbuf.head + 1) % LOG_CBUF_LEN; tail != log_cbuf.head; tail = (tail + 1) % LOG_CBUF_LEN) {
        if(log_cbuf.message[tail].message[0]) {
            json_writer_begin_object(&json, NULL);

            json_writer_write_int(&json, "timestamp", log_cbuf.message[tail].timestamp);
            json_writer_write_int(&json, "level", log_cbuf.message[tail].level);
            json_writer_write_int(&json, "system", log_cbuf.message[tail].system);
            json_writer_write_string(&json, "message", log_cbuf.message[tail].message);

            json_writer_end_object(&json);
        }
    }

    json_writer_end_array(&json);

    http_end_body(request);
    return HTTP_CGI_DONE;
}


extern ip_addr_t syslog_addr;
extern int syslog_enabled;


enum http_cgi_state cgi_syslog_config(struct http_request* request)
{
    if(request->method == HTTP_METHOD_GET) {
        http_begin_response(request, 200, "application/json");
        http_write_header(request, "Cache-Control", "no-cache");
        http_end_header(request);

        struct json_writer json;
        json_writer_http_init(&json, request);

        json_writer_begin_object(&json, NULL);

        char buf[16];

        ipaddr_ntoa_r(&syslog_addr, buf, sizeof(buf));
        json_writer_write_string(&json, "ip", buf);

        json_writer_write_bool(&json, "enabled", syslog_enabled);

        json_writer_begin_array(&json, "systems");
        for(enum log_system system = 0; system < LOG_NUM_SYSTEMS; system++) {
            json_writer_write_string(&json, NULL, log_system_names[system]);
        }
        json_writer_end_array(&json);

        json_writer_begin_array(&json, "levels");
        for(enum log_level level = 0; level < LOG_NUM_LEVELS; level++) {
            json_writer_write_string(&json, NULL, log_level_names[level]);
        }
        json_writer_end_array(&json);

        json_writer_begin_array(&json, "system-levels");
        for(enum log_system system = 0; system < LOG_NUM_SYSTEMS; system++) {
            json_writer_write_int(&json, NULL, log_get_level(LOG_CBUF, system));
        }
        json_writer_end_array(&json);


        json_writer_end_object(&json);

        http_end_body(request);
        return HTTP_CGI_DONE;
    } else if(request->method == HTTP_METHOD_POST) {
        json_stream json;
        json_open_http(&json, request);

        if(json_expect(&json, JSON_OBJECT)) {

            ip_addr_t ip = syslog_addr;
            int enabled = syslog_enabled;
            enum log_level levels[LOG_NUM_SYSTEMS];

            uint8_t state = 0;

            enum {
                GOT_IP = 0x01,
                GOT_ENABLED = 0x02,
                GOT_LEVELS = 0x04,
                GOT_ERROR = 0x08,
            };

            int n;
            while((n = json_find_names(&json, (const char *[]) { "enabled", "ip", "system-levels"  }, 3)) >= 0) {

                switch(n) {
                case 0: // enabled
                {
                    enum json_type next = json_next(&json);

                    if(next == JSON_TRUE) {
                        enabled = 1;
                        state |= GOT_ENABLED;
                    } else if(next == JSON_FALSE) {
                        enabled = 0;
                        state |= GOT_ENABLED;
                    } else {
                        state |= GOT_ERROR;
                    }
                }
                break;

                case 1: // ip
                    if(json_expect(&json, JSON_STRING) && ipaddr_aton(json_get_string(&json, 0), &ip)) {
                        state |= GOT_IP;
                    } else {
                        state |= GOT_ERROR;
                    }
                    break;

                case 2: // system-levels

                    if(json_expect(&json, JSON_ARRAY)) {

                        enum log_system system = 0;
                        enum json_type type;
                        while((type = json_next(&json)) == JSON_NUMBER) {
                            if(system < LOG_NUM_SYSTEMS) {
                                levels[system] = json_get_number(&json);
                            }

                            system++;
                        }

                        if((type == JSON_ARRAY_END) && (system == LOG_NUM_SYSTEMS)) {
                            state |= GOT_LEVELS;
                        } else {
                            state |= GOT_ERROR;
                        }
                    } else {
                        state |= GOT_ERROR;
                    }
                    break;
                }
            }

            if(!(state & GOT_ERROR)) {
                if(state & GOT_IP) {
                    ip_addr_copy(syslog_addr, ip);
                }
                if(state & GOT_ENABLED) {
                    syslog_enabled = enabled;
                }
                if(state & GOT_LEVELS) {
                    for(enum log_system system = 0; system < LOG_NUM_SYSTEMS; system++) {
                        log_set_level(LOG_CBUF, system, levels[system]);
                    }
                }

                http_server_write_simple_response(request, 204, NULL, NULL);
                return HTTP_CGI_DONE;
            }
        }

        http_server_write_simple_response(request, 400, "application/json", "{\"message\" : \"Bad request\"}");
        return HTTP_CGI_DONE;
    }

    return HTTP_CGI_NOT_FOUND;
}
