#include <esp_common.h>
#include <string.h>

#include "json.h"
#include "json-util.h"
#include "json-http.h"
#include "journey.h"
#include "journey-task.h"
#include "keys.h"
#include "status.h"
#include "http-sm/http.h"
#include "log.h"

#define LOG_SYS LOG_SYS_JOURNEY

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)


struct journey journies[JOURNEY_MAX_JOURNIES];

void journey_set_journey(uint8_t num, const struct journey *jour)
{
    if(num < JOURNEY_MAX_JOURNIES)
    {
        memcpy(&journies[num], jour, sizeof(*jour));

        journies[num].next_update = time(0);
    } else {
        LOG("Trying to set journey #%d!", num);
    }
}


static void construct_http_request(const struct journey *jour, struct http_request *request, char buf[], size_t buf_len)
{
    const char str_true[] = "true";
    const char str_false[] = "false";

    snprintf(buf, buf_len, "/api2/realtimedeparturesV4.json?key=%s&TimeWindow=60&SiteId=%d&bus=%s&metro=%s&train=%s&tram=%s&ship=%s",
             KEY_SL_REALTIME,
             jour->site_id,
             (jour->mode == TRANSPORT_MODE_BUS) ? str_true : str_false,
             (jour->mode == TRANSPORT_MODE_METRO) ? str_true : str_false,
             (jour->mode == TRANSPORT_MODE_TRAIN) ? str_true : str_false,
             (jour->mode == TRANSPORT_MODE_TRAM) ? str_true : str_false,
             (jour->mode == TRANSPORT_MODE_SHIP) ? str_true : str_false
        );

    LOG("path = %s", buf);

    request->host = "api.sl.se";
    request->path = buf;
    request->port = 80;
}

static int update_journey(struct journey *journey)
{
    struct http_request request;
    http_request_init(&request);

    char buf[256];

    construct_http_request(journey, &request, buf, sizeof(buf));

    if(http_get_request(&request) < 0) {
        LOG("http_get_request failed");
        return -1;
    }

    json_stream json;
    json_open_http(&json, &request);

    journey_parse_json(&json, journey);

    if (json_get_error(&json)) {
        LOG("JSON error %s", json_get_error(&json));
    }

    json_close(&json);
    http_close(&request);

    return 0;
}

void journey_task(void *pvParameters)
{
    LOG("Journey task starting");

    while(!(app_status.wifi_connected && app_status.obtained_time && app_status.obtained_tz))
    {
        vTaskDelayMs(100);
    }


    for(;;)
    {
        vTaskDelayMs(1000);

        for(int j = 0; j < JOURNEY_MAX_JOURNIES; j++)
        {
            struct journey *journey = &journies[j];
            time_t now = time(0);

            if(now < 946684800)
            {
                LOG("'now' is in the past, is SNTP started?");
                continue;
            }

            if(!journey->line[0])
            {
                continue;
            }

            if(journey->mode == TRANSPORT_MODE_UNKNOWN)
            {
                LOG("Journey %d: transport mode unknown", j);
                continue;
            }

            if(journey->next_update - now > 0)
            {
                while((journey->departures[0] > 0) && (journey->departures[0] - now <= 0)) {
                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&journey->departures[0]));
                    LOG("%d: Departured at %s", j, buf);

                    for(int i = 0; i < JOURNEY_MAX_DEPARTURES - 1; i++) {
                        journey->departures[i] = journey->departures[i+1];
                    }
                }
            } else {
                LOG("Updating journey %d", j);

                int ret = update_journey(journey);


                if(ret < 0)
                {
                    journey->next_update = now + 15;
                } else {
                    journey->next_update = now + 30 * 60;

                    LOG("Journey from with line %s from %s to %s:", journey->line, journey->stop, journey->destination);
                    for(int i = 0; i < JOURNEY_MAX_DEPARTURES && journey->departures[i]; i++)
                    {
                        char buf[32];
                        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&journey->departures[i]));

                        LOG("Depature %d at %s", i, buf);
                    }

                    printf("\n");

                    LOG("xPortGetFreeHeapSize: %d", xPortGetFreeHeapSize());
                    LOG("uxTaskGetStackHighWaterMark: %u", (unsigned)uxTaskGetStackHighWaterMark(0));
                }

                char buf[32];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&journey->next_update));

                LOG("Next update at %s", buf);
                printf("\n");
            }
        }
    }
}
