#include "json-writer.h"

static int file_write_string(FILE *f, const char *s)
{
    return fputs(s, f);
}

void json_writer_http_init(struct json_writer *json, struct http_request *request)
{
    json->user = request;
    json->current = 0;
    json->write_string = (json_writer_io) http_write_string;
}

void json_writer_file_init(struct json_writer *json, FILE *f)
{
    json->user = f;
    json->current = 0;
    json->write_string = (json_writer_io) file_write_string;
}

static inline int write_string(struct json_writer *json, const char *s)
{
    return json->write_string(json->user, s);
}

static void maybe_write_comma(struct json_writer *json)
{
    if(json->current > 0) {
        if(json->stack[json->current-1] & JSON_WRITER_COMMA) {
            write_string(json, ",");
        }

        json->stack[json->current-1] |= JSON_WRITER_COMMA;
    }
}

void json_writer_begin_object(struct json_writer *json, const char *name)
{
    maybe_write_comma(json);
    if(name) {
        write_string(json, "\"");
        write_string(json, name);
        write_string(json, "\":{");
    } else {
        write_string(json, "{");
    }

    json->stack[json->current++] = JSON_WRITER_OBJECT;
}

void json_writer_end_object(struct json_writer *json)
{
    write_string(json, "}");
    json->current--;
}

void json_writer_begin_array(struct json_writer *json, const char *name)
{
    maybe_write_comma(json);

    if(name) {
        write_string(json, "\"");
        write_string(json, name);
        write_string(json, "\":[");
    } else {
        write_string(json, "[");
    }
    json->stack[json->current++] = JSON_WRITER_ARRAY;
}

void json_writer_end_array(struct json_writer *json)
{
    write_string(json, "]");
    json->current--;
}

void json_writer_write_string(struct json_writer *json, const char *name, const char *str)
{
    maybe_write_comma(json);

    if(name) {
        write_string(json, "\"");
        write_string(json, name);

        if(str) {
            write_string(json, "\":\"");
            write_string(json, str);
            write_string(json, "\"");
        } else {
            write_string(json, "\":null");
        }
    } else {
        if(str) {
            write_string(json, "\"");
            write_string(json, str);
            write_string(json, "\"");
        } else {
            write_string(json, "null");
        }
    }
}


void json_writer_write_int(struct json_writer *json, const char *name, int num)
{
    char buf[16];
    sprintf(buf, "%d", num);

    maybe_write_comma(json);

    if(name) {
        write_string(json, "\"");
        write_string(json, name);
        write_string(json, "\":");
    }

    write_string(json, buf);
}

void json_writer_write_bool(struct json_writer *json, const char *name, int val)
{
    maybe_write_comma(json);

    if(name) {
        write_string(json, "\"");
        write_string(json, name);
        write_string(json, "\":");
    }
    if(val) {
        write_string(json, "true");
    } else {
        write_string(json, "false");
    }
}
