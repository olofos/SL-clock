#include <esp_common.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <string.h>

#include "wifi-task.h"
#include "status.h"
#include "keys.h"
#include "log.h"

#define LOG_SYS LOG_SYS_WIFI

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)


xQueueHandle wifi_event_queue;

xSemaphoreHandle wifi_mutex = NULL;
xSemaphoreHandle wifi_scan_mutex = NULL;

static void wifi_handle_event_cb(System_Event_t *evt)
{
    uint8_t wifi_event = WIFI_EVENT_NO_EVENT;
    switch (evt->event_id) {
    case EVENT_STAMODE_GOT_IP:
        wifi_event = WIFI_EVENT_AP_CONNECTED;
        break;

    case EVENT_STAMODE_DISCONNECTED:
        printf("EVENT_STAMODE_DISCONNECTED: reason: %d\n", evt->event_info.disconnected.reason);
        switch(evt->event_info.disconnected.reason) {
        case REASON_AUTH_FAIL:
        case REASON_AUTH_EXPIRE:
        case REASON_HANDSHAKE_TIMEOUT:
        case REASON_ASSOC_EXPIRE:
            wifi_event = WIFI_EVENT_AP_CONNECTION_FAILED;
            break;

        case REASON_BEACON_TIMEOUT:
            wifi_event = WIFI_EVENT_AP_CONNECTION_LOST;
            break;

        case REASON_NO_AP_FOUND:
            wifi_event = WIFI_EVENT_AP_NOT_FOUND;
            break;

        default:
            wifi_event = WIFI_EVENT_ERROR;
            break;
        }
        break;

    case EVENT_STAMODE_CONNECTED:
        break;

    case EVENT_SOFTAPMODE_STACONNECTED:
        wifi_event = WIFI_EVENT_STA_CONNECTED;
        break;

    case EVENT_SOFTAPMODE_STADISCONNECTED:
        wifi_event = WIFI_EVENT_STA_DISCONNECTED;
        break;

    case EVENT_SOFTAPMODE_PROBEREQRECVED:
        break;

    default:
        printf("Unkown wifi event: %d\n", evt->event_id);
        break;
    }

    if(wifi_event != WIFI_EVENT_NO_EVENT) {
        if(!xQueueSend(wifi_event_queue, &wifi_event, 0)) {
            printf("Could not post event %d\n", wifi_event);
        }
    }
}

void wifi_ap_connect(const struct wifi_ap *ap)
{
    struct station_config station_config;

    memset(&station_config, 0, sizeof(station_config));

    strncpy((char *)&station_config.ssid, ap->ssid, WIFI_SSID_LEN);
    strncpy((char *)&station_config.password, ap->password, WIFI_PASS_LEN);

    wifi_station_set_config_current(&station_config);
    wifi_station_connect();
}

void wifi_ap_disconnect(void)
{
    wifi_station_disconnect();
}

static void wifi_softap_config(void)
{
    struct softap_config softap_config;
    memset(&softap_config, 0, sizeof(softap_config));

    strncpy((char *)softap_config.ssid, WIFI_SOFTAP_SSID, 32);
    strncpy((char *)softap_config.password, WIFI_SOFTAP_PASS, 64);
    softap_config.ssid_len = strlen(WIFI_SOFTAP_SSID);
    softap_config.channel = 4;
    softap_config.authmode = AUTH_WPA_PSK;
    softap_config.ssid_hidden = 0;
    softap_config.max_connection = 2;
    softap_config.beacon_interval = 100;

    wifi_softap_set_config_current(&softap_config);
}

void wifi_softap_enable(void)
{
    wifi_set_opmode(STATIONAP_MODE);
    wifi_softap_config();
}

void wifi_softap_disable(void)
{
    wifi_set_opmode(STATION_MODE);
}

volatile static uint8_t wifi_scan_in_progress;

static void wifi_scan_cb(void *arg, STATUS status)
{
    for(struct bss_info *bss = arg; bss != NULL; bss = STAILQ_NEXT(bss, next)) {
        wifi_scan_add((char*)bss->ssid, bss->rssi, bss->authmode);
    }
    wifi_scan_in_progress = 0;
}

void wifi_start_scan(void)
{
    if(!wifi_scan_in_progress) {
        wifi_scan_in_progress = 1;
        wifi_scan_free_all();
        wifi_station_scan(NULL, wifi_scan_cb);
    }
}

void wifi_take_mutex(void)
{
    for(;;) {
        if((wifi_mutex != NULL) && (xSemaphoreTake(wifi_mutex, portMAX_DELAY) == pdTRUE)) {
            break;
        }
        LOG("wifi_take_mutex failed");
    }
}

void wifi_give_mutex(void)
{
    xSemaphoreGive(wifi_mutex);
}


void wifi_task(void *pvParameters)
{
    LOG("Starting WiFi");

    wifi_mutex = xSemaphoreCreateMutex();
    wifi_scan_mutex = xSemaphoreCreateMutex();

    wifi_take_mutex();

    wifi_event_queue = xQueueCreate(8, 1);

    wifi_set_event_handler_cb(wifi_handle_event_cb);

    wifi_set_opmode(STATION_MODE);

    wifi_station_set_reconnect_policy(0);
    wifi_station_set_auto_connect(0);

    wifi_state_machine_init();

    wifi_give_mutex();

    uint8_t wifi_event = WIFI_EVENT_NO_EVENT;
    for(;;)
    {
        wifi_take_mutex();
        wifi_handle_event(wifi_event);
        wifi_give_mutex();

        if(!xQueueReceive(wifi_event_queue, &wifi_event, WIFI_TASK_TICK_RATE/portTICK_RATE_MS)) {
            wifi_event = WIFI_EVENT_NO_EVENT;
        }
    }
}
