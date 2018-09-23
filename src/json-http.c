#include "json.h"
#include "json-http.h"
#include "http-sm/http.h"

void json_open_http(json_stream *json, struct http_request *request)
{
    json_open_user(json, (json_user_io) &http_getc, (json_user_io) &http_peek, request);
}

void http_json_init(struct http_json_writer *json, struct http_request *request)
{
    json->request = request;
    json->current = 0;
}


static void maybe_write_comma(struct http_json_writer *json)
{
    if(json->current > 0) {
        if(json->stack[json->current-1] & HTTP_JSON_COMMA) {
            http_write_string(json->request, ",");
        }

        json->stack[json->current-1] |= HTTP_JSON_COMMA;
    }
}

void http_json_begin_object(struct http_json_writer *json, const char *name)
{
    maybe_write_comma(json);
    if(name) {
        http_write_string(json->request, "\"");
        http_write_string(json->request, name);
        http_write_string(json->request, "\":{");
    } else {
        http_write_string(json->request, "{");
    }

    json->stack[json->current++] = HTTP_JSON_OBJECT;
}

void http_json_end_object(struct http_json_writer *json)
{
    http_write_string(json->request, "}");
    json->current--;
}

void http_json_begin_array(struct http_json_writer *json, const char *name)
{
    if(name) {
        maybe_write_comma(json);

        http_write_string(json->request, "\"");
        http_write_string(json->request, name);
        http_write_string(json->request, "\":[");
    } else {
        http_write_string(json->request, "[");
    }
    json->stack[json->current++] = HTTP_JSON_ARRAY;
}

void http_json_end_array(struct http_json_writer *json)
{
    http_write_string(json->request, "]");
    json->current--;
}

void http_json_write_string(struct http_json_writer *json, const char *name, const char *str)
{
    maybe_write_comma(json);

    http_write_string(json->request, "\"");
    http_write_string(json->request, name);

    if(str) {
        http_write_string(json->request, "\":\"");
        http_write_string(json->request, str);
        http_write_string(json->request, "\"");
    } else {
        http_write_string(json->request, "\":null");
    }
}


void http_json_write_int(struct http_json_writer *json, const char *name, int num)
{
    char buf[16];
    sprintf(buf, "%d", num);

    maybe_write_comma(json);

    http_write_string(json->request, "\"");
    http_write_string(json->request, name);
    http_write_string(json->request, "\":");
    http_write_string(json->request, buf);
}

void http_json_write_bool(struct http_json_writer *json, const char *name, int val)
{
    maybe_write_comma(json);

    http_write_string(json->request, "\"");
    http_write_string(json->request, name);
    if(val) {
        http_write_string(json->request, "\":true");
    } else {
        http_write_string(json->request, "\":false");
    }
}
