#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <stdint.h>

#define WIFI_SSID_LEN 32
#define WIFI_PASS_LEN 64

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

extern char wifi_ssid[WIFI_SSID_LEN];
extern char wifi_pass[WIFI_PASS_LEN];



#endif
