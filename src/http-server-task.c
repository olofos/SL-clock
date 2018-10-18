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


static const char *WWW_DIR = "/www";
static const char *GZIP_EXT = ".gz";

struct http_spiffs_response
{
    int fd;
    char buf[128];
};

struct http_mime_map
{
    const char *ext;
    const char *type;
};

static const struct http_mime_map mime_tab[] = {
    {"html", "text/html"},
    {"css", "text/css"},
    {"js", "text/javascript"},
    {"png", "image/png"},
    {"svg", "image/svg+xml"},
    {"json", "application/json"},
    {NULL, "text/plain"},
};

static const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');

    const struct http_mime_map *p;

    if(!ext) {
        return "text/plain";
    }

    ext++;

    for(p = mime_tab; p->ext; p++) {
        if(!strcmp(ext, p->ext)) {
            break;
        }
    }
    return p->type;
}

enum http_cgi_state cgi_spiffs(struct http_request* request)
{
    if(request->method != HTTP_METHOD_GET) {
        return HTTP_CGI_NOT_FOUND;
    }

    if(!request->cgi_data) {
        const char *base_filename;
        const char *www_prefix;

        if(request->cgi_arg) {
            base_filename = request->cgi_arg;
            www_prefix = "";
        } else {
            base_filename = request->path;
            www_prefix = WWW_DIR;
        }

        char *filename;

        filename = malloc(strlen(WWW_DIR) + strlen(base_filename) + strlen(GZIP_EXT) + 1);

        if(!filename) {
            return HTTP_CGI_NOT_FOUND;
        }

        strcpy(filename, www_prefix);
        strcat(filename, base_filename);

        uint8_t file_flag = 0;
        int fd = -1;

        if(!file_flag && (request->flags & HTTP_FLAG_ACCEPT_GZIP)) {
            strcat(filename, GZIP_EXT);

            fd = open(filename, O_RDONLY);

            if(fd >= 0) {
                INFO("Opened file '%s'", filename);

                file_flag |= HTTP_FLAG_ACCEPT_GZIP;
            } else {
                INFO("File '%s' not found", filename);
            }

            filename[strlen(filename) - strlen(GZIP_EXT)] = 0;
        }

        if(!file_flag) {
            fd = open(filename, O_RDONLY);

            if(fd >= 0) {
                INFO("Opened file '%s'", filename);
            } else {
                INFO("File '%s' not found", filename);
            }
        }

        if(fd < 0) {
            free(filename);
            return HTTP_CGI_NOT_FOUND;
        }

        struct stat s;
        fstat(fd, &s);
        INFO("File size: %d", s.st_size);

        const char *mime_type = get_mime_type(filename);

        free(filename);


        request->cgi_data = malloc(sizeof(struct http_spiffs_response));

        if(!request->cgi_data) {
            return HTTP_CGI_NOT_FOUND;
        }

        struct http_spiffs_response *resp = request->cgi_data;

        resp->fd = fd;

        http_begin_response(request, 200, mime_type);
        http_write_header(request, "Cache-Control", "max-age=3600, must-revalidate");
        http_set_content_length(request, s.st_size);

        if(file_flag & HTTP_FLAG_ACCEPT_GZIP) {
            http_write_header(request, "Content-Encoding", "gzip");
        }
        http_end_header(request);

        return HTTP_CGI_MORE;
    } else {
        struct http_spiffs_response *resp = request->cgi_data;

        int n = read(resp->fd, resp->buf, sizeof(resp->buf));

        if(n > 0) {
            http_write_bytes(request, resp->buf, n);
        }

        if(n < sizeof(resp->buf)) {
            http_end_body(request);

            close(resp->fd);
            free(request->cgi_data);

            return HTTP_CGI_DONE;
        } else {
            return HTTP_CGI_MORE;
        }
    }
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
    {"/api/wifi-scan.json", cgi_wifi_scan, NULL},
    {"/api/wifi-list.json", cgi_wifi_list, NULL},
    {"/api/journies-config.json", cgi_journey_config, NULL},
    {"/api/places.json", cgi_forward, &places_forward_data},
    {"/api/journies.json", cgi_forward, &journies_forward_data},
    {"/api/status.json", cgi_status, NULL},
    {"/api/log.json", cgi_log, NULL},
    {"/api/syslog-config.json", cgi_syslog_config, NULL},
    {"/", cgi_spiffs, "/www/index.html"},
    {"/*", cgi_spiffs, NULL},
    {NULL, NULL, NULL}
};

void http_server_task(void *pvParameters)
{
    for(;;) {
        http_server_main(80);
        ERROR("The http server has returned!");
        vTaskDelayMs(5000);
    }
}
