#ifndef DISPALY_MESSAGE_H_
#define DISPALY_MESSAGE_H_

enum display_message {
    DISPLAY_MESSAGE_NO_WIFI = 0,
    DISPLAY_MESSAGE_NO_JOURNIES = 1,

    DISPLAY_MESSAGE_NONE = 0xFF,
};

void display_post_message(enum display_message);
const char *display_get_message(enum display_message);

#endif
