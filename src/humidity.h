#ifndef HUMIDITY_H_
#define HUMIDITY_H_

#define HUMIDITY_NAME_LEN 32

typedef struct json_stream json_stream;

struct measurement {
    unsigned node_id;
    char *name;
    time_t timestamp;
    float temperature;
    float humidity;
    float battery1_level;
    float battery2_level;
    struct measurement *next;
};

extern struct measurement *humidity_first_measurement;

struct measurement *humidity_parse_json(json_stream *json);
void humidity_free(struct measurement *measurement);

void humidity_take_mutex(void);
int humidity_take_mutex_noblock(void);
void humidity_give_mutex(void);


#endif
