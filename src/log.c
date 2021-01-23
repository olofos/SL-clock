#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"

static enum log_level log_levels[LOG_NUM_DESTS][LOG_NUM_SYSTEMS];

const char* log_level_names[LOG_NUM_LEVELS] = {
    [LOG_LEVEL_EMERGENCY] = "EMERG",
    [LOG_LEVEL_ALERT] = "ALERT",
    [LOG_LEVEL_CRITICAL] = "CRIT",
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_WARNING] = "WARNING",
    [LOG_LEVEL_NOTICE] = "NOTICE",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_DEBUG] = "DEBUG",
};

const char* log_system_names[LOG_NUM_SYSTEMS] = {
    [LOG_SYS_RTOS] = "RTOS",
    [LOG_SYS_SDK] = "SDK",
    [LOG_SYS_MAIN] = "main",
    [LOG_SYS_RTC] = "RTC",
    [LOG_SYS_SNTP] = "SNTP",
    [LOG_SYS_HTTP] = "HTTP",
    [LOG_SYS_HTTPD] = "HTTPD",
    [LOG_SYS_JOURNEY] = "JOURNEY",
    [LOG_SYS_TZDB] = "TZDB",
    [LOG_SYS_JSON] = "JSON",
    [LOG_SYS_SH1106] = "SH1106",
    [LOG_SYS_MATRIX] = "LED",
    [LOG_SYS_WIFI] = "WIFI",
    [LOG_SYS_CONFIG] = "CONF",
    [LOG_SYS_DISPLAY] = "DISP",
    [LOG_SYS_LOG] = "LOG",
    [LOG_SYS_HUMIDITY] = "HUMIDITY",
};

typedef void (*log_func_t)(enum log_level, enum log_system, const char *);

void log_stdout(enum log_level level, enum log_system system, const char *msg);
void log_cbuf_writer(enum log_level level, enum log_system system, const char *msg);

log_func_t log_funcs[] = {
    [LOG_STDOUT] = log_stdout,
    [LOG_CBUF] = log_cbuf_writer,
};


void log_stdout(enum log_level level, enum log_system system, const char *msg)
{
    printf("[%s] (%s): %s\n", log_level_names[level], log_system_names[system], msg);
}

struct log_cbuf log_cbuf;

void log_cbuf_writer(enum log_level level, enum log_system system, const char *msg)
{
    log_cbuf.message[log_cbuf.head].level = level;
    log_cbuf.message[log_cbuf.head].system = system;
    log_cbuf.message[log_cbuf.head].timestamp = time(0);
    strncpy(log_cbuf.message[log_cbuf.head].message, msg, LOG_CBUF_STRLEN);

    log_cbuf.head = (log_cbuf.head + 1) % LOG_CBUF_LEN;

    if(log_cbuf.head == log_cbuf.tail) {
        log_cbuf.tail = (log_cbuf.tail + 1) % LOG_CBUF_LEN;
    }
}

void log_string(enum log_level level, enum log_system system, const char *msg)
{
    for(int i = 0; i < LOG_NUM_DESTS; i++)
    {
        if(log_levels[i][system] >= level)
        {
            log_funcs[i](level, system, msg);
        }
    }
}

void log_vlog(enum log_level level, enum log_system system, const char *fmt, va_list va)
{
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, va);
    log_string(level, system, buf);
}


void log_log(enum log_level level, enum log_system system, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(level, system, fmt, va);
    va_end(va);
}

void log_init(void)
{
    log_set_level(LOG_ALL, LOG_SYS_ALL, LOG_LEVEL_INFO);
}

void log_set_level(uint8_t dest, enum log_system system, enum log_level level)
{
    if(dest == LOG_ALL)
    {
        for(int i = 0; i < LOG_NUM_DESTS; i++)
        {
            log_set_level(i, system, level);
        }
    } else {
        if(system == LOG_SYS_ALL)
        {
            for(int j = 0; j < LOG_NUM_SYSTEMS; j++)
            {
                log_levels[dest][j] = level;
            }
        } else {
            log_levels[dest][system] = level;
        }
    }
}

enum log_level log_get_level(uint8_t dest, enum log_system system)
{
    return log_levels[dest][system];
}
