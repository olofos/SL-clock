#include <stdio.h>
#include <stdarg.h>

#include "log.h"

static enum log_level log_levels[LOG_NUM_DESTS][LOG_NUM_SYSTEMS];

static const char* log_level_names[] = {
    [LOG_TRACE] = "TRACE",
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO] = "INFO",
    [LOG_WARN] = "WARN",
    [LOG_ERROR] = "ERROR",
    [LOG_FATAL] = "FATAL",
    [LOG_NONE] = "",
};

static const char* log_system_names[LOG_NUM_SYSTEMS] = {
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
    [LOG_SYS_SSD1306] = "SSD1306",
    [LOG_SYS_WIFI] = "WIFI",
    [LOG_SYS_CONFIG] = "CONF",
};

typedef void (*log_func_t)(enum log_level, enum log_system, const char *);

void log_stdout(enum log_level level, enum log_system system, const char *msg);

log_func_t log_funcs[] = {
    [LOG_STDOUT] = log_stdout,
};


void log_stdout(enum log_level level, enum log_system system, const char *msg)
{
    printf("[%s] (%s): %s\n", log_level_names[level], log_system_names[system], msg);
}

void log_string(enum log_level level, enum log_system system, const char *msg)
{
    for(int i = 0; i < LOG_NUM_DESTS; i++)
    {
        if(log_levels[i][system] <= level)
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
    log_set_level(LOG_ALL, LOG_INFO, LOG_SYS_ALL);
}

void log_set_level(uint8_t dest, enum log_level level, enum log_system system)
{
    if(dest == LOG_ALL)
    {
        for(int i = 0; i < LOG_NUM_DESTS; i++)
        {
            log_set_level(i, level, system);
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


void log_rtos_fatal(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(LOG_FATAL, LOG_SYS_RTOS, fmt, va);
    va_end(va);
}

void log_rtos_warn(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(LOG_WARN, LOG_SYS_RTOS, fmt, va);
    va_end(va);
}

void log_rtos_info(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(LOG_INFO, LOG_SYS_RTOS, fmt, va);
    va_end(va);
}

void log_sdk_info(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(LOG_INFO, LOG_SYS_SDK, fmt, va);
    va_end(va);
}

void log_sntp_warn(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(LOG_WARN, LOG_SYS_SNTP, fmt, va);
    va_end(va);
}

void log_sntp_info(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    log_vlog(LOG_INFO, LOG_SYS_SNTP, fmt, va);
    va_end(va);
}
