#include "json.h"
#include "http-client.h"

void json_open_http(json_stream *json, struct HTTPRequest *request)
{
    json_open_user(json, (json_user_io) &http_getc, (json_user_io) &http_peek, request);
}
