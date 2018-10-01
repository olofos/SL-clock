#ifndef LOG_H_
#define LOG_H_

#include <stdint.h>
#include <stdarg.h>
#include <time.h>

enum log_destination
{
    LOG_STDOUT = 0,
    LOG_CBUF = 1,

    LOG_NUM_DESTS,
    LOG_ALL
};

enum log_level
{
    LOG_LEVEL_EMERGENCY = 0,
    LOG_LEVEL_ALERT,
    LOG_LEVEL_CRITICAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_NOTICE,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,

    LOG_NUM_LEVELS,
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
    LOG_SYS_SH1106,
    LOG_SYS_WIFI,
    LOG_SYS_CONFIG,
    LOG_SYS_DISPLAY,
    LOG_SYS_LOG,

    LOG_NUM_SYSTEMS,

    LOG_SYS_ALL,
};


void log_log(enum log_level level, enum log_system system, const char *format, ...);
void log_init(void);
void log_set_level(uint8_t dests, enum log_system system, enum log_level level);
enum log_level log_get_level(uint8_t dests, enum log_system system);

#define DEBUG(...) log_log(LOG_LEVEL_DEBUG, LOG_SYS, __VA_ARGS__)
#define INFO(...) log_log(LOG_LEVEL_INFO, LOG_SYS, __VA_ARGS__)
#define LOG(...) log_log(LOG_LEVEL_NOTICE, LOG_SYS, __VA_ARGS__)
#define WARNING(...) log_log(LOG_LEVEL_WARNING, LOG_SYS, __VA_ARGS__)
#define ERROR(...) log_log(LOG_LEVEL_ERROR, LOG_SYS, __VA_ARGS__)

#define LOG_ERROR ERROR
#define LOG_WARNING WARN

#define LOG_CBUF_LEN 128
#define LOG_CBUF_STRLEN 80

struct log_cbuf_message {
    time_t timestamp;
    enum log_level level;
    enum log_system system;
    char message[LOG_CBUF_STRLEN];
    char zero;
};

struct log_cbuf {
    uint8_t head;
    uint8_t tail;
    struct log_cbuf_message message[LOG_CBUF_LEN];
};

extern struct log_cbuf log_cbuf;
extern const char* log_system_names[LOG_NUM_SYSTEMS];
extern const char* log_level_names[LOG_NUM_LEVELS];

#endif
