#ifndef JSON_HTTP_H_
#define JSON_HTTP_H_

#include "http-sm/http.h"
#include "json.h"

void json_open_http(json_stream *json, struct http_request *request);

#define HTTP_JSON_MAX_DEPTH 8

#define HTTP_JSON_OBJECT 0x01
#define HTTP_JSON_ARRAY  0x02
#define HTTP_JSON_COMMA  0x80

struct http_json_writer {
    struct http_request *request;
    uint8_t current;
    uint8_t stack[HTTP_JSON_MAX_DEPTH];
};

void http_json_init(struct http_json_writer *json, struct http_request *request);

void http_json_begin_object(struct http_json_writer *json, const char *name);
void http_json_end_object(struct http_json_writer *json);

void http_json_begin_array(struct http_json_writer *json, const char *name);
void http_json_end_array(struct http_json_writer *json);

void http_json_write_string(struct http_json_writer *json, const char *name, const char *str);
void http_json_write_int(struct http_json_writer *json, const char *name, int num);
void http_json_write_bool(struct http_json_writer *json, const char *name, int val);



#endif
