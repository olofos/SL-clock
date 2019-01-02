#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <freertos/queue.h>

void display_task(void *pvParameters);

void oled_display_main(void);
void matrix_display_main(void);

extern xQueueHandle display_message_queue;

#endif
