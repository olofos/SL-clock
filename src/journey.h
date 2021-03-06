#ifndef JOURNEY_H_
#define JOURNEY_H_

#include <stdint.h>
#include <time.h>

#include "json.h"

#define JOURNEY_STOP_LEN 32
#define JOURNEY_LINE_LEN 6
#define JOURNEY_MAX_DEPARTURES 32
#define JOURNEY_MAX_JOURNIES 2

#define JOURNEY_UPDATE_INTERVAL (20 * 60)
#define JOURNEY_ERROR_INTERVAL (1 * 60)


enum journey_transport_mode
{
    TRANSPORT_MODE_UNKNOWN = 0,
    TRANSPORT_MODE_BUS = 1,
    TRANSPORT_MODE_METRO = 2,
    TRANSPORT_MODE_TRAIN = 3,
    TRANSPORT_MODE_TRAM = 4,
    TRANSPORT_MODE_SHIP = 5
};

enum journey_status
{
    JOURNEY_ERROR = 0,
    JOURNEY_OK = 1
};

struct journey {
    char line[JOURNEY_LINE_LEN];
    char stop[JOURNEY_STOP_LEN];
    char destination[JOURNEY_STOP_LEN];
    uint32_t site_id;
    enum journey_transport_mode mode;
    uint8_t direction;

    time_t departures[JOURNEY_MAX_DEPARTURES];

    time_t next_update;
    time_t timeout;

    uint16_t margin;
};

struct json_stream;
enum journey_status journey_parse_json(struct json_stream *json, struct journey *jour);

extern struct journey journies[JOURNEY_MAX_JOURNIES];

#endif
