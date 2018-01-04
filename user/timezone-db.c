#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esp_common.h>
#include <apps/sntp_time.h>

#include "json.h"
#include "json-util.h"
#include "keys.h"
#include "timezone-db.h"
#include "log.h"

#define LOG_SYS LOG_SYS_TZDB

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)


#define COUNTRY_NAME 0
#define ZONE_NAME 1
#define ABBREVIATION 2
#define GMT_OFFSET 3
#define DST 4
#define DST_END 5

static time_t timezone_next_update;

static char* nul_strncpy(char *dest, const char *src, size_t n)
{
    strncpy(dest, src, n-1);
    dest[n-1] = '\0';
    return dest;
}

static void construct_http_request(struct HTTPRequest *request, char buf[], size_t buf_len)
{
    snprintf(buf, buf_len, "/v2/get-time-zone?format=json&key=%s&by=zone&zone=%s&fields=countryName,zoneName,abbreviation,gmtOffset,dst,dstStart,dstEnd", KEY_TIMEZONEDB, TIMEZONE);

    request->host = "api.timezonedb.com";
    request->path = buf;
    request->port = 80;
}

static int set_timezone(const char *abbrev, int offset, time_t end)
{
    time_t now = time(0);

    sntp_set_timezone(offset);

    if((0 < end - now) && (end - now < 24 * 60 * 60))
    {
        timezone_next_update = end + 5;
    } else {
        timezone_next_update = now + 24 * 60 * 60;
    }

    return 0;
}

static int timezone_db_parse_json(json_stream *json)
{
    int ret = -1;
    json_expect(json, JSON_OBJECT);

    if(json_find_name(json, "status"))
    {
        json_expect(json, JSON_STRING);
        if(strcmp(json_get_string(json, 0), "OK") == 0)
        {
            const char *names[] = {
                [COUNTRY_NAME] = "countryName",
                [ZONE_NAME] = "zoneName",
                [ABBREVIATION] = "abbreviation",
                [GMT_OFFSET] = "gmtOffset",
                [DST] = "dst",
                [DST_END] = "dstEnd"
            };

            int n;

            char abbrev[6];
            int offset = 0;
            time_t end = 0;
            while((n = json_find_names(json, names, sizeof(names)/sizeof(names[0]))) >= 0)
            {
                switch(n)
                {
                case COUNTRY_NAME:
                    json_expect(json, JSON_STRING);
                    LOG("Country: %s", json_get_string(json,0));
                    break;

                case ZONE_NAME:
                    json_expect(json, JSON_STRING);
                    LOG("Zone name: %s", json_get_string(json,0));
                    break;

                case ABBREVIATION:
                    json_expect(json, JSON_STRING);
                    nul_strncpy(abbrev, json_get_string(json,0), sizeof(abbrev));
                    break;

                case GMT_OFFSET:
                    json_expect(json, JSON_NUMBER);
                    offset = json_get_long(json);
                    LOG("Offset: %d", offset);
                    break;

                case DST:
                    json_expect(json, JSON_STRING);
                    LOG("DST: %s", (json_get_string(json,0)[0] == '0') ? "no" : "yes");
                    break;

                case DST_END:
                {
                    json_expect(json, JSON_NUMBER);
                    end = json_get_long(json);

                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&end));
                    LOG("Timezone change at %s", buf);
                    break;
                }
                }
            }


            ret = set_timezone(abbrev, offset, end);
        } else {
            if(json_find_name(json, "message"))
            {
                json_expect(json, JSON_STRING);
                LOG("Error: %s", json_get_string(json, 0));
            }
        }
    }
    return ret;
}

static int update_timezone(void)
{
    struct HTTPRequest request;
//    char buf[256];
    const int buf_size = 256;
    char *buf = malloc(buf_size);

    construct_http_request(&request, buf, buf_size);

    if(http_open(&request) < 0)
    {
        LOG("http_open failed");
        return -1;
    }

    if(http_get(&request) < 0) {
        LOG("http_get failed");
        return -1;
    }

    json_stream *json = malloc(sizeof(json_stream));
    json_open_http(json, &request);
    int ret = timezone_db_parse_json(json);

    if (json_get_error(json)) {
        LOG("JSON error %s", json_get_error(json));
    }

    json_close(json);
    free(json);
    http_close(&request);

    free(buf);

    return ret;
}

void timezone_db_task(void *pvParameters)
{
    LOG("Starting TimeZoneDB task");
    LOG("%d", sizeof(struct HTTPRequest));
    for(;;)
    {
        time_t now = time(0);

        if(now < 946684800)
        {
            LOG("'now' is in the past, is SNTP started?");
        } else if(now - timezone_next_update > 0)
        {
            LOG("Updating timezone");
            if(update_timezone() < 0)
            {
                timezone_next_update = now + 60;
            }
        }

        vTaskDelayMs(5000);
    }
}
