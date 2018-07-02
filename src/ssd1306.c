// Based on https://github.com/SonalPinto/Arduino_SSD1306_OLED

#include <stdint.h>
#include <esp_common.h>

#ifdef BRZO
#include "brzo_i2c.h"
#else
#include "i2c-master.h"
#endif


#include "ssd1306.h"
#include "ssd1306-cmd.h"
#include "framebuffer.h"

#include "log.h"
#define LOG_SYS LOG_SYS_SSD1306

const uint8_t ssd1306_init_seq[] = {
    // Tell the SSD1306 that a command stream is incoming
    OLED_CONTROL_BYTE_CMD_STREAM,

    // Follow instructions on pg.64 of the dataSheet for software configuration of the SSD1306
    // Turn the Display OFF
    OLED_CMD_DISPLAY_OFF,
    // Set mux ration tp select max number of rows - 64
    OLED_CMD_SET_MUX_RATIO,
    0x3F,
    // Set the display offset to 0
    OLED_CMD_SET_DISPLAY_OFFSET,
    0x00,
    // Display start line to 0
    OLED_CMD_SET_DISPLAY_START_LINE,

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
    // Enable the charge pump
    OLED_CMD_SET_CHARGE_PUMP,
    0x14,
    // Set precharge cycles to high cap type
    OLED_CMD_SET_PRECHARGE,
    0x22,
    // Set the V_COMH deselect volatage to max
    OLED_CMD_SET_VCOMH_DESELCT,
    0x30,
    // Horizonatal addressing mode - same as the KS108 GLCD
    OLED_CMD_SET_MEMORY_ADDR_MODE,
    0x00,
    // Turn the Display ON
    OLED_CMD_DISPLAY_ON,
};

static uint8_t framebuffer_full[FB_SIZE + 1] = {[0] = OLED_CONTROL_BYTE_DATA_STREAM};
uint8_t *framebuffer = &framebuffer_full[1];

void fb_display(void)
{
    const uint8_t init[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_SET_COLUMN_RANGE, 0, 0x7F,
        OLED_CMD_SET_PAGE_RANGE, 0, 7,
        OLED_CMD_SET_DISPLAY_OFFSET, 0,
    };

#ifdef BRZO

    brzo_i2c_start_transaction(OLED_I2C_ADDRESS, SSD1306_I2C_FREQ);
    brzo_i2c_write(init, sizeof(init), 0);
    brzo_i2c_end_transaction();


    framebuffer_full[0] = OLED_CONTROL_BYTE_DATA_STREAM;

    brzo_i2c_start_transaction(OLED_I2C_ADDRESS, SSD1306_I2C_FREQ);
    brzo_i2c_write(framebuffer_full, sizeof(framebuffer_full), 0);
    brzo_i2c_end_transaction();
#else
    i2c_start(OLED_I2C_ADDRESS, I2C_WRITE);
    i2c_write(init, sizeof(init));
    i2c_stop();

    framebuffer_full[0] = OLED_CONTROL_BYTE_DATA_STREAM;

    i2c_start(OLED_I2C_ADDRESS, I2C_WRITE);
    i2c_write(framebuffer_full, sizeof(framebuffer_full));
    i2c_stop();
#endif
}

int ssd1306_init(void)
{
#ifdef BRZO
    brzo_i2c_start_transaction(OLED_I2C_ADDRESS, SSD1306_I2C_FREQ);
    brzo_i2c_write(ssd1306_init_seq, sizeof(ssd1306_init_seq), 0);
    int ret = brzo_i2c_end_transaction();
#else
    int ret = i2c_start(OLED_I2C_ADDRESS, I2C_WRITE) != I2C_ACK;
    if(!ret) {
        LOG("ACK");
        i2c_write(ssd1306_init_seq, sizeof(ssd1306_init_seq));
    } else {
        LOG("No ACK");
    }
    i2c_stop();
#endif

    return ret;
}
