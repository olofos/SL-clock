#include <string.h>
#include <stdlib.h>

#include "wifi-task.h"
#include "log.h"

#define LOG_SYS LOG_SYS_WIFI


struct wifi_ap *wifi_first_ap = 0;
struct wifi_scan_ap *wifi_first_scan_ap = 0;


void wifi_ap_free(struct wifi_ap *ap)
{
    free(ap->ssid);
    free(ap->password);
    free(ap);
}

struct wifi_ap *wifi_ap_alloc(const char *ssid, const char *pass)
{
    struct wifi_ap *ap = malloc(sizeof(*ap));
    ap->ssid = malloc(strlen(ssid)+1);
    ap->password = malloc(strlen(pass)+1);

    strcpy(ap->ssid, ssid);
    strcpy(ap->password, pass);

    return ap;
}

void wifi_ap_remove(const char *ssid)
{
    if(!wifi_first_ap) {
        return;
    }

    if(strcmp(wifi_first_ap->ssid, ssid) == 0) {
        struct wifi_ap *next = wifi_first_ap->next;
        wifi_ap_free(wifi_first_ap);
        wifi_first_ap = next;

        return;
    }

    struct wifi_ap *prev = wifi_first_ap;

    for(;;) {
        if(!prev->next) {
            return;
        }

        if(strcmp(prev->next->ssid, ssid) == 0) {
            break;
        }

        prev = prev->next;
    }

    struct wifi_ap *next = prev->next->next;
    wifi_ap_free(prev->next);
    prev->next = next;
}

void wifi_ap_add(const char *ssid, const char *pass)
{
    if(!ssid || !pass) {
        return;
    }

    struct wifi_ap *new_ap = wifi_ap_alloc(ssid, pass);

    wifi_ap_remove(ssid);
    new_ap->next = wifi_first_ap;
    wifi_first_ap = new_ap;
}

void wifi_ap_add_back(const char *ssid, const char *pass)
{
    if(!ssid || !pass) {
        return;
    }

    struct wifi_ap *new_ap = wifi_ap_alloc(ssid, pass);
    new_ap->next = 0;

    if(!wifi_first_ap) {
        wifi_first_ap = new_ap;
        return;
    }

    struct wifi_ap *ap = wifi_first_ap;

    while(ap->next) {
        ap = ap->next;
    }

    ap->next = new_ap;
}

uint16_t wifi_ap_number(void)
{
    uint16_t n = 0;

    for(const struct wifi_ap *ap = wifi_first_ap; ap; ap = ap->next) {
        n++;
    }

    return n;
}

const char *wifi_ap_ssid(uint16_t n)
{
    struct wifi_ap *ap = wifi_first_ap;

    for(int i = 0; (ap != NULL) && (i < n); i++) {
        ap = ap->next;
    }

    if(!ap) {
        return 0;
    }

    return ap->ssid;
}

const char *wifi_ap_password(uint16_t n)
{
    struct wifi_ap *ap = wifi_first_ap;

    for(int i = 0; (ap != NULL) && (i < n); i++) {
        ap = ap->next;
    }

    if(!ap) {
        return 0;
    }

    return ap->password;
}



void wifi_scan_add(const char *ssid, int8_t rssi, uint8_t authmode)
{
    struct wifi_scan_ap *new_ap = malloc(sizeof(struct wifi_scan_ap));

    new_ap->ssid = malloc(strlen(ssid) + 1);
    strcpy(new_ap->ssid, ssid);
    new_ap->rssi = rssi;
    new_ap->authmode = authmode;

    new_ap->next = wifi_first_scan_ap;
    wifi_first_scan_ap = new_ap;
}

void wifi_scan_free_all(void)
{
    struct wifi_scan_ap *ap = wifi_first_scan_ap;

    while(ap) {
        struct wifi_scan_ap *next = ap->next;

        free(ap->ssid);
        free(ap);

        ap = next;
    }

    wifi_first_scan_ap = 0;
}
