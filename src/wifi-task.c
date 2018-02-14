#include <esp_common.h>
#include <string.h>

#include "wifi-task.h"
#include "status.h"
#include "log.h"

#define LOG_SYS LOG_SYS_WIFI

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

char wifi_ssid[WIFI_SSID_LEN];
char wifi_pass[WIFI_PASS_LEN];


static void wifi_handle_event_cb(System_Event_t *evt)
{
    switch (evt->event_id) {
         case EVENT_STAMODE_CONNECTED:
         case EVENT_STAMODE_DISCONNECTED:
             app_status.wifi_connected = 0;
             break;
         case EVENT_STAMODE_AUTHMODE_CHANGE:
             break;
         case EVENT_STAMODE_GOT_IP:
             app_status.wifi_connected = 1;
             break;
         case EVENT_SOFTAPMODE_STACONNECTED:
             break;
         case EVENT_SOFTAPMODE_STADISCONNECTED:
             break;
         default:
             break;
    }
}

void wifi_task(void *pvParameters)
{
    LOG("Starting WiFi");

    wifi_set_event_handler_cb(wifi_handle_event_cb);

    wifi_set_opmode(STATION_MODE);

    struct station_config station_config;

    memset(&station_config, 0, sizeof(station_config));

    if(wifi_first_ap) {
        strncpy((char *)&station_config.ssid, wifi_first_ap->ssid, WIFI_SSID_LEN);
        strncpy((char *)&station_config.password, wifi_first_ap->password, WIFI_PASS_LEN);
    }

    wifi_station_set_config(&station_config);
    wifi_station_connect();

    for(;;)
    {
        vTaskDelay(100000);
    }
}
