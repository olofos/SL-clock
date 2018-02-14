#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "json.h"
#include "json-util.h"

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

typedef void (*load_func)(json_stream *);
typedef void (*save_func)(FILE *);

static void load_wifi(json_stream *json);
static void load_journies(json_stream *json);
static void load_timezone(json_stream *json);

static void save_wifi(FILE *f);
static void save_journies(FILE *f);
static void save_timezone(FILE *f);


static const char* config_names[] = {
    [WIFI] = "wifi",
    [JOURNIES] = "journies",
    [TIMEZONE] = "timezone",
};

static load_func load_funcs[] = {
    [WIFI] = load_wifi,
    [JOURNIES] = load_journies,
    [TIMEZONE] = load_timezone,
};

static save_func save_funcs[] = {
    [WIFI] = save_wifi,
    [JOURNIES] = save_journies,
    [TIMEZONE] = save_timezone,
};

static void load_wifi(json_stream *json)
{
    char ssid[WIFI_SSID_LEN];
    char pass[WIFI_PASS_LEN];

    json_expect(json, JSON_ARRAY);

    while(json_next(json) == JSON_OBJECT) {

        ssid[0] = 0;
        pass[0] = 0;

        int n;
        while((n = json_find_names(json, (const char*[]) { "SSID", "PASS" }, 2)) >= 0) {
            json_expect(json, JSON_STRING);
            switch(n) {
            case 0:
                nul_strncpy(ssid, json_get_string(json, 0), WIFI_SSID_LEN);
                printf("SSID: %s\n", json_get_string(json, 0));
                break;
            case 1:
                nul_strncpy(pass, json_get_string(json, 0), WIFI_PASS_LEN);
                printf("PASS: %s\n", json_get_string(json, 0));
                break;
            }
        }

        wifi_ap_add_back(ssid, pass);
    }
}

static void load_journies(json_stream *json)
{
    json_expect(json, JSON_ARRAY);

    int num = 0;
    while(json_next(json) == JSON_OBJECT)
    {
        printf("Journey #%d:", num);

        int n;

        struct journey journey;

        memset(&journey, 0, sizeof(journey));

        while((n = json_find_names(json, (const char*[]) {"line", "stop","destination","site_id","mode","direction"}, 6)) >= 0)
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
                printf("  site_id: %d\n", journey.site_id);
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
            }
        }

        journey_set_journey(num++, &journey);
    }
}

static void load_timezone(json_stream *json)
{
    if(json_expect(json, JSON_STRING))
    {
        timezone_set_timezone(json_get_string(json, 0));
        printf("TZ: %s\n", timezone_get_timezone());
    }
}

static void save_wifi(FILE *f)
{
    struct wifi_ap *ap = wifi_first_ap;

    fprintf(f, "[");

    while(ap) {
        fprintf(f, "{\"SSID\":\"%s\",\"PASS\":\"%s\"}", ap->ssid, ap->password);
        if(ap->next) {
            fprintf(f, ", ");
        }
        ap = ap->next;
    }

    fprintf(f, "]");
}


static void save_journies(FILE *f)
{
    fprintf(f, "[");

    for(int i = 0; i < JOURNEY_MAX_JOURNIES; i++)
    {
        if(strlen(journies[i].line) > 0)
        {
            if(i > 0)
            {
                fprintf(f,",");
            }
            fprintf(f, "\n        {");
            fprintf(f, "\"line\":\"%s\",", journies[i].line);
            fprintf(f, "\"stop\":\"%s\",", journies[i].stop);
            fprintf(f, "\"destination\":\"%s\",", journies[i].destination);
            fprintf(f, "\"site_id\":%d,", journies[i].site_id);
            fprintf(f, "\"mode\":%d,", journies[i].mode);
            fprintf(f, "\"direction\":%d", journies[i].direction);
            fprintf(f, "}");
        }
    }
    fprintf(f, "\n    ]");
}

static void save_timezone(FILE *f)
{
    fprintf(f, "\"%s\"", "Europe/Stockholm");
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
        load_funcs[n](&json);
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

    fprintf(f, "{\n");

    for(int i = 0; i < NUM_CONFIG; i++)
    {
        if(i > 0)
        {
            fprintf(f, ",\n");
        }
        fprintf(f, "    \"%s\":", config_names[i]);

        save_funcs[i](f);
    }


    fprintf(f, "\n}\n");
    fclose(f);
}
