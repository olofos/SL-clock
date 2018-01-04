#include <stdint.h>
#include <string.h>
#include <lwip/api.h>
#include <esp_common.h>
#include <sys/time.h>

#include "sntp.h"
#include "log.h"

#define LOG_SYS LOG_SYS_SNTP

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

#define SNTP_PORT                   123
#define SNTP_RECV_TIMEOUT           3000
#define SNTP_MAX_DATA_LEN           48
#define SNTP_LI_NO_WARNING          0x00
#define SNTP_VERSION               (4/* NTP Version 4*/<<3)
#define SNTP_MODE_CLIENT            0x03
#define SNTP_MODE_SERVER            0x04
#define SNTP_MODE_BROADCAST         0x05
#define SNTP_MODE_MASK              0x07

#define SNTP_ERR_KOD                1

#define UNIX_EPOCH                  2208988800UL

//#define TIMER_COUNT	            RTC.COUNTER
#define TIMER_COUNT (*((uint32_t*)RTC_SLP_CNT_VAL))

// daylight settings
// Base calculated with value obtained from NTP server (64 bits)
//#define sntp_base	(*((uint64_t*)RTC_SCRATCH0))
// Timer value when base was obtained
//#define tim_ref 	(*((uint32_t*)RTC_SCRATCH2))
// Calibration value
//#define cal             (*((uint32_t*)RTC_SCRATCH3))

uint64_t sntp_base;
uint32_t tim_ref, cal;


struct ntp_packet
{
    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t reference_identifier;
    uint32_t reference_timestamp[2];
    uint32_t originate_timestamp[2];
    uint32_t receive_timestamp[2];
    uint32_t transmit_timestamp[2];
} __attribute__ ((packed));

// Update RTC timer. Called by SNTP module each time it receives an update.
static void sntp_update_rtc(time_t t, uint32_t us) {
    // DEBUG: Compute and print drift
    int64_t sntp_current = sntp_base + TIMER_COUNT - tim_ref;
    int64_t sntp_correct = (((uint64_t)us + (uint64_t)t * 1000000U)<<12) / cal;

    if((time_t)(sntp_correct - sntp_current) > 1<<30)
    {
        LOG("Adjust: drift = %d000 ticks, cal = %d", (time_t)(sntp_correct - sntp_current)/1000, (uint32_t)cal);
    } else {
        LOG("Adjust: drift = %d ticks, cal = %d", (int)(sntp_correct - sntp_current), (uint32_t)cal);
    }

    tim_ref = TIMER_COUNT;
    cal = system_rtc_clock_cali_proc();

    sntp_base = (((uint64_t)us + (uint64_t)t * 1000000U)<<12) / cal;
}

static int sntp_request(const char *server)
{
    ip_addr_t addr;
    err_t err;
    struct netconn *conn;
    struct netbuf *send_buf;
    struct netbuf *recv_buf;
    struct ntp_packet *request;
    struct ntp_packet *response;
    uint16_t len;

    err = netconn_gethostbyname(server, &addr);

    if(err != ERR_OK)
    {
        LOG("DNS error: %d", err);
        return err;
    }

    conn = netconn_new(NETCONN_UDP);
    send_buf = netbuf_new();
    request = netbuf_alloc(send_buf, SNTP_MAX_DATA_LEN);

    if(!conn || !send_buf || !request)
    {
        LOG("Netconn or netbuf or data allocation failed.");
        netbuf_delete(send_buf);
        netconn_delete(conn);
        return err;
    }

    err = netconn_connect(conn, &addr, SNTP_PORT);

    if(err != ERR_OK)
    {
        LOG("Netconn connect to server %X, port %u failed with %d", addr.addr, SNTP_PORT, err);
        netbuf_delete(send_buf);
        netconn_delete(conn);
        return err;
    }

    memset(request, 0, SNTP_MAX_DATA_LEN);
    request->li_vn_mode = SNTP_LI_NO_WARNING | SNTP_VERSION | SNTP_MODE_CLIENT;

    err = netconn_send(conn, send_buf);
    netbuf_delete(send_buf);

    if(err != ERR_OK)
    {
        LOG("Netconn send failed with %d", err);
        netconn_delete(conn);
        return err;
    }

    netconn_set_recvtimeout(conn, SNTP_RECV_TIMEOUT);

    err = netconn_recv(conn, &recv_buf);

    if(err != ERR_OK)
    {
        LOG("Netconn receive failed with %d", err);
        netconn_delete(conn);
        return err;
    }

    netbuf_data(recv_buf, (void**) &response, (uint16_t *) &len);

    if(len == SNTP_MAX_DATA_LEN)
    {
        if(response->stratum == 0)
        {
            return SNTP_ERR_KOD;
        }

        uint32_t t = ntohl(response->receive_timestamp[0]) - UNIX_EPOCH;
        uint32_t us = ntohl(response->receive_timestamp[1]) / 4295;

        LOG("Stratum: %d", response->stratum);
        LOG("Got timestamp %lu", t);

        sntp_update_rtc(t, us);
    } else {
        LOG("Length of data did not match SNTP_MAX_DATA_LEN, received len %u", len);

        return err;
    }

    netbuf_delete(recv_buf);
    netconn_delete(conn);

    return ERR_OK;
}


inline time_t sntp_get_rtc_time(int32_t *us) {
    time_t secs;
    uint32_t tim;
    uint64_t base;

    tim = TIMER_COUNT;
    base = sntp_base + tim - tim_ref;
    secs = base * cal / (1000000U<<12);
    if (us) {
        *us = base * cal % (1000000U<<12);
    }
    return secs;
}


int _gettimeofday_r(struct _reent *r, struct timeval *tp, void *tzp) {
    (void)r;
    // Syscall defined by xtensa newlib defines tzp as void*
    // So it looks like it is not used. Also check tp is not NULL
    if (tzp || !tp) return EINVAL;

    tp->tv_sec = sntp_get_rtc_time((int32_t*)&tp->tv_usec);
    return 0;
}

static void sntp_init(void)
{
    cal = system_rtc_clock_cali_proc();;
    tim_ref = TIMER_COUNT;
}


void sntp_task(void *param)
{
    int delay = SNTP_DELAY;
    const char *servers[] = {SNTP_SERVERS};

    sntp_init();

    for(;;)
    {
        /* Wait until we have joined AP and are assigned an IP */
        while (wifi_station_get_connect_status() != STATION_GOT_IP) {
            vTaskDelayMs(100);
        }

        if(sntp_request(servers[0]) == SNTP_ERR_KOD)
        {
            // TODO
            if(delay < 3600)
            {
                delay *= 2;
            }
        }

        LOG("xPortGetFreeHeapSize: %d", xPortGetFreeHeapSize());
        LOG("uxTaskGetStackHighWaterMark: %u", (unsigned)uxTaskGetStackHighWaterMark(0));

        vTaskDelayMs(delay * 1000);
    }
}
