// Based on https://github.com/SonalPinto/Arduino_SSD1306_OLED

#include <stdint.h>
#include <string.h>

//#include "i2c-master.h"

#include "brzo_i2c.h"

#include "ssd1306.h"
#include "ssd1306-cmd.h"

#include "framebuffer.h"
#include "fonts.h"

#include "logo-paw-64x64.h"

#include "log.h"
#define LOG_SYS LOG_SYS_SSD1306

#define pgm_read_byte(p) (*(p))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

extern const uint8_t font[] PROGMEM;

const uint8_t ssd1306_init_seq[] PROGMEM = {
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

void fb_set_pixel(int16_t x,int16_t y,int8_t color)
{
    if((0 <= x) && (x < FB_WIDTH) && (0 <= y) && (y < FB_HEIGHT))
    {
        const uint16_t n = x + FB_WIDTH * (y / 8);
        const uint8_t m = 1 << (y % 8);

        if(color)
        {
            framebuffer[n] |= m;
        } else {
            framebuffer[n] &= ~m;
        }
    }
}

void fb_display(void)
{
    const uint8_t init[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_SET_COLUMN_RANGE, 0, 0x7F,
        OLED_CMD_SET_PAGE_RANGE, 0, 7,
        OLED_CMD_SET_DISPLAY_OFFSET, 0,
    };

    brzo_i2c_start_transaction(OLED_I2C_ADDRESS, SSD1306_I2C_FREQ);
    brzo_i2c_write(init, sizeof(init), 0);
    brzo_i2c_end_transaction();


    framebuffer_full[0] = OLED_CONTROL_BYTE_DATA_STREAM;

    brzo_i2c_start_transaction(OLED_I2C_ADDRESS, SSD1306_I2C_FREQ);
    brzo_i2c_write(framebuffer_full, sizeof(framebuffer_full), 0);
    brzo_i2c_end_transaction();
}

void fb_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer_full) - 1);
}



static void fb_blit(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t len)
{
    uint8_t raster_height = 1 + ((h - 1) / 8);
    int8_t offset = y & 0x07;

    for(uint16_t i = 0; i < len; i++)
    {
        uint8_t c = *data++;

        int16_t x_pos = x + (i / raster_height);
        int16_t data_pos = x_pos + ((y >> 3) + (i % raster_height)) * FB_WIDTH;

        if((0 <= x_pos) && (x_pos < FB_WIDTH))
        {
            if((0 <= data_pos) && (data_pos < FB_SIZE))
            {
                framebuffer[data_pos] |= c << offset;
            }
            if((0 <= data_pos + FB_WIDTH) && (data_pos + FB_WIDTH))
            {
                framebuffer[data_pos + FB_WIDTH] |= c >> (8 - offset);
            }
        }
    }
}

void fb_draw_string(int16_t x, int16_t y, const char *text, const uint8_t *font_data)
{
    const uint8_t char_height = font_data[FONT_HEIGHT_POS];
    const uint8_t first_char = font_data[FONT_FIRST_CHAR_POS];
    const uint8_t char_num = font_data[FONT_CHAR_NUM_POS];
    const uint16_t jump_table_size = char_num * FONT_JUMPTABLE_BYTES;

    uint8_t c;

    while((c = *text++))
    {
        if(c >= first_char && (c - first_char) < char_num)
        {
            c -= first_char;

            const uint8_t *jump_table_entry = font_data + FONT_JUMPTABLE_START + c * FONT_JUMPTABLE_BYTES;

            const uint16_t char_index = (jump_table_entry[FONT_JUMPTABLE_MSB] << 8) | jump_table_entry[FONT_JUMPTABLE_LSB];
            const uint8_t char_bytes = jump_table_entry[FONT_JUMPTABLE_SIZE];
            const uint8_t char_width = jump_table_entry[FONT_JUMPTABLE_WIDTH];

            if(char_index != 0xFFFF)
            {
                const uint8_t *char_p = font_data + FONT_JUMPTABLE_START + jump_table_size + char_index;
                fb_blit(x, y, char_width, char_height, char_p, char_bytes);
            }

            x += char_width;
        }
    }
}

uint16_t fb_string_length(const char *text, const uint8_t *font_data)
{
    const uint8_t first_char = font_data[FONT_FIRST_CHAR_POS];
    const uint8_t char_num = font_data[FONT_CHAR_NUM_POS];
    const uint16_t jump_table_size = char_num * FONT_JUMPTABLE_BYTES;

    uint8_t c;
    uint16_t len;

    while((c = *text++))
    {
        if(c >= first_char && (c - first_char) < char_num)
        {
            c -= first_char;
            const uint8_t *jump_table_entry = font_data + FONT_JUMPTABLE_START + c * FONT_JUMPTABLE_BYTES;
            len += jump_table_entry[FONT_JUMPTABLE_WIDTH];
        }
    }
    return len;
}

void ssd1306_init(void)
{
    brzo_i2c_start_transaction(OLED_I2C_ADDRESS, SSD1306_I2C_FREQ);
    brzo_i2c_write(ssd1306_init_seq, sizeof(ssd1306_init_seq), 0);
    brzo_i2c_end_transaction();
}

void fb_splash(void)
{
    fb_clear();

    for(int y = 0; y < 8; y++)
    {
        fb_blit(32, y * 8, 64, 8, &paw_64x64[y * 64], 64);
    }

    fb_display();
}

