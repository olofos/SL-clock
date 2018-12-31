// Based on https://github.com/SonalPinto/Arduino_SSD1306_OLED

#include <stdint.h>
#include <esp_common.h>

#include "i2c-master.h"

#include "sh1106.h"
#include "sh1106-cmd.h"
#include "oled_framebuffer.h"

#include "log.h"
#define LOG_SYS LOG_SYS_SH1106

const uint8_t sh1106_init_seq[] = {
    // Tell the SH1106 that a command stream is incoming
    OLED_CONTROL_BYTE_CMD_STREAM,

    // Follow instructions on pg.64 of the dataSheet for software configuration of the SH1106
    // Turn the Display OFF
    OLED_CMD_DISPLAY_OFF,
    // Set mux ration tp select max number of rows - 64
    OLED_CMD_SET_MUX_RATIO,
    0x3F,
    // Set the display offset to 0
    OLED_CMD_SET_DISPLAY_OFFSET,
    0x00,

    // Mirror the x-axis. In case you set it up such that the pins are north.
    // 0xA0, // - in case pins are south - default
    OLED_CMD_SET_SEGMENT_REMAP,

    // Mirror the y-axis. In case you set it up such that the pins are north.
    // 0xC0, // - in case pins are south - default
    OLED_CMD_SET_COM_SCAN_MODE,

    // Default - alternate COM pin map
    OLED_CMD_SET_COM_PIN_MAP,
    0x12,
    // set contrast
    OLED_CMD_SET_CONTRAST,
    0x03,

    // Set display to enable rendering from GDDRAM (Graphic Display Data RAM)
    OLED_CMD_DISPLAY_RAM,
    // Normal mode!
    OLED_CMD_DISPLAY_NORMAL,
    // Default oscillator clock
    OLED_CMD_SET_DISPLAY_CLK_DIV,
    0x80,
    // Set precharge cycles to high cap type
    OLED_CMD_SET_PRECHARGE,
    0x22,
    // Set the V_COMH deselect volatage to max
    OLED_CMD_SET_VCOMH_DESELCT,
    0x30,
    // Turn the Display ON
    OLED_CMD_DISPLAY_ON,
};

#define OLED_START_COL 2

void oled_display(void)
{
    for(int y = 0; y < 8; y++) {
        const uint8_t init_page[] = {
            OLED_CONTROL_BYTE_CMD_STREAM,
            OLED_CMD_COL_ADDRESS_LOW + OLED_START_COL,
            OLED_CMD_COL_ADDRESS_HIGH,
            OLED_CMD_PAGE_ADDRESS + y,
        };

        i2c_start(OLED_I2C_ADDRESS, I2C_WRITE);
        i2c_write(init_page, sizeof(init_page));
        i2c_stop();


        i2c_start(OLED_I2C_ADDRESS, I2C_WRITE);
        i2c_write_byte(OLED_CONTROL_BYTE_DATA_STREAM);
        i2c_write(framebuffer + y * 128, 128);
        i2c_stop();
    }
}

int sh1106_init(void)
{
    int ret = i2c_start(OLED_I2C_ADDRESS, I2C_WRITE) != I2C_ACK;
    if(!ret) {
        i2c_write(sh1106_init_seq, sizeof(sh1106_init_seq));
        fb_blit = oled_blit;
    } else {
        LOG("No ACK");
    }
    i2c_stop();

    return ret;
}
