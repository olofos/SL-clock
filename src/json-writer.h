#ifndef JSON_WRITER_H_
#define JSON_WRITER_H_

#include <stdio.h>
#include "http-sm/http.h"

#define JSON_WRITER_MAX_DEPTH 8

#define JSON_WRITER_OBJECT 0x01
#define JSON_WRITER_ARRAY  0x02
#define JSON_WRITER_COMMA  0x80

typedef int (*json_writer_io) (void *user, const char* string);

struct json_writer {
    uint8_t current;
    uint8_t stack[JSON_WRITER_MAX_DEPTH];
    void *user;
    json_writer_io write_string;
};

void json_writer_http_init(struct json_writer *json, struct http_request *request);
void json_writer_file_init(struct json_writer *json, FILE *f);

void json_writer_begin_object(struct json_writer *json, const char *name);
void json_writer_end_object(struct json_writer *json);

void json_writer_begin_array(struct json_writer *json, const char *name);
void json_writer_end_array(struct json_writer *json);

void json_writer_write_string(struct json_writer *json, const char *name, const char *str);
void json_writer_write_int(struct json_writer *json, const char *name, int num);
void json_writer_write_bool(struct json_writer *json, const char *name, int val);


#endif
