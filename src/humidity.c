#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "json-util.h"
#include "log.h"
#include "humidity.h"

#define LOG_SYS LOG_SYS_HUMIDITY

void humidity_free(struct measurement *measurement)
{
    for(;;) {
        if(!measurement) {
            break;
        }

        struct measurement *next = measurement->next;
        free(measurement->name);
        free(measurement);
        measurement = next;
    }
}

struct measurement * humidity_parse_json(json_stream *json)
{
    struct measurement *first = NULL;
    struct measurement *prev = NULL;

    int num_nodes = 0;

    json_expect(json, JSON_ARRAY);

    while(json_next(json) == JSON_OBJECT) {
        const char *names[] = {
            [0] = "nodeId",
            [1] = "name",
            [2] = "timestamp",
            [3] = "temperature",
            [4] = "humidity",
            [5] = "battery1_level",
            [6] = "battery2_level",
        };

        int node_id = -1;
        time_t timestamp = 0;
        char *name = NULL;
        float temperature = 0;
        float humidity = 0;
        float battery1_level = 0;
        float battery2_level = 0;

        int n;

        while((n = json_find_names(json, names, sizeof(names)/sizeof(names[0]))) >= 0) {
            switch(n) {
            case 0:
            {
                json_expect(json, JSON_NUMBER);
                node_id = json_get_long(json);
                break;
            }
            case 1:
            {
                enum json_type next = json_next(json);
                if(next == JSON_STRING) {
                    size_t len;
                    const char *s = json_get_string(json, &len);
                    name = malloc(len+1);
                    strncpy(name, s, len+1);
                }
                break;
            }
            case 2:
            {
                json_expect(json, JSON_NUMBER);
                timestamp = json_get_long(json);
                break;
            }
            case 3:
            {
                json_expect(json, JSON_NUMBER);
                temperature = json_get_number(json);
                break;
            }
            case 4:
            {
                json_expect(json, JSON_NUMBER);
                humidity = json_get_number(json);
                break;
            }
            case 5:
            {
                json_expect(json, JSON_NUMBER);
                battery1_level = json_get_number(json);
                break;
            }
            case 6:
            {
                json_expect(json, JSON_NUMBER);
                battery2_level = json_get_number(json);
                break;
            }
            }
        }

        if((node_id >= 0) && (timestamp > 0)) {
            struct measurement *new = malloc(sizeof(*new));
            if(new) {
                new->node_id = node_id;
                new->name = name;
                new->timestamp = timestamp;
                new->temperature = temperature;
                new->humidity = humidity;
                new->battery1_level = battery1_level;
                new->battery2_level = battery2_level;
                new->next = NULL;
            }

            if(prev) {
                prev->next = new;
            } else {
                first = new;
            }
            prev = new;

            num_nodes++;
        }
    }

    LOG("%d nodes found", num_nodes);

    return first;
}
