#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <stdint.h>

#define WIFI_SSID_LEN 32
#define WIFI_PASS_LEN 64

#define WIFI_AP_NUMBER_OF_RETRIES 3

enum wifi_event
{
    WIFI_EVENT_NO_EVENT = 0,
    WIFI_EVENT_AP_CONNECTED,
    WIFI_EVENT_AP_CONNECTION_FAILED,
    WIFI_EVENT_AP_NOT_FOUND,
    WIFI_EVENT_AP_CONNECTION_LOST,

    WIFI_EVENT_STA_CONNECTED,
    WIFI_EVENT_STA_DISCONNECTED,

    WIFI_EVENT_ERROR,
};

enum wifi_state
{
    WIFI_STATE_NOT_CONNECTED = 0,
    WIFI_STATE_AP_CONNECTED,
    WIFI_STATE_AP_CONNECTING,
    WIFI_STATE_NO_AP_FOUND,

    WIFI_STATE_ERROR,
};


// SS/a/n = State/AP/retry
//
//        |  NO_EVENT   AP_CONNECTED   AP_WRONG_PW   AP_NOT_FOUND   AP_CONN_LOST
// -------+----------------------------------------------------------------------
// NC/a/n |  CG/a/n     ER/a/n         ER/a/n        ER/a/n         ER/a/n
// NC/0/n |  NF/0/n     ER/0/n         ER/0/n        ER/0/n         ER/0/n
//        |
// CG/a/> |  CG/a/n     CD/a/n         NC/a/-        NC/a/-         ER/a/n
// CG/a/0 |  CG/a/0     CD/a/0         NC/+/N        NC/+/N         ER/a/0
// CG/0/n |  ER/0/n     ER/0/n         ER/0/n        ER/0/n         ER/0/n
//        |
// CD/a/n |  CD/a/n     ER/a/n         ER/a/n        ER/a/n         NC/a/N
//        |
// NF/a/n |  NC/A/N     ER/a/n         ER/a/n        ER/a/n         ER/a/n
//        |
// ER/a/n |  NC/A/N     ER/a/n         ER/a/n        ER/a/n         ER/a/n


struct wifi_ap
{
    char *ssid;
    char *password;

    struct wifi_ap *next;
};

extern struct wifi_ap *wifi_first_ap;

void wifi_task(void *pvParameters);

void wifi_ap_add(const char *ssid, const char *pass);
void wifi_ap_add_back(const char *ssid, const char *pass);

void wifi_ap_remove(const char *ssid);
uint16_t wifi_ap_number(void);
const char *wifi_ap_ssid(uint16_t n);
const char *wifi_ap_password(uint16_t n);

void wifi_ap_free(struct wifi_ap *ap);

void wifi_ap_connect(const struct wifi_ap *ap);

void wifi_state_machine_init(void);
void wifi_handle_event(enum wifi_event event);

void wifi_softap_enable(void);
void wifi_softap_disable(void);


#endif
