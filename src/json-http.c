#include "json.h"
#include "json-http.h"
#include "http-sm/http.h"

void json_open_http(json_stream *json, struct http_request *request)
{
    json_open_user(json, (json_user_io) &http_getc, (json_user_io) &http_peek, request);
}
