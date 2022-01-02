#include <esp_common.h>
#include <freertos/semphr.h>
#include <string.h>

#include "json.h"
#include "json-util.h"
#include "json-http.h"
#include "status.h"
#include "http-sm/http.h"
#include "log.h"
#include "humidity.h"

#define LOG_SYS LOG_SYS_HUMIDITY

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

static xSemaphoreHandle humidity_mutex = NULL;

struct measurement *humidity_first_measurement = NULL;

static char hostname[32];

void humidity_set_hostname(const char *name)
{
    strncpy(hostname, name, sizeof(hostname)-1);
}

const char *humidity_get_hostname(void)
{
    return hostname;
}


void humidity_take_mutex(void)
{
    for(;;) {
        if((humidity_mutex != NULL) && (xSemaphoreTake(humidity_mutex, portMAX_DELAY) == pdTRUE)) {
            break;
        }
        LOG("humidity_take_mutex failed");
    }
}

int humidity_take_mutex_noblock(void)
{
    if((humidity_mutex != NULL) && (xSemaphoreTake(humidity_mutex, 0) == pdTRUE)) {
        return 1;
    }
    LOG("humidity_take_mutex failed");
    return 0;
}

void humidity_give_mutex(void)
{
    xSemaphoreGive(humidity_mutex);
}

static const char *format_time(time_t *tp)
{
    struct tm *tmp = localtime(tp);
    static char s[20];
    strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S%z", tmp);
    return s;
}


static void print_measurements(struct measurement *first)
{
    for(struct measurement *m = first; m; m = m->next) {
        LOG("Node id:         %d", m->node_id);
        if(m->name) {
            LOG("Name:            %s", m->name);
        }
        LOG("Timestamp:       %s", format_time(&m->timestamp));
        LOG("Temperature:     %f", m->temperature);
        LOG("Humidity:        %f", m->humidity);
        LOG("Battery 1 level: %f", m->battery1_level);
        LOG("Battery 2 level: %f", m->battery2_level);
    }
}

static int humidity_update(void)
{
    LOG("Updating humidity data");

    struct http_request request;
    http_request_init(&request);

    request.host = hostname;
    request.path = "/api/measurements/newest";
    request.port = 3001;

    humidity_take_mutex();

    if(http_get_request(&request) < 0) {
        LOG("http_get_request failed");
        humidity_give_mutex();
        return -1;
    }

    json_stream json;
    json_open_http(&json, &request);

    struct measurement *new = humidity_parse_json(&json);

    humidity_free(humidity_first_measurement);
    humidity_first_measurement = new;

    humidity_give_mutex();

    print_measurements(humidity_first_measurement);

    if (json_get_error(&json)) {
        LOG("JSON error %s", json_get_error(&json));
    }

    json_close(&json);
    http_close(&request);

    return 1;
}

void humidity_task(void *pvParameters)
{
    LOG("Humidity task starting");
    humidity_mutex = xSemaphoreCreateMutex();

    for(;;)
    {
        if(app_status.wifi_connected && app_status.obtained_time && app_status.obtained_tz) {
            humidity_update();
            vTaskDelayMs(60 * 1000);
        } else {
            vTaskDelayMs(100);
        }

    }
}
