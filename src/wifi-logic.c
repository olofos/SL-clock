#include "wifi-task.h"
#include "status.h"
#include "display-message.h"
#include "config.h"
#include "log.h"

#define LOG_SYS LOG_SYS_WIFI

#define WIFI_SOFTAP_TIMEOUT (10 * 1000 / WIFI_TASK_TICK_RATE)
#define WIFI_INFO_MESSAGE_TIMEOUT (15 * 1000 / WIFI_TASK_TICK_RATE)

enum wifi_state wifi_state;
struct wifi_ap *wifi_current_ap;
int wifi_ap_retries_left;
int wifi_softap_num_connected_stations = 0;
int wifi_softap_enabled = 0;
int wifi_softap_timeout;

static int wifi_info_message_timeout;

void wifi_state_machine_init(void)
{
    wifi_state = WIFI_STATE_NOT_CONNECTED;
    wifi_current_ap = wifi_first_ap;
    wifi_ap_retries_left = WIFI_AP_NUMBER_OF_RETRIES;

    app_status.wifi_connected = 0;
}

void wifi_handle_event(enum wifi_event event)
{

    if(event == WIFI_EVENT_STA_CONNECTED) {
        LOG("A station connected to the soft AP");

        wifi_softap_num_connected_stations++;
        return;
    } else if(event == WIFI_EVENT_STA_DISCONNECTED) {
        LOG("A station disconnected from the soft AP");

        if(wifi_softap_num_connected_stations > 0) {
            wifi_softap_num_connected_stations--;
        } else {
            ERROR("There were no connected stations!");
        }
        return;
    } else if(event == WIFI_EVENT_ERROR) {
        wifi_state = WIFI_STATE_ERROR;
        return;
    }

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
            if(wifi_softap_enabled) {
                if(wifi_softap_num_connected_stations > 0) {
                    wifi_softap_timeout = WIFI_SOFTAP_TIMEOUT;
                } else {
                    if(wifi_softap_timeout > 0) {
                        wifi_softap_timeout--;
                    } else {
                        LOG("WIFI_STATE_AP_CONNECTED: disabling soft AP");

                        wifi_softap_disable();
                        wifi_softap_enabled = 0;
                    }
                }
            }

            if(wifi_info_message_timeout > 0) {
                wifi_info_message_timeout--;
                if(!wifi_info_message_timeout) {
                    display_post_message(DISPLAY_MESSAGE_NONE);
                }
            }
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

            display_post_message(DISPLAY_MESSAGE_WIFI_INFO);
            wifi_info_message_timeout = WIFI_INFO_MESSAGE_TIMEOUT;

            LOG("WIFI_STATE_AP_CONNECTING: connected");
            wifi_state = WIFI_STATE_AP_CONNECTED;
            break;

        case WIFI_EVENT_AP_CONNECTION_FAILED:
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
            if(!wifi_softap_enabled) {
                LOG("WIFI_STATE_NO_AP_FOUND: enabling soft AP");

                display_post_message(DISPLAY_MESSAGE_NO_WIFI);

                wifi_softap_enable();
                wifi_softap_enabled = 1;
            }
            LOG("WIFI_STATE_NO_AP_FOUND: retrying");


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
            WARNING("There was an error, resetting");

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
