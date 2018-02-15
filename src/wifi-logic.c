#include "wifi-task.h"
#include "status.h"

#include "log.h"

#define LOG_SYS LOG_SYS_WIFI


enum wifi_state wifi_state;
struct wifi_ap *wifi_current_ap;
int wifi_ap_retries_left;

void wifi_state_machine_init(void)
{
    wifi_state = WIFI_STATE_NOT_CONNECTED;
    wifi_current_ap = wifi_first_ap;
    wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;

    app_status.wifi_connected = 0;
}

void wifi_handle_event(enum wifi_event event)
{
    switch(wifi_state) {
    case WIFI_STATE_NOT_CONNECTED:
        if(event == WIFI_EVENT_NO_EVENT) {
            if(wifi_current_ap) {
                LOG("WIFI_STATE_NOT_CONNECTED: trying to connect to %s", wifi_current_ap->ssid);
                wifi_ap_connect(wifi_current_ap);
                wifi_state = WIFI_STATE_AP_CONNECTING;
            } else {
                LOG("WIFI_STATE_NOT_CONNECTED: no AP found");
                wifi_state = WIFI_STATE_NO_AP_FOUND;
            }
        } else {
            LOG("WIFI_STATE_NOT_CONNECTED: unexpected event %d", event);
            wifi_state = WIFI_STATE_ERROR;
        }
        break;

    case WIFI_STATE_AP_CONNECTED:
        switch(event) {
        case WIFI_EVENT_NO_EVENT:
            break;

        case WIFI_EVENT_AP_CONNECTION_LOST:
            app_status.wifi_connected = 0;

            LOG("WIFI_STATE_AP_CONNECTED: connection lost, retrying");
            wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;
            wifi_state = WIFI_STATE_NOT_CONNECTED;
            break;

        default:
            LOG("WIFI_STATE_AP_CONNECTED: unexpected event %d", event);
            wifi_state = WIFI_STATE_ERROR;
            break;
        }
        break;

    case WIFI_STATE_AP_CONNECTING:
        switch(event) {
        case WIFI_EVENT_NO_EVENT:
            break;

        case WIFI_EVENT_AP_CONNECTED:
            app_status.wifi_connected = 1;

            LOG("WIFI_STATE_AP_CONNECTING: connected");
            wifi_state = WIFI_STATE_AP_CONNECTED;
            break;

        case WIFI_EVENT_AP_WRONG_PASSWORD:
            if(--wifi_ap_retries_left) {
                LOG("WIFI_STATE_AP_CONNECTING: wrong password, %d retries left", wifi_ap_retries_left);
            } else {
                wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;
                wifi_current_ap = wifi_current_ap->next;
                LOG("WIFI_STATE_AP_CONNECTING: wrong password, trying next AP");
            }
            wifi_state = WIFI_STATE_NOT_CONNECTED;
            break;

        case WIFI_EVENT_AP_NOT_FOUND:
            if(--wifi_ap_retries_left) {
                LOG("WIFI_STATE_AP_CONNECTING: AP not found, %d retries left", wifi_ap_retries_left);
            } else {
                wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;
                wifi_current_ap = wifi_current_ap->next;
                LOG("WIFI_STATE_AP_CONNECTING: AP not found, trying next AP");
            }
            wifi_state = WIFI_STATE_NOT_CONNECTED;
            break;

        default:
            LOG("WIFI_STATE_AP_CONNECTING: unexpected event %d", event);
            wifi_state = WIFI_STATE_ERROR;
            break;

        }
        break;



    case WIFI_STATE_NO_AP_FOUND:
        if(event == WIFI_EVENT_NO_EVENT) {
            LOG("No ap found -- should probably start our own AP");
            LOG("For now we just start over");

            wifi_current_ap = wifi_first_ap;
            wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;
            wifi_state = WIFI_STATE_NOT_CONNECTED;
        } else {
            LOG("WIFI_STATE_NO_AP_FOUND: unexpected event %d", event);
            wifi_state = WIFI_STATE_ERROR;
        }
        break;

    case WIFI_STATE_ERROR:
        if(event == WIFI_EVENT_NO_EVENT) {
            LOG("There was an error, resetting");

            app_status.wifi_connected = 0;

            wifi_current_ap = wifi_first_ap;
            wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;
            wifi_state = WIFI_STATE_NOT_CONNECTED;
        } else {
            LOG("WIFI_STATE_ERROR: unexpected event %d", event);
            wifi_state = WIFI_STATE_ERROR;
        }
        break;
    }
}
