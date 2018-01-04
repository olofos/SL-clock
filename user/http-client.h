#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include <stdint.h>

enum http_transfer_encoding
{
    HTTP_TE_IDENTITY,
    HTTP_TE_CHUNKED,
    HTTP_TE_ERROR,
    HTTP_DONE
};

struct HTTPRequest
{
    const char *host;
    const char *path;
    uint16_t port;
    int fd;
    size_t content_length;
    enum http_transfer_encoding transfer_encoding;
    uint16_t chunk_size;
    int16_t status;
    int16_t poke;
};

int http_open(struct HTTPRequest *request);
int http_get(struct HTTPRequest *request);
int http_close(struct HTTPRequest *request);
size_t http_read(struct HTTPRequest *request, void *buf, size_t count);
int http_getc(struct HTTPRequest *request);
int http_peek(struct HTTPRequest *request);

#endif
