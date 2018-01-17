#ifndef LOG_H_
#define LOG_H_

#include <stdint.h>
#include <stdarg.h>

enum log_destination
{
    LOG_STDOUT = 0,
//    LOG_CBUF,

    LOG_NUM_DESTS,
    LOG_ALL
};

enum log_level
{
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_NONE
};

enum log_system
{
    LOG_SYS_RTOS = 0,
    LOG_SYS_SDK,
    LOG_SYS_MAIN,
    LOG_SYS_RTC,
    LOG_SYS_SNTP,
    LOG_SYS_HTTP,
    LOG_SYS_HTTPD,
    LOG_SYS_JOURNEY,
    LOG_SYS_TZDB,
    LOG_SYS_JSON,
    LOG_SYS_SSD1306,
    LOG_SYS_WIFI,
    LOG_SYS_CONFIG,

    LOG_NUM_SYSTEMS,

    LOG_SYS_ALL,
};


void log_log(enum log_level level, enum log_system system, const char *format, ...);
void log_init(void);
void log_set_level(uint8_t dests, enum log_level level, enum log_system system);

#define LOG(...) log_log(LOG_INFO, LOG_SYS, __VA_ARGS__)


#endif
