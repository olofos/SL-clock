#include <esp_common.h>

#include "i2c-master.h"
#include "display.h"
#include "matrix_framebuffer.h"
#include "sh1106-cmd.h"
#include "log.h"

#define LOG_SYS LOG_SYS_DISPLAY

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_RATE_MS)

xQueueHandle display_message_queue;

void display_task(void *pvParameters)
{
    display_message_queue = xQueueCreate(4, 1);
    i2c_master_init();
    for(;;) {
        if(i2c_start(OLED_I2C_ADDRESS, I2C_WRITE) == I2C_ACK) {
            i2c_stop();
            oled_display_main();
        }
        i2c_stop();
        LOG("SH1106 not found");

        vTaskDelayMs(25);

        if(i2c_start(MATRIX_I2C_ADDRESS, I2C_WRITE) == I2C_ACK) {
            i2c_stop();
            matrix_display_main();
        }
        i2c_stop();
        LOG("LED matrix not found");

        vTaskDelayMs(25);
    }
}
