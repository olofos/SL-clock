#include <esp_common.h>
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
    switch(message) {
    case DISPLAY_MESSAGE_NO_WIFI:
        return "WIFI not found\nConnect to\n" WIFI_SOFTAP_SSID "\nto configure";

    case DISPLAY_MESSAGE_NO_JOURNIES:
        return "No journies configured";

    case DISPLAY_MESSAGE_NONE:
        LOG("Displaying emty message!");
        return "No message!\nThis should not be displayed";

    default:
        LOG("Displaying unkown message!");
        return "Unkown message!\nThis should not be displayed";
    }
}
