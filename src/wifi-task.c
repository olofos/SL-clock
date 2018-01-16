#include <esp_common.h>

#include "wifi-task.h"
#include "status.h"
#include "keys.h"

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

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
    printf("Starting Wi-Fi Controller...\n");

    wifi_set_event_handler_cb(wifi_handle_event_cb);

    struct station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    wifi_set_opmode(STATION_MODE);
    wifi_station_set_config(&config);

    // for(int i = 0; i < 3; i++)
    // {
    //     wifi_station_disconnect();
    //     vTaskDelayMs(3000);
    // }

    wifi_station_connect();

    for(;;)
    {
        vTaskDelay(100000);
    }
}
