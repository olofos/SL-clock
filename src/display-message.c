#include <esp_common.h>
#include <stdio.h>

#include "display-message.h"
#include "display.h"
#include "keys.h"

#include "log.h"
#define LOG_SYS LOG_SYS_DISPLAY

void display_post_message(enum display_message message)
{
    if(display_message_queue) {
        uint8_t msg = message;
        xQueueSend(display_message_queue, &msg, 0);
    } else {
        LOG("Trying to post a message before the queue was created");
    }
}

const char *display_get_message(enum display_message message)
{
    static char buf[64];
    struct ip_info ipconfig;

    switch(message) {
    case DISPLAY_MESSAGE_NO_WIFI:
        snprintf(buf, sizeof(buf), "WIFI not found\nConnect to\n" WIFI_SOFTAP_SSID "\nto configure");
        break;

    case DISPLAY_MESSAGE_NO_JOURNIES:
        wifi_get_ip_info(STATION_IF, &ipconfig);
        snprintf(buf, sizeof(buf), "No journies configured\nConnect to\n" IPSTR "\nto configure", IP2STR(&ipconfig.ip));
        break;

    case DISPLAY_MESSAGE_NONE:
        LOG("Displaying emty message!");
        snprintf(buf, sizeof(buf), "No message!\nThis should not be displayed");
        break;

    default:
        LOG("Displaying unkown message!");
        snprintf(buf, sizeof(buf), "Unkown message!\nThis should not be displayed");
        break;
    }

    return buf;
}
