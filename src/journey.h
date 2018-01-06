#ifndef JOURNEY_H_
#define JOURNEY_H_

#include <stdint.h>
#include <time.h>

#include "json.h"

#define JOURNEY_STOP_LEN 32
#define JOURNEY_LINE_LEN 6
#define JOURNEY_MAX_DEPARTURES 32
#define JOURNEY_MAX_JOURNIES 2


enum journey_transport_mode
{
    TRANSPORT_MODE_UNKNOWN = 0,
    TRANSPORT_MODE_BUS,
    TRANSPORT_MODE_METRO,
    TRANSPORT_MODE_TRAIN,
    TRANSPORT_MODE_TRAM,
    TRANSPORT_MODE_SHIP
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
    uint8_t next_departure;

    time_t next_update;
};

struct json_stream;
enum journey_status journey_parse_json(struct json_stream *json, struct journey *jour);

extern struct journey journies[JOURNEY_MAX_JOURNIES];

#endif
