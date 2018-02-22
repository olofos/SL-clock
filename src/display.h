#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <freertos/queue.h>

void display_task(void *pvParameters);

extern xQueueHandle display_message_queue;

#endif
