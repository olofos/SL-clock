#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "json.h"
#include "json-util.h"

#include "log.h"

#define LOG_SYS LOG_SYS_JSON

const char *json_type_to_string(enum json_type type)
{
    switch(type)
    {
    case JSON_ERROR: return "ERROR";
    case JSON_DONE: return "DONE";
    case JSON_OBJECT: return "OBJECT";
    case JSON_OBJECT_END: return "OBJECT_END";
    case JSON_ARRAY: return "ARRAY";
    case JSON_ARRAY_END: return "ARRAY_END";
    case JSON_STRING: return "STRING";
    case JSON_NUMBER: return "NUMBER";
    case JSON_TRUE: return "TRUE";
    case JSON_FALSE: return "FALSE";
    case JSON_NULL: return "NULL";
    default: return "UNKNOWN";
    }
}

bool json_expect(json_stream *json, enum json_type type)
{
    enum json_type next = json_next(json);
    if(next != type)
    {
        LOG("Expected %s got %s", json_type_to_string(type), json_type_to_string(next));
    }

    return next == type;
}

static void json_skip_until_zero(json_stream *json, int arrays, int objects)
{
    while((arrays > 0) || (objects > 0))
    {
        enum json_type type = json_next(json);

        switch(type)
        {
        case JSON_OBJECT:
            objects++;
            break;
        case JSON_ARRAY:
            arrays++;
            break;
        case JSON_OBJECT_END:
            objects--;
            break;
        case JSON_ARRAY_END:
            arrays--;
            break;
        case JSON_STRING:
        case JSON_NUMBER:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            break;

        case JSON_DONE:
        case JSON_ERROR:
        default:
            return;
        }
    }
}

void json_skip_until_end_of_object(json_stream *json)
{
    json_skip_until_zero(json, 0, 1);
}

void json_skip_until_end_of_array(json_stream *json)
{
    json_skip_until_zero(json, 1, 0);
}

void json_skip(json_stream *json)
{
    switch(json_next(json))
    {
    case JSON_OBJECT:
        json_skip_until_end_of_object(json);
        break;
    case JSON_ARRAY:
        json_skip_until_end_of_array(json);
        break;
    case JSON_OBJECT_END:
        LOG("Unexpected JSON_OBJECT_END");
        break;
    case JSON_ARRAY_END:
        LOG("Unexpected JSON_ARRAY_END");
        break;

    case JSON_DONE:
    case JSON_STRING:
    case JSON_NUMBER:
    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_NULL:
    case JSON_ERROR:
    default:
        break;
    }
}

int json_find_names(json_stream *json, const char * names[], int num)
{
    enum json_type type;
    while((type = json_next(json)) == JSON_STRING)
    {
        const char *name = json_get_string(json, 0);
        for(int i = 0; i < num; i++)
        {
            if(strcmp(name, names[i]) == 0)
            {
                return i;
            }
        }

        json_skip(json);
    }
    if(type != JSON_OBJECT_END)
    {
        LOG("Expected JSON_OBJECT_END");
    }
    return -1;
}


bool json_find_name(json_stream *json, const char *name)
{
    const char* names[] = {name};
    return json_find_names(json, names, 1) >= 0;
}


void json_open_http(json_stream *json, struct HTTPRequest *request)
{
    json_open_user(json, (json_user_io) &http_getc, (json_user_io) &http_peek, request);
}
