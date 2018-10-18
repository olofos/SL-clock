#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lwip/api.h>
#include <esp_common.h>

#include "status.h"
#include "syslog.h"
#include "log.h"

#define LOG_SYS LOG_SYS_LOG

ip_addr_t syslog_addr;
int syslog_enabled;

static const char *syslog_domain_name = "sl-clock";


static err_t syslog_send_package(struct log_cbuf_message *message) {
    struct netconn *conn;
    struct netbuf *send_buf;
    char *request;
    err_t err;

    conn = netconn_new(NETCONN_UDP);
    send_buf = netbuf_new();
    request = netbuf_alloc(send_buf, 5 + strlen(syslog_domain_name) + 1 + strlen(log_system_names[message->system]) + 2 + strlen(message->message));

    if(!conn || !send_buf || !request)
    {
        ERROR("Netconn or netbuf or data allocation failed.");
        netbuf_delete(send_buf);
        netconn_delete(conn);
        return ERR_MEM;
    }

    err = netconn_connect(conn, &syslog_addr, 514);

    if(err != ERR_OK)
    {
        ERROR("Netconn connect to server %X, port %u failed with %d", syslog_addr.addr, 514, err);
        netbuf_delete(send_buf);
        netconn_delete(conn);
        return err;
    }

    char *buf = request;

    *buf++ = '<';
    *buf++ = '1';
    *buf++ = '6';
    *buf++ = '0' + message->level;
    *buf++ = '>';

    memcpy(buf, syslog_domain_name, strlen(syslog_domain_name));
    buf += strlen(syslog_domain_name);

    *buf++ = ' ';
    memcpy(buf, log_system_names[message->system], strlen(log_system_names[message->system]));
    buf += strlen(log_system_names[message->system]);
    *buf++ = ':';
    *buf++ = ' ';

    memcpy(buf, message->message, strlen(message->message));

    err = netconn_send(conn, send_buf);
    netbuf_delete(send_buf);
    netconn_delete(conn);

    if(err != ERR_OK)
    {
        ERROR("Netconn send failed with %d", err);
    }

    return err;
}


void syslog_task(void *param)
{
    IP4_ADDR(&syslog_addr,192,168,10,249);

    for(;;) {
        if(app_status.wifi_connected && syslog_enabled) {
            while(log_cbuf.tail != log_cbuf.head) {
                syslog_send_package(&log_cbuf.message[log_cbuf.tail]);
                log_cbuf.tail = (log_cbuf.tail + 1) % LOG_CBUF_LEN;

                vTaskDelay(10);
            }
        }

        vTaskDelay((500)/portTICK_RATE_MS);
    }
}
