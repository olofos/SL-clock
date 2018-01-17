#include <esp_common.h>
#include <string.h>

#include "wifi-task.h"
#include "status.h"
#include "keys.h"
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

    struct station_config config;

    memset(&config, 0, sizeof(config));

    strncpy((char *)&config.ssid, wifi_ssid, WIFI_SSID_LEN);
    strncpy((char *)&config.password, wifi_pass, WIFI_PASS_LEN);


    wifi_set_opmode(STATION_MODE);
    wifi_station_set_config(&config);

    wifi_station_connect();

    for(;;)
    {
        vTaskDelay(100000);
    }
}
