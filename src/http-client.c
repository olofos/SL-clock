#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <esp_common.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include "http-client.h"

#include "log.h"

xSemaphoreHandle http_mutex = 0;

#define LOG_SYS LOG_SYS_HTTP

#define HTTP_PROTOCOL_11 "HTTP/1.1"

static int http_read_until(int fd, char *s, char *buf, int buf_len)
{
    int n = 0;
    char c;

    for(;;)
    {
        if(!read(fd, &c, 1))
        {
            LOG("Got unexpected end of file");
            return 0;
        }

        if(buf)
        {
            if(n < buf_len - 1)
            {
                buf[n] = c;
            } else if(n == buf_len - 1) {
                buf[n] = 0;
            }
        }

        n++;

        if(strchr(s, c))
        {
            if(buf && n - 1 < buf_len)
            {
                buf[n - 1] = 0;
            }
            return c;
        }
    }
}

static int http_expect(int fd, char c)
{
    char d;
    read(fd, &d, 1);

    if(c != d)
    {
        LOG("Expected '%c' (%d) got '%c' (%d)", c, c, d, d);
    }

    return c == d;
}

static void http_read_chunk_header(struct HTTPRequest *request, char *buf, int buf_len)
{
    http_read_until(request->fd, ";\r", buf, buf_len);

    char *p;
    request->chunk_size = strtol(buf, &p, 16);

    if(p == buf)
    {
        LOG("Error parsing chunk length");
    }

    request->content_length += request->chunk_size;

    LOG("Chunk size %d", request->chunk_size);

    http_read_until(request->fd, "\n", 0, 0);
}

static void http_parse_header(int fd, struct HTTPRequest *request)
{
    char buf[32];
    char *p;

    read(fd,buf,strlen(HTTP_PROTOCOL_11));

    buf[strlen(HTTP_PROTOCOL_11)] = 0;
    if(strcmp(buf, HTTP_PROTOCOL_11) != 0)
    {
        LOG("Expected '%s', got '%s'", HTTP_PROTOCOL_11, buf);
    }

    http_expect(fd, ' ');
    http_read_until(fd, " ", buf, sizeof(buf));
    request->status = strtol(buf, &p, 10);

    if(*p)
    {
        LOG("Error parsing status code '%s'", buf);
    } else {
        LOG("Status code: %d ('%s')", request->status, buf);
    }

    http_read_until(fd, "\n", 0, 0);
    request->content_length = -1;
    request->transfer_encoding = HTTP_TE_IDENTITY;

    char c;
    while((c = http_read_until(fd, ":\n", buf, sizeof(buf))))
    {
        if(c == '\n')
        {
            break;
        }

        if(strcmp(buf, "Content-Length") == 0)
        {
            http_expect(fd, ' ');
            http_read_until(fd, "\r", buf, sizeof(buf));
            http_expect(fd, '\n');
            request->content_length = strtol(buf, &p, 10);

            if(*p)
            {
                LOG("Error parsing content length '%s', %d, %c, %02x", buf, p - buf, *p, *p);
            } else {
                LOG("Content length: %d ('%s')", request->content_length, buf);
            }
        } else if(strcmp(buf, "Transfer-Encoding") == 0) {
            http_expect(fd, ' ');
            http_read_until(fd, "\r", buf, sizeof(buf));
            http_expect(fd, '\n');

            if(strcmp(buf, "chunked") == 0)
            {
                request->transfer_encoding = HTTP_TE_CHUNKED;
                LOG("TE: chunked");
            } else if(strcmp(buf, "identity") != 0) {
                request->transfer_encoding = HTTP_TE_ERROR;
                LOG("TE: not supported");
            }
        } else {
            http_read_until(fd, "\n", 0, 0);
        }
    }

    if(c != '\n')
    {
        LOG("Expected '\\n'");
    }

    if(request->transfer_encoding == HTTP_TE_CHUNKED)
    {
        http_read_chunk_header(request, buf, sizeof(buf));
    }
}

int http_open(struct HTTPRequest *request)
{
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    char port_str[6];
    //itoa(request->port, port_str, 10);
    sprintf(port_str, "%d", request->port);

    int err = getaddrinfo(request->host, port_str, &hints, &res);

    if (err != 0 || res == NULL)
    {
        LOG("DNS lookup for %s failed err=%d res=%p", request->host, err, res);
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    struct sockaddr *sa = res->ai_addr;
    if (sa->sa_family == AF_INET)
    {
        LOG("DNS lookup for %s succeeded. IP=%s", request->host, inet_ntoa(((struct sockaddr_in *)sa)->sin_addr));
    }

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0)
    {
        LOG("Failed to allocate socket.");
        freeaddrinfo(res);
        return -1;
    }

    LOG("Allocated socket");

    if(connect(s, res->ai_addr, res->ai_addrlen) != 0)
    {
        close(s);
        freeaddrinfo(res);
        LOG("Socket connect failed.");

        return -1;
    }

    LOG("Connected");

    freeaddrinfo(res);

    request->poke = -1;

    return request->fd = s;
}

int http_get(struct HTTPRequest *request)
{
    const char *messages[5] = {
        "GET ", request->path, " HTTP/1.1\r\n"
        "Host: ", request->host, "\r\n"
        "User-Agent: esp-open-rtos/0.1 esp8266\r\n"
        "Connection: close\r\n"
        "\r\n"
    };

    uint8_t i;

    for(i = 0; i < sizeof(messages)/sizeof(messages[0]); i++)
    {
        int ret = write(request->fd, messages[i], strlen(messages[i]));

        if(ret < 0)
        {
            LOG("Socket send failed");
            close(request->fd);
            return -1;
        }
    }

    http_parse_header(request->fd, request);

    return request->fd;
}

size_t http_read(struct HTTPRequest *request, void *vbuf, size_t count)
{
    int ret = 0;
    unsigned char *buf = vbuf;
    if(count > 0 && request->poke >= 0)
    {
        *buf++ = request->poke;
        count--;
        ret++;
        request->poke = -1;
    }

    if(count > 0)
    {
        if(request->transfer_encoding == HTTP_TE_IDENTITY)
        {
            ret += read(request->fd, buf, count);
        } else if(request->transfer_encoding == HTTP_TE_CHUNKED) {
            if(request->chunk_size == 0)
            {
                char str[32];
                http_read_until(request->fd, "\n", 0, 0);
                http_read_chunk_header(request, str, sizeof(str));

                if(request->chunk_size == 0)
                {
                    http_expect(request->fd, '\r');
                    http_expect(request->fd, '\n');

                    request->transfer_encoding = HTTP_DONE;

                    return 0;
                }
            }


            if(count > request->chunk_size)
            {
                count = request->chunk_size;
            }

            size_t len = read(request->fd, buf, count);
            request->chunk_size -= len;
            ret += len;
        } else if(request->transfer_encoding == HTTP_DONE) {
            return 0;
        } else {
            return -1;
        }
    }
    return ret;
}

int http_getc(struct HTTPRequest *request)
{
    unsigned char c;
    if(http_read(request, &c, 1) > 0)
    {
        return c;
    } else {
        return -1;
    }
}

int http_peek(struct HTTPRequest *request)
{
    if(request->poke < 0)
    {
        unsigned char c;
        if(http_read(request, &c, 1) > 0)
        {
            request->poke = c;
        } else {
            request->poke = -1;
        }
    }
    return request->poke;
}

int http_close(struct HTTPRequest *request)
{
    return close(request->fd);
}
