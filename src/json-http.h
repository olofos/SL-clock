#ifndef JSON_HTTP_H_
#define JSON_HTTP_H_

#include "http-sm/http.h"

void json_open_http(json_stream *json, struct http_request *request);

#endif
