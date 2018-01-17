#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#define WIFI_SSID_LEN 32
#define WIFI_PASS_LEN 64

void wifi_task(void *pvParameters);

extern char wifi_ssid[WIFI_SSID_LEN];
extern char wifi_pass[WIFI_PASS_LEN];



#endif
