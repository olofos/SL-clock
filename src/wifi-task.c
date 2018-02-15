#include <esp_common.h>
#include <freertos/queue.h>
#include <string.h>

#include "wifi-task.h"
#include "log.h"

#define LOG_SYS LOG_SYS_WIFI

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)


xQueueHandle wifi_event_queue;

static void wifi_handle_event_cb(System_Event_t *evt)
{
    uint8_t wifi_event;
    switch (evt->event_id) {
    case EVENT_STAMODE_GOT_IP:
        wifi_event = WIFI_EVENT_AP_CONNECTED;
        xQueueSend(wifi_event_queue, &wifi_event, 0);
        break;

    case EVENT_STAMODE_DISCONNECTED:
        switch(evt->event_info.disconnected.reason) {
        case REASON_AUTH_FAIL:
        case REASON_AUTH_EXPIRE:
            wifi_event = WIFI_EVENT_AP_WRONG_PASSWORD;
            xQueueSend(wifi_event_queue, &wifi_event, 0);
            break;

        case REASON_BEACON_TIMEOUT:
            wifi_event = WIFI_EVENT_AP_CONNECTION_LOST;
            xQueueSend(wifi_event_queue, &wifi_event, 0);
            break;

        case REASON_NO_AP_FOUND:
            wifi_event = WIFI_EVENT_AP_NOT_FOUND;
            xQueueSend(wifi_event_queue, &wifi_event, 0);
            break;

        default:
            printf("EVENT_STAMODE_DISCONNECTED: Unhandled reason: %d\n", evt->event_info.disconnected.reason);
            break;
        }
        break;

    case EVENT_STAMODE_CONNECTED:
        break;

    case EVENT_SOFTAPMODE_STACONNECTED:
        break;

    case EVENT_SOFTAPMODE_STADISCONNECTED:
        break;


    case EVENT_SOFTAPMODE_PROBEREQRECVED:
        break;

    default:
        printf("Unkown wifi event: %d\n", evt->event_id);
        break;
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

void wifi_task(void *pvParameters)
{
    LOG("Starting WiFi");

    wifi_event_queue = xQueueCreate(8, 1);

    wifi_set_event_handler_cb(wifi_handle_event_cb);

    wifi_set_opmode(STATION_MODE);

    wifi_station_set_reconnect_policy(0);
    wifi_station_set_auto_connect(0);

    wifi_state_machine_init();

    uint8_t wifi_event = WIFI_EVENT_NO_EVENT;
    for(;;)
    {
        wifi_handle_event(wifi_event);

        if(!xQueueReceive(wifi_event_queue, &wifi_event, 500/portTICK_RATE_MS)) {
            wifi_event = WIFI_EVENT_NO_EVENT;
        } else {
            LOG("received event: %d", wifi_event);
        }
    }
}
