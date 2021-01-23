#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <esp_common.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "timezone-db.h"
#include "journey.h"
#include "keys.h"
#include "wifi-task.h"
#include "json.h"
#include "json-http.h"
#include "json-util.h"
#include "json-writer.h"
#include "log.h"
#include "http-sm/http.h"
#include "http-sm/websocket.h"
#include "http-server-task.h"
#include "http-server-url-handlers.h"


#define LOG_SYS LOG_SYS_HTTPD

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

void http_server_write_simple_response(struct http_request* request, int status, const char* content_type, const char* reply)
{
    http_begin_response(request, status, content_type);
    http_write_header(request, "Cache-Control", "no-cache");
    if(reply) {
        http_set_content_length(request, strlen(reply));
    } else {
        http_set_content_length(request, 0);
    }
    http_end_header(request);
    if(reply) {
        http_write_string(request, reply);
    }
    http_end_body(request);
}

enum http_cgi_state cgi_not_found(struct http_request* request)
{
    http_server_write_simple_response(request, 404, "text/plain", "Not found\r\n");

    return HTTP_CGI_DONE;
}

struct cgi_forward_data {
    char *host;
    char *path;
    uint16_t port;

    char *query;
};

enum http_cgi_state cgi_forward(struct http_request* request)
{
    struct http_request* req = request->cgi_data;

    if(!req) {
        if(request->method != HTTP_METHOD_GET) {
            return HTTP_CGI_NOT_FOUND;
        }

        const struct cgi_forward_data *data = request->cgi_arg;

        const char *query_data_raw = http_get_query_arg(request, data->query);

        if(!query_data_raw) {
            WARNING("No query provided");

            http_server_write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"No query given\"}");

            return HTTP_CGI_DONE;
        }

        int query_data_len = http_urlencode(0, query_data_raw, 0);
        char *query_data = malloc(query_data_len + 1);

        if(!query_data) {
            ERROR("cgi_forward: malloc failed while allocating query_data");

            http_server_write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"Out of memory while allocating query\"}");

            return HTTP_CGI_DONE;
        }

        http_urlencode(query_data, query_data_raw, query_data_len + 1);

        int path_len = strlen(data->path) + 1 + strlen(data->query) + 1 + strlen(query_data) + 1;
        char *path = malloc(path_len);

        if(!path) {
            ERROR("cgi_forward: malloc failed while allocating path");

            http_server_write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"Out of memory while allocating path\"}");

            free(query_data);
            return HTTP_CGI_DONE;
        }

        req = malloc(sizeof(*req));

        if(!req) {
            ERROR("cgi_forward: malloc failed while allocating req");

            http_server_write_simple_response(request, 400, "application/json", "{\"StatusCode\":-1,\"Message\":\"Out of memory while allocating request\"}");

            free(query_data);
            free(path);
            return HTTP_CGI_DONE;
        }

        http_request_init(req);

        strcpy(path, data->path);
        strcat(path, "&");
        strcat(path, data->query);
        strcat(path, "=");
        strcat(path, query_data);

        free(query_data);

        req->host = "api.sl.se";
        req->path = path;
        req->port = 80;

        if(http_get_request(req) < 0) {
            ERROR("http_get_request failed");

            http_server_write_simple_response(request, 500, "application/json", "{\"StatusCode\":-1,\"Message\":\"Request failed\"}");

            free(path);
            free(req);
            return HTTP_CGI_DONE;
        }

        http_begin_response(request, req->status, req->content_type);
        http_write_header(request, "Cache-Control", "no-cache");
        http_set_content_length(request, req->read_content_length);
        http_end_header(request);


        request->cgi_data = req;
        return HTTP_CGI_MORE;
    } else {

        char buf[64];
        int n = http_read(req, buf, sizeof(buf));

        if(n > 0) {
            http_write_bytes(request, buf, n);
            return HTTP_CGI_MORE;
        } else {
            http_end_body(request);
            http_close(req);
            free(req->path);
            free(req);
            return HTTP_CGI_DONE;
        }
    }
}



const char *simple_response = "This is a response from \'cgi_simple\'";
const char *stream_response = "This is a response from \'cgi_stream\'";

enum http_cgi_state cgi_simple(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    const char *response = simple_response;

    http_begin_response(request, 200, "text/plain");
    http_set_content_length(request, strlen(response));
    http_end_header(request);

    http_write_string(request, response);

    http_end_body(request);

    return HTTP_CGI_DONE;
}

enum http_cgi_state cgi_stream(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    if(!request->cgi_data) {
        http_begin_response(request, 200, "text/plain");
        http_end_header(request);

        request->cgi_data = malloc(1);

        return HTTP_CGI_MORE;
    } else {
        const char *response = stream_response;

        http_write_string(request, response);
        http_end_body(request);

        free(request->cgi_data);

        return HTTP_CGI_DONE;
    }
}

enum http_cgi_state cgi_query(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    http_begin_response(request, 200, "text/plain");
    http_end_header(request);

    http_write_string(request, "This is a response from \'cgi_query\'\r\n");
    http_write_string(request, "The parameters were:\r\n");

    const char *sa = http_get_query_arg(request, "a");
    const char *sb = http_get_query_arg(request, "b");

    if(sa) {
        http_write_string(request, "a = ");
        http_write_string(request, sa);
        http_write_string(request, "\r\n");
    }

    if(sb) {
        http_write_string(request, "b = ");
        http_write_string(request, sb);
        http_write_string(request, "\r\n");
    }

    http_end_body(request);

    return HTTP_CGI_DONE;
}

enum http_cgi_state cgi_post(struct http_request* request)
{
    if(request->method != HTTP_METHOD_POST) {
        return HTTP_CGI_NOT_FOUND;
    }

    char data[32];
    int len = 0;

    while(request->state == HTTP_STATE_SERVER_READ_BODY) {
        int c = http_getc(request);
        if(c <= 0) {
            break;
        }
        if(len < sizeof(data) - 1) {
            data[len++] = c;
        }
    }

    data[len] = 0;
    LOG("Post data: \"%s\"", data);

    http_begin_response(request, 200, "text/plain");
    http_end_header(request);

    http_write_string(request, "This is a response from \'cgi_post\'\r\nYou posted: \"");
    http_write_string(request, data);
    http_write_string(request, "\"\r\n");

    http_end_body(request);

    return HTTP_CGI_DONE;
}

int ws_echo_open(struct websocket_connection* conn, struct http_request* request)
{
    return 1;
}

void ws_echo_message(struct websocket_connection* conn)
{
    char *str = malloc(conn->frame_length+1);
    int n = websocket_read(conn, str, conn->frame_length);
    LOG("n=%d", n);
    LOG("len=%d", (int)conn->frame_length);
    str[n] = 0;
    if(n < conn->frame_length) {
        LOG("Expected %d bytes but received %d", (int)conn->frame_length, n);
    }
    websocket_send(conn, str, conn->frame_length, conn->frame_opcode);

    if((conn->frame_opcode & WEBSOCKET_FRAME_OPCODE) == WEBSOCKET_FRAME_OPCODE_TEXT) {
        LOG("Message: %s", str);
    } else {
        LOG("Binary message of length %d", conn->frame_length);
    }
    free(str);
}

struct websocket_connection* ws_in_conn = 0;
struct websocket_connection* ws_out_conn = 0;

int ws_in_open(struct websocket_connection* conn, struct http_request* request)
{
    if(ws_in_conn == 0) {
        LOG("WS: new in connection %d", request->fd);
        ws_in_conn = conn;
        return 1;
    }
    return 0;
}

void ws_in_close(struct websocket_connection* conn)
{
    ws_in_conn = 0;
}

int ws_out_open(struct websocket_connection* conn, struct http_request* request)
{
    if(ws_out_conn == 0) {
        LOG("WS: new out connection %d", request->fd);
        ws_out_conn = conn;
        return 1;
    }
    return 0;
}

void ws_out_close(struct websocket_connection* conn)
{
    ws_out_conn = 0;
}

void ws_in_message(struct websocket_connection* conn)
{
    uint8_t *str = malloc(conn->frame_length+1);
    int n = websocket_read(conn, str, conn->frame_length);

    if(n > 0) {
        str[n] = 0;

        if(ws_out_conn) {
            websocket_send(ws_out_conn, str, n, conn->frame_opcode);
        } else {
            char *msg = "There is no out connection";
            websocket_send(conn, msg, strlen(msg), WEBSOCKET_FRAME_OPCODE_TEXT | WEBSOCKET_FRAME_FIN);
        }
    }

    free(str);
}

struct websocket_connection* ws_display_conn = 0;

int ws_display_open(struct websocket_connection* conn, struct http_request* request)
{
    if(ws_display_conn == 0) {
        LOG("WS: new in connection %d", request->fd);
        ws_display_conn = conn;
        return 1;
    }
    return 0;
}

void ws_display_close(struct websocket_connection* conn)
{
    ws_display_conn = 0;
}



struct cgi_forward_data journies_forward_data = {
    .host = "api.sl.se",
    .path = "/api2/realtimedeparturesV4.json?key=" KEY_SL_REALTIME "&TimeWindow=60",
    .port = 80,
    .query = "siteId",
};

struct cgi_forward_data places_forward_data = {
    .host = "api.sl.se",
    .path = "/api2/typeahead.json?key=" KEY_SL_PLACES,
    .port = 80,
    .query = "SearchString",
};


struct http_url_handler http_url_tab[] = {
    {"/simple", cgi_simple, NULL},
    {"/stream", cgi_stream, NULL},
    {"/query", cgi_query, NULL},
    {"/post", cgi_post, NULL},
    {"/wildcard/*", cgi_simple, NULL},

    {"/api/wifi-scan.json", cgi_wifi_scan, NULL},
    {"/api/wifi-list.json", cgi_wifi_list, NULL},
    {"/api/journies-config.json", cgi_journey_config, NULL},
    {"/api/places.json", cgi_forward, &places_forward_data},
    {"/api/journies.json", cgi_forward, &journies_forward_data},
    {"/api/status.json", cgi_status, NULL},
    {"/api/log.json", cgi_log, NULL},
    {"/api/syslog-config.json", cgi_syslog_config, NULL},
    {"/api/led-matrix-config.json", cgi_led_matrix_config, NULL},
    {"/api/led-matrix-status.json", cgi_led_matrix_status, NULL},
    {"/", cgi_fs, "/www/index.html"},
    {"/*", cgi_fs, NULL},
    {NULL, NULL, NULL}
};

struct websocket_url_handler websocket_url_tab[] = {
    {"/ws-echo", ws_echo_open, NULL, ws_echo_message, NULL},
    {"/ws-in", ws_in_open, ws_in_close, ws_in_message, NULL},
    {"/ws-out", ws_out_open, ws_out_close, NULL, NULL},
    {"/ws-display", ws_display_open, ws_display_close, NULL, NULL},

    {NULL, NULL, NULL, NULL, NULL}
};

void http_server_task(void *pvParameters)
{
    for(;;) {
        http_server_main(80);
        ERROR("The http server has returned!");
        vTaskDelayMs(5000);
    }
}
