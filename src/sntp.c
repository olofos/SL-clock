#include <stdint.h>
#include <string.h>
#include <lwip/api.h>
#include <esp_common.h>
#include <sys/time.h>

#include "sntp.h"
#include "status.h"
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

static uint64_t sntp_base;
static uint32_t sntp_tim_ref, sntp_cal;


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
    int64_t sntp_current = sntp_base + TIMER_COUNT - sntp_tim_ref;
    int64_t sntp_correct = (((uint64_t)us + (uint64_t)t * 1000000U)<<12) / sntp_cal;

    if((time_t)(sntp_correct - sntp_current) > 1<<30)
    {
        LOG("Adjust: drift = %d000 ticks, cal = %d", (time_t)(sntp_correct - sntp_current)/1000, (uint32_t)sntp_cal);
    } else {
        LOG("Adjust: drift = %d ticks, cal = %d", (int)(sntp_correct - sntp_current), (uint32_t)sntp_cal);
    }

    sntp_tim_ref = TIMER_COUNT;
    sntp_cal = system_rtc_clock_cali_proc();

    sntp_base = (((uint64_t)us + (uint64_t)t * 1000000U)<<12) / sntp_cal;
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

    LOG("Sending SNTP request");

    err = netconn_gethostbyname(server, &addr);

    if(err != ERR_OK)
    {
        ERROR("DNS error: %d", err);
        return err;
    }

    conn = netconn_new(NETCONN_UDP);
    send_buf = netbuf_new();
    request = netbuf_alloc(send_buf, SNTP_MAX_DATA_LEN);

    if(!conn || !send_buf || !request)
    {
        ERROR("Netconn or netbuf or data allocation failed.");
        netbuf_delete(send_buf);
        netconn_delete(conn);
        return err;
    }

    err = netconn_connect(conn, &addr, SNTP_PORT);

    if(err != ERR_OK)
    {
        ERROR("Netconn connect to server %X, port %u failed with %d", addr.addr, SNTP_PORT, err);
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
        ERROR("Netconn send failed with %d", err);
        netconn_delete(conn);
        return err;
    }

    netconn_set_recvtimeout(conn, SNTP_RECV_TIMEOUT);

    err = netconn_recv(conn, &recv_buf);

    if(err != ERR_OK)
    {
        ERROR("Netconn receive failed with %d", err);
        netconn_delete(conn);
        return err;
    }

    netbuf_data(recv_buf, (void**) &response, (uint16_t *) &len);

    if(len == SNTP_MAX_DATA_LEN)
    {
        if(response->stratum == 0)
        {
            ERROR("Got KOD");
            return SNTP_ERR_KOD;
        }

        if((response->li_vn_mode & 0x38) != SNTP_VERSION) {
            ERROR("Wrong version in reply: expected %d but got %d", SNTP_VERSION >> 3, (response->li_vn_mode & 0x38) >> 3);
        }

        if((response->li_vn_mode & 0x07) != SNTP_MODE_SERVER) {
            ERROR("Wrong mode in reply: expected %x but got %x", SNTP_MODE_SERVER, response->li_vn_mode & 0x07);
        }

        if((response->li_vn_mode & 0xC0) == 0xC0) {
            WARNING("Server not synchronised");
        }

        if((response->transmit_timestamp[0] == 0) && (response->transmit_timestamp[1] == 0))  {
            ERROR("Transmit timestamp is zero");
        }

        uint32_t t = ntohl(response->receive_timestamp[0]) - UNIX_EPOCH;
        uint32_t us = ntohl(response->receive_timestamp[1]) / 4295;

        INFO("Stratum: %d", response->stratum);
        INFO("Got timestamp %lu", t);

        time_t old_time = time(0);

        if(app_status.obtained_time) {
            if(old_time > t &&  old_time - t > 60) {
                WARNING("Time is going backwards: %d %d", old_time, t);
            } else if(t > old_time && t - old_time > 60) {
                WARNING("More than one minute of lag");
            }
        }

        sntp_update_rtc(t, us);

        char buf[32];

        time_t new_time = time(0);

        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&old_time));
        LOG("Old time: %s", buf);

        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&new_time));
        LOG("New time: %s", buf);

        app_status.obtained_time = 1;
    } else {
        WARNING("Length of data did not match SNTP_MAX_DATA_LEN, received len %u", len);

        return err;
    }

    netbuf_delete(recv_buf);
    netconn_delete(conn);

    return ERR_OK;
}


inline static time_t sntp_get_rtc_time(int32_t *us) {
    time_t secs;
    uint32_t tim;
    uint64_t base;

    tim = TIMER_COUNT;
    base = sntp_base + tim - sntp_tim_ref;
    secs = base * sntp_cal / (1000000U<<12);
    if (us) {
        *us = base * sntp_cal % (1000000U<<12);
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
    sntp_cal = system_rtc_clock_cali_proc();
    sntp_tim_ref = TIMER_COUNT;
}


void sntp_task(void *param)
{
    int delay = SNTP_DELAY;
    const char *servers[] = {SNTP_SERVERS};

    sntp_init();

    for(;;)
    {
        while (!app_status.wifi_connected) {
            vTaskDelayMs(100);
        }

        int ret = sntp_request(servers[0]);
        if(ret == SNTP_ERR_KOD)
        {
            // TODO
            if(delay < 3600)
            {
                delay *= 2;
            }
        } else if(ret == ERR_TIMEOUT) {
            delay = 5;
        } else {
            delay = SNTP_DELAY;
        }

        vTaskDelayMs(delay * 1000);
    }
}
