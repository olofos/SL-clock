#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "json.h"
#include "json-util.h"
#include "json-writer.h"

#include "journey.h"
#include "journey-task.h"
#include "wifi-task.h"
#include "timezone-db.h"

#include "log.h"
#define LOG_SYS LOG_SYS_CONFIG

static char* nul_strncpy(char *dest, const char *src, size_t n)
{
    strncpy(dest, src, n-1);
    dest[n-1] = '\0';
    return dest;
}

#define WIFI 0
#define JOURNIES 1
#define TIMEZONE 2
#define NUM_CONFIG 3

typedef void (*config_load_func)(json_stream *);
typedef void (*config_save_func)(struct json_writer *, const char *name);

static void config_load_wifi(json_stream *json);
void config_load_journies(json_stream *json);
static void config_load_timezone(json_stream *json);

static void config_save_wifi(struct json_writer *json, const char *name);
static void config_save_journies(struct json_writer *json, const char *name);
static void config_save_timezone(struct json_writer *json, const char *name);


static const char* config_names[] = {
    [WIFI] = "wifi",
    [JOURNIES] = "journies",
    [TIMEZONE] = "timezone",
};

static config_load_func config_load_funcs[] = {
    [WIFI] = config_load_wifi,
    [JOURNIES] = config_load_journies,
    [TIMEZONE] = config_load_timezone,
};

static config_save_func config_save_funcs[] = {
    [WIFI] = config_save_wifi,
    [JOURNIES] = config_save_journies,
    [TIMEZONE] = config_save_timezone,
};

static void config_load_wifi(json_stream *json)
{
    char ssid[WIFI_SSID_LEN];
    char pass[WIFI_PASS_LEN];

    json_expect(json, JSON_ARRAY);

    while(json_next(json) == JSON_OBJECT) {

        ssid[0] = 0;
        pass[0] = 0;

        int n;
        while((n = json_find_names(json, (const char*[]) { "ssid", "password" }, 2)) >= 0) {
            json_expect(json, JSON_STRING);
            switch(n) {
            case 0:
                nul_strncpy(ssid, json_get_string(json, 0), WIFI_SSID_LEN);
                printf("SSID: %s\n", json_get_string(json, 0));
                break;
            case 1:
                nul_strncpy(pass, json_get_string(json, 0), WIFI_PASS_LEN);
                break;
            }
        }

        wifi_ap_add_back(ssid, pass);
    }
}

void config_load_journies(json_stream *json)
{
    json_expect(json, JSON_ARRAY);

    int num = 0;
    while(json_next(json) == JSON_OBJECT)
    {
        printf("Journey #%d:", num);

        int n;

        struct journey journey;

        memset(&journey, 0, sizeof(journey));

        const char *names[] = {"line", "stop","destination","site-id","mode","direction","margin"};

        while((n = json_find_names(json, names, sizeof(names)/sizeof(names[0]))) >= 0)
        {
            switch(n)
            {
            case 0: // line
                json_expect(json, JSON_STRING);

                nul_strncpy(journey.line, json_get_string(json, 0), JOURNEY_LINE_LEN);
                printf("  line: %s\n", journey.line);
                break;

            case 1: // stop
                json_expect(json, JSON_STRING);
                nul_strncpy(journey.stop, json_get_string(json, 0), JOURNEY_STOP_LEN);
                printf("  stop: %s\n", journey.stop);
                break;

            case 2: // destination
                json_expect(json, JSON_STRING);
                nul_strncpy(journey.destination, json_get_string(json, 0), JOURNEY_STOP_LEN);
                printf("  destination: %s\n", journey.destination);
                break;

            case 3: // site_id
                json_expect(json, JSON_NUMBER);
                journey.site_id = json_get_long(json);
                printf("  site-id: %d\n", journey.site_id);
                break;

            case 4: // mode
                json_expect(json, JSON_NUMBER);
                journey.mode = json_get_long(json);
                printf("  mode: %d\n", journey.mode);
                break;

            case 5: // direction
                json_expect(json, JSON_NUMBER);
                journey.direction = json_get_long(json);
                printf("  direction: %d\n", journey.direction);
                break;

            case 6: // margin
                json_expect(json, JSON_NUMBER);
                journey.margin = json_get_long(json);
                printf("  margin: %d\n", journey.margin);
                break;
            }
        }

        journey_set_journey(num++, &journey);
    }

    while(num < JOURNEY_MAX_JOURNIES) {
        struct journey journey;
        journey.line[0] = 0;
        journey.stop[0] = 0;
        journey.destination[0] = 0;
        journey.departures[0] = 0;

        journey_set_journey(num++, &journey);
    }
}

static void config_load_timezone(json_stream *json)
{
    if(json_expect(json, JSON_STRING))
    {
        timezone_set_timezone(json_get_string(json, 0));
        printf("TZ: %s\n", timezone_get_timezone());
    }
}

static void config_save_wifi(struct json_writer *json, const char *name)
{
    json_writer_begin_array(json, name);

    wifi_take_mutex();

    for(struct wifi_ap *ap = wifi_first_ap; ap; ap = ap->next) {
        json_writer_begin_object(json, NULL);
        json_writer_write_string(json, "ssid", ap->ssid);
        json_writer_write_string(json, "password", ap->password);
        json_writer_end_object(json);
    }

    wifi_give_mutex();

    json_writer_end_array(json);
}


static void config_save_journies(struct json_writer *json, const char *name)
{
    json_writer_begin_array(json, name);

    for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
    {
        if(strlen(journies[i].line) > 0)
        {
            json_writer_begin_object(json, NULL);
            json_writer_write_string(json, "line", journies[i].line);
            json_writer_write_string(json, "stop", journies[i].stop);
            json_writer_write_string(json, "destination", journies[i].destination);
            json_writer_write_int(json, "site-id", journies[i].site_id);
            json_writer_write_int(json, "mode", journies[i].mode);
            json_writer_write_int(json, "direction", journies[i].direction);
            json_writer_write_int(json, "margin", journies[i].margin);
            json_writer_end_object(json);

        }
    }
    json_writer_end_array(json);
}

static void config_save_timezone(struct json_writer *json, const char *name)
{
    json_writer_write_string(json, name, timezone_get_timezone());
}


void config_load(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if(!f)
    {
        LOG("Could not open file for reading config");
        return;
    }

    json_stream json;

    json_open_stream(&json, f);

    json_expect(&json, JSON_OBJECT);


    int n;
    while((n = json_find_names(&json, config_names, NUM_CONFIG)) >= 0)
    {
        config_load_funcs[n](&json);
    }

    json_close(&json);

    fclose(f);
}

void config_save(const char *filename)
{
    FILE *f = fopen(filename, "w");

    if(!f)
    {
        LOG("Could not open file for writing config");
        return;
    }

    struct json_writer json;

    json_writer_file_init(&json, f);

    json_writer_begin_object(&json, NULL);

    for(int i = 0; i < NUM_CONFIG; i++)
    {
        config_save_funcs[i](&json, config_names[i]);
    }

    json_writer_end_object(&json);

    fclose(f);
}
