#ifndef STATUS_H_
#define STATUS_H_

#include <stdint.h>

struct app_status
{
    uint8_t wifi_connected;
    uint8_t obtained_time;
    uint8_t obtained_tz;
};


extern struct app_status app_status;


#endif
