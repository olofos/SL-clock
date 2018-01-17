#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "framebuffer.h"

static uint8_t reverse(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

struct icon *fb_load_icon_pbm(const char *filename)
{
    printf("Loading icon from file %s\n", filename);

    int fd = open(filename, O_RDONLY);
    if(!fd)
    {
        return 0;
    }

    char buf[8];

    read(fd, buf, 3);

    if((buf[0] != 'P') || (buf[1] != '4') || (buf[2] != '\n'))
    {
        buf[3] = 0;
        printf("Unexpected PBM magic '%s'\n", buf);
        close(fd);
        return 0;
    }

    int i = 0;

    do {
        read(fd, &buf[i++], 1);
    } while((i < sizeof(buf)-1) && (buf[i-1] != ' '));

    buf[i] = 0;
    if(buf[i-1] != ' ')
    {
        printf("Could not find width of PBM (got '%s')\n", buf);
        close(fd);
        return 0;
    }

    int h = strtol(buf, 0, 10);

    i = 0;

    do {
        read(fd, &buf[i++], 1);
    } while((i < sizeof(buf)-1) && (buf[i-1] != '\n'));

    buf[i] = 0;
    if(buf[i-1] != '\n')
    {
        printf("Could not find height of PBM (got '%s')\n", buf);
        close(fd);
        return 0;
    }

    int w = strtol(buf, 0, 10);

    struct icon *icon = malloc(sizeof(struct icon));

    int h_bytes = 1 + (h - 1) / 8;

    icon->width = w;
    icon->height = h;

    uint8_t *data = malloc(h_bytes * w);

    if(data)
    {
        read(fd, data, h_bytes * w);

        for(int j = 0; j < h_bytes * w; j++)
        {
            data[j] = reverse(data[j]);
        }
    }

    icon->data = data;

    close(fd);
    return icon;
}

void fb_free_icon(struct icon *icon)
{
    if(icon)
    {
        if(icon->data)
        {
            free((char *)icon->data);
        }
        free(icon);
    }
}
