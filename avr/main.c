#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "io.h"
#include "config.h"
#include "max7219.h"

#define FLAG_REDRAWING 0x01
#define FLAG_FB_UPDATED 0x02

uint8_t fb_write_index;
uint8_t read_index;

volatile uint8_t prev_adc_value_hi;
volatile uint8_t prev_adc_value_lo;
volatile uint8_t prev_intensity;

#define I2C_ADDRESS 0x5B

#define NUM_MATRIX_COLS 4
#define NUM_MATRIX_ROWS 4

#define REV(b) ((((b) & 0x80) >> 7) | (((b) & 0x40) >> 5) | (((b) & 0x20) >> 3) | (((b) & 0x10) >> 1) | (((b) & 0x08) << 1) | (((b) & 0x04) << 3) | (((b) & 0x02) << 5) | (((b) & 0x01) << 7))

volatile uint8_t framebuffer[8*NUM_MATRIX_COLS*NUM_MATRIX_ROWS] = {
    REV(0b00000000), REV(0b00000000), REV(0b00000000), REV(0b00000000),
    REV(0b00000000), REV(0b00001111), REV(0b11110000), REV(0b00000000),
    REV(0b00000000), REV(0b01110000), REV(0b00001110), REV(0b00000000),
    REV(0b00000001), REV(0b10000000), REV(0b00000001), REV(0b10000000),
    REV(0b00000010), REV(0b00000000), REV(0b00000000), REV(0b01000000),
    REV(0b00000100), REV(0b00011000), REV(0b00011000), REV(0b00100000),
    REV(0b00001000), REV(0b00100100), REV(0b00100100), REV(0b00010000),
    REV(0b00010000), REV(0b01000010), REV(0b01000010), REV(0b00001000),
    REV(0b00100000), REV(0b01000010), REV(0b01000010), REV(0b00000100),
    REV(0b00100000), REV(0b01000010), REV(0b01000010), REV(0b00000100),
    REV(0b01000000), REV(0b01000010), REV(0b01000010), REV(0b00000010),
    REV(0b01000111), REV(0b00100100), REV(0b00100100), REV(0b11100010),
    REV(0b10001000), REV(0b10011000), REV(0b00011001), REV(0b00010001),
    REV(0b10010000), REV(0b01000000), REV(0b00000010), REV(0b00001001),
    REV(0b10010000), REV(0b00100000), REV(0b00000100), REV(0b00001001),
    REV(0b10001000), REV(0b00100000), REV(0b00000100), REV(0b00010001),
    REV(0b10001000), REV(0b00100011), REV(0b11000100), REV(0b00010001),
    REV(0b10000100), REV(0b01001100), REV(0b00110010), REV(0b00100001),
    REV(0b10000011), REV(0b10010000), REV(0b00001001), REV(0b11000001),
    REV(0b10000000), REV(0b00100000), REV(0b00000100), REV(0b00000001),
    REV(0b01000000), REV(0b01000000), REV(0b00000010), REV(0b00000010),
    REV(0b01000000), REV(0b01000000), REV(0b00000010), REV(0b00000010),
    REV(0b01000000), REV(0b10000000), REV(0b00000001), REV(0b00000010),
    REV(0b00100000), REV(0b10000000), REV(0b00000001), REV(0b00000100),
    REV(0b00100000), REV(0b10000000), REV(0b00000001), REV(0b00000100),
    REV(0b00010000), REV(0b10000011), REV(0b11000001), REV(0b00001000),
    REV(0b00001000), REV(0b01001100), REV(0b00110010), REV(0b00010000),
    REV(0b00000100), REV(0b00110000), REV(0b00001100), REV(0b00100000),
    REV(0b00000010), REV(0b00000000), REV(0b00000000), REV(0b01000000),
    REV(0b00000001), REV(0b10000000), REV(0b00000001), REV(0b10000000),
    REV(0b00000000), REV(0b01110000), REV(0b00001110), REV(0b00000000),
    REV(0b00000000), REV(0b00001111), REV(0b11110000), REV(0b00000000),
};

// Missing definitions for USART in SPI mode
#define    UCPHA0          UCSZ00
#define    UDORD0          UCSZ01

#define CS1 1
#define CS2 2
#define CS3 4
#define CS4 8


void IO_init(void)
{
    set_output(PIN_CS1);
    set_output(PIN_CS2);
    set_output(PIN_CS3);
    set_output(PIN_CS4);

    // Note: all CS pins start out low

    set_output(PIN_MOSI);
    set_output(PIN_SCK);
}

void SPI_init(void)
{
    UBRR0 = 0;
    set_output(PIN_SCK);

    UCSR0C = (1 << UMSEL01) | (1 << UMSEL00) | (0 << UDORD0) | (0 << UCPHA0) | (0 << UCPOL0);
    UCSR0B = (0 << RXEN0) | (1 << TXEN0);

    const uint32_t baud = 4000000;
    const uint16_t ubrr = (F_CPU - 2 * baud) / (2 * baud);

    UBRR0 = ubrr;
}

void SPI_transfer(uint8_t data)
{
    while(!(UCSR0A & (1 << UDRE0))) {
    }

    UDR0 = data;
}

void SPI_CS_low(uint8_t cs)
{
    if(cs & CS1) {
        set_low(PIN_CS1);
    }
    if(cs & CS2) {
        set_low(PIN_CS2);
    }
    if(cs & CS3) {
        set_low(PIN_CS3);
    }
    if(cs & CS4) {
        set_low(PIN_CS4);
    }
}

void SPI_CS_high(uint8_t cs)
{
    while(!(UCSR0A & (1 << TXC0))) {
    }

    UCSR0A |= 1 << TXC0;

    if(cs & CS1) {
        set_high(PIN_CS1);
    }
    if(cs & CS2) {
        set_high(PIN_CS2);
    }
    if(cs & CS3) {
        set_high(PIN_CS3);
    }
    if(cs & CS4) {
        set_high(PIN_CS4);
    }
}

void MAX7219_cmd(uint8_t reg, uint8_t val, uint8_t cs)
{
    SPI_CS_low(cs);
    for(uint8_t i = 0; i < NUM_MATRIX_COLS; i++) {
        SPI_transfer(reg);
        SPI_transfer(val);
    }
    SPI_CS_high(cs);
}

void MAX7219_blank(uint8_t cs)
{
    for(int i = 1; i <= 8; i++) {
        MAX7219_cmd(i, 0, cs);
    }
}

void MAX7219_init(uint8_t cs, uint8_t intensity)
{
    MAX7219_cmd(MAX7219_REG_SCANLIMIT, 0x07, cs);
    MAX7219_cmd(MAX7219_REG_DECODEMODE, 0x00, cs);
    MAX7219_cmd(MAX7219_REG_DISPLAYTEST, 0x00, cs);
    MAX7219_cmd(MAX7219_REG_INTENSITY, intensity, cs);
    MAX7219_cmd(MAX7219_REG_SHUTDOWN, 0x01, cs);
}

void fb_show(void)
{
    int n = 3;

    n = 3;
    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS1);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(framebuffer[n--]);
        }
        SPI_CS_high(CS1);
        n += 8;
    }

    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS2);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(framebuffer[n--]);
        }
        SPI_CS_high(CS2);
        n += 8;
    }

    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS3);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(framebuffer[n--]);
        }
        SPI_CS_high(CS3);
        n += 8;
    }

    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS4);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(framebuffer[n--]);
        }
        SPI_CS_high(CS4);
        n += 8;
    }
}

#if 0

#define fb_transfer_byte(y, index) do { SPI_transfer(REV(y)); SPI_transfer(framebuffer[index]); } while(0)

#define fb_transfer_line(cs, y, index) do {     \
        SPI_CS_low(cs);                         \
        fb_transfer_byte(y, index + 3);         \
        fb_transfer_byte(y, index + 2);         \
        fb_transfer_byte(y, index + 1);         \
        fb_transfer_byte(y, index + 0);         \
        SPI_CS_high(cs);                        \
    } while(0)

void fb_test(void)
{
    fb_transfer_line(CS1, 8, 0);
    fb_transfer_line(CS1, 7, 4);
    fb_transfer_line(CS1, 6, 8);
    fb_transfer_line(CS1, 5, 12);
    fb_transfer_line(CS1, 4, 16);
    fb_transfer_line(CS1, 3, 20);
    fb_transfer_line(CS1, 2, 24);
    fb_transfer_line(CS1, 1, 28);

    fb_transfer_line(CS2, 8, 32);
    fb_transfer_line(CS2, 7, 36);
    fb_transfer_line(CS2, 6, 40);
    fb_transfer_line(CS2, 5, 44);
    fb_transfer_line(CS2, 4, 48);
    fb_transfer_line(CS2, 3, 52);
    fb_transfer_line(CS2, 2, 56);
    fb_transfer_line(CS2, 1, 60);

    fb_transfer_line(CS3, 8, 64);
    fb_transfer_line(CS3, 7, 68);
    fb_transfer_line(CS3, 6, 72);
    fb_transfer_line(CS3, 5, 76);
    fb_transfer_line(CS3, 4, 80);
    fb_transfer_line(CS3, 3, 84);
    fb_transfer_line(CS3, 2, 88);
    fb_transfer_line(CS3, 1, 92);

    fb_transfer_line(CS4, 8, 96);
    fb_transfer_line(CS4, 7, 100);
    fb_transfer_line(CS4, 6, 104);
    fb_transfer_line(CS4, 5, 108);
    fb_transfer_line(CS4, 4, 112);
    fb_transfer_line(CS4, 3, 116);
    fb_transfer_line(CS4, 2, 120);
    fb_transfer_line(CS4, 1, 124);
}
#endif


void i2c_init(void)
{
    TWSA = I2C_ADDRESS << 1;
    TWSAM = 0;
    TWSCRA = _BV(TWSHE) | _BV(TWDIE) | _BV(TWASIE) | _BV(TWEN) | _BV(TWSIE);
}

void adc_init(void)
{
    ADMUXA = PIN_LDR_ADC;
    ADMUXB = _BV(REFS2) | _BV(REFS1);
    DIDR0 = _BV(PIN_LDR_ADC);
    ADCSRB = 0;
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADATE) | _BV(ADPS2) | _BV(ADPS1);
}

#define NUM_LIGHT_LEVELS 8

uint8_t calc_intensity(uint8_t current_intensity, uint16_t adc_result)
{
    static const uint16_t light_tab_low[NUM_LIGHT_LEVELS] = {
        0, 8, 16, 32, 64, 128, 256, 512
    };

    static const uint16_t light_tab_high[NUM_LIGHT_LEVELS] = {
        12, 23, 46, 91, 182, 363, 725, 1023
    };

    if(adc_result < light_tab_low[current_intensity]) {
        current_intensity--;
    }
    if(adc_result > light_tab_high[current_intensity]) {
        current_intensity++;
    }

    return current_intensity;
}

#define NUM_ADC_VALUES 3

int main(void)
{
    IO_init();
    SPI_init();
    i2c_init();
    adc_init();

    MAX7219_init(CS1 | CS2 | CS3 | CS4, 0);
    MAX7219_blank(CS1 | CS2 | CS3 | CS4);

    GPIOR0 |= FLAG_FB_UPDATED;
    sei();

    uint8_t current_intensity = 0;

    uint16_t adc_values[NUM_ADC_VALUES] = {0};
    uint8_t adc_value_index = 0;

    for(;;) {
        if(GPIOR0 & FLAG_FB_UPDATED) {
            GPIOR0 |= FLAG_REDRAWING;

            uint16_t adc_value = ADC;

            adc_values[adc_value_index++] = adc_value;

            if(adc_value_index >= NUM_ADC_VALUES) {
                adc_value_index = 0;
            }

            uint8_t intensities[NUM_ADC_VALUES];

            for(uint8_t i = 0; i < NUM_ADC_VALUES; i++) {
                intensities[i] = calc_intensity(current_intensity, adc_values[i]);
            }

            uint8_t all_equal = 1;
            for(uint8_t i = 0; i < NUM_ADC_VALUES-1; i++) {
                if(intensities[i+1] != intensities[i]) {
                    all_equal = 0;
                    break;
                }
            }

            if(all_equal) {
                current_intensity = intensities[0];
            }

            prev_adc_value_hi = (adc_value >> 8) & 0xFF;
            prev_adc_value_lo = adc_value & 0xFF;
            prev_intensity = current_intensity;

            MAX7219_init(CS1 | CS2 | CS3 | CS4, current_intensity);
            fb_show();

            GPIOR0 &= ~(FLAG_REDRAWING | FLAG_FB_UPDATED);
        }
    }
}

#define TW_CMD_ACK (_BV(TWCMD1) | _BV(TWCMD0))
#define TW_CMD_NACK (_BV(TWAA) | _BV(TWCMD1) | _BV(TWCMD0))
#define TW_CMD_ACK_WAIT_START (_BV(TWCMD1))
#define TW_CMD_NACK_WAIT_START (_BV(TWAA) | _BV(TWCMD1))

// Inspired by https://github.com/orangkucing/WireS/blob/master/WireS.cpp

ISR(TWI_SLAVE_vect)
{
    uint8_t status = TWSSRA;

    if(status & (_BV(TWC) | _BV(TWBE))) {
        TWSSRA |= _BV(TWASIF) | _BV(TWDIF) | _BV(TWBE); // Release hold
    } else if(status & _BV(TWASIF)) {
        if(status & _BV(TWAS)) { // Address received
            if(GPIOR0 & FLAG_REDRAWING) {
                TWSCRB = TW_CMD_NACK;
            } else {
                if(status & _BV(TWDIR)) {
                    read_index = 0;
                } else {
                    fb_write_index = 0;
                }

                TWSCRB = TW_CMD_ACK;
            }
        } else { // Stop condition
            if(!(status & _BV(TWDIR))) {
                GPIOR0 |= FLAG_FB_UPDATED;
            }

            TWSSRA = _BV(TWASIF);
        }
    } else if(status & _BV(TWDIF)) {
        if(status & _BV(TWDIR)) { // Send data to master
            switch(read_index) {
            case 0:
                TWSD = prev_adc_value_hi;
                read_index++;
                break;
            case 1:
                TWSD = prev_adc_value_lo;
                read_index++;
                break;
            default:
                TWSD = prev_intensity;
                break;
            }
            TWSCRB = TW_CMD_ACK;
        } else { // Data received
            framebuffer[fb_write_index] = TWSD;
            fb_write_index = (fb_write_index + 1) % sizeof(framebuffer);
            TWSCRB = TW_CMD_ACK;
        }
    }
}
