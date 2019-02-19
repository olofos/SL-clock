#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <freertos/queue.h>

enum DisplayType {
    DISPLAY_TYPE_NONE = 0,
    DISPLAY_TYPE_OLED,
    DISPLAY_TYPE_MATRIX,
};

extern enum DisplayType display_type;

void display_task(void *pvParameters);

void oled_display_main(void);
void matrix_display_main(void);

extern xQueueHandle display_message_queue;

#endif
