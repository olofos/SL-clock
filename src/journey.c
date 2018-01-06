#include <time.h>
#include <string.h>

#include "json.h"
#include "json-util.h"
#include "log.h"
#include "journey.h"

#define LOG_SYS LOG_SYS_JOURNEY

#define TRANSPORT_MODE     0
#define LINE_NUMBER     1
#define DESTINATION     2
#define JOURNEY_DIRECTION 3
#define STOP_AREA_NAME 4
#define EXPECTED_DATE_TIME 5

static char* nul_strncpy(char *dest, const char *src, size_t n)
{
    strncpy(dest, src, n-1);
    dest[n-1] = '\0';
    return dest;
}


enum journey_status journey_parse_json(json_stream *json, struct journey *jour)
{
    json_expect(json, JSON_OBJECT);

    json_find_name(json, "StatusCode");
    json_expect(json, JSON_NUMBER);
    int status = json_get_number(json);

    if(status == 0)
    {
        if(json_find_name(json, "ResponseData"))
        {
            json_expect(json,JSON_OBJECT);


            while(json_find_names(json, (const char *[]){"Metros", "Buses", "Trains", "Trams", "Ships"}, 5) >= 0)
            {
                json_expect(json, JSON_ARRAY);

                uint8_t num_departs = 0;

                while(json_next(json) == JSON_OBJECT)
                {
                    int n = 0;
                    const char *names[] = {
                        [TRANSPORT_MODE] = "TransportMode",
                        [LINE_NUMBER] = "LineNumber",
                        [DESTINATION] = "Destination",
                        [JOURNEY_DIRECTION] = "JourneyDirection",
                        [STOP_AREA_NAME] = "StopAreaName",
                        [EXPECTED_DATE_TIME] = "ExpectedDateTime"
                    };

                    char line[JOURNEY_LINE_LEN] = "";
                    uint8_t dir = 0;
                    enum journey_transport_mode mode = TRANSPORT_MODE_UNKNOWN;
                    time_t depart = 0;

                    while((n = json_find_names(json, names, sizeof(names)/sizeof(names[0]))) >= 0)
                    {
                        switch(n)
                        {
                        case TRANSPORT_MODE:
                        {
                            json_expect(json, JSON_STRING);
                            const char *s = json_get_string(json, 0);

                            if(strcmp(s, "BUS") == 0)
                            {
                                mode = TRANSPORT_MODE_BUS;
                            } else if(strcmp(s, "METRO") == 0) {
                                mode = TRANSPORT_MODE_METRO;
                            } else if(strcmp(s, "TRAIN") == 0) {
                                mode = TRANSPORT_MODE_TRAIN;
                            } else if(strcmp(s, "TRAM") == 0) {
                                mode = TRANSPORT_MODE_TRAM;
                            } else if(strcmp(s, "SHIP") == 0) {
                                mode = TRANSPORT_MODE_SHIP;
                            } else {
                                LOG("Unknown transport mode '%s'", s);
                            }
                            LOG("Mode: %s", s);
                            break;

                        }

                        case LINE_NUMBER:
                            json_expect(json, JSON_STRING);
                            nul_strncpy(line, json_get_string(json, 0), JOURNEY_LINE_LEN);
                            LOG("Number: %s", json_get_string(json, 0));
                            break;

                        case DESTINATION:
                            json_expect(json, JSON_STRING);
                            LOG("Destination: %s", json_get_string(json, 0));
                            break;

                        case JOURNEY_DIRECTION:
                        {
                            json_expect(json, JSON_NUMBER);
                            uint8_t n = json_get_long(json);
                            dir = n;
                            LOG("Direction: %d", n);
                        }
                            break;

                        case STOP_AREA_NAME:
                            json_expect(json, JSON_STRING);
                            LOG("Stop: %s", json_get_string(json, 0));
                            break;

                        case EXPECTED_DATE_TIME:
                        {
                            json_expect(json, JSON_STRING);
                            const char * s = json_get_string(json, 0);

                            struct tm ts;
                            ts.tm_isdst = 0;

                            if(!strptime(s, "%Y-%m-%dT%H:%M:%S", &ts))
                            {
                                LOG("Error parsing date time %s", s);
                            }
                            depart = mktime(&ts);
                            char buf[32];
                            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ts);
                            LOG("Raw time: '%s'", s);
                            LOG("Time: %s (%lu)", buf, (unsigned long) depart);
                            break;
                        }
                        }
                    }

                    if((mode == jour->mode) && (dir == jour->direction) && (strcmp(line, jour->line) == 0) && (depart > 0))
                    {
                        if(num_departs < JOURNEY_MAX_DEPARTURES)
                        {
                            jour->departures[num_departs++] = depart;
                            LOG("Match #%d", num_departs);
                        } else {
                            LOG("Too many matches");
                        }
                    }
                    printf("\n");
                }

                if(num_departs > 0)
                {
                    jour->next_departure = 0;
                }
            }
        }

        return JOURNEY_OK;
    } else {
        json_find_name(json, "Message");
        json_expect(json, JSON_STRING);

        LOG("Status: %d", status);
        LOG("Message: %s", json_get_string(json, 0));
    }

    return JOURNEY_ERROR;
}
