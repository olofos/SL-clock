#include <avr/io.h>
#include <util/delay.h>

#include "io.h"
#include "config.h"
#include "max7219.h"

#define NUM_MATRICES 4
#define NUM_ROWS 4

#define REV(b) ((((b) & 0x80) >> 7) | (((b) & 0x40) >> 5) | (((b) & 0x20) >> 3) | (((b) & 0x10) >> 1) | (((b) & 0x08) << 1) | (((b) & 0x04) << 3) | (((b) & 0x02) << 5) | (((b) & 0x01) << 7))

uint8_t framebuffer[8*NUM_MATRICES*NUM_ROWS] = {
    0b00000000, 0b00000000, 0b00000100, 0b01000000,
    0b00000011, 0b00000000, 0b00000010, 0b10000000,
    0b00000100, 0b10000000, 0b00000001, 0b00000000,
    0b00001000, 0b01000000, 0b00000010, 0b10000000,
    0b00001000, 0b01000000, 0b00000100, 0b01000000,
    0b00000100, 0b10000000, 0b00000000, 0b00000000,
    0b00000011, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,

    0b00000000, 0b00001000, 0b00000000, 0b01101100,
    0b00000000, 0b00001000, 0b00110000, 0b10010010,
    0b00000000, 0b00001000, 0b01001000, 0b10000010,
    0b00000000, 0b00001000, 0b10000100, 0b01000100,
    0b00000000, 0b00001000, 0b10000100, 0b00101000,
    0b00000000, 0b00001000, 0b01001000, 0b00010000,
    0b00000000, 0b00001000, 0b00110000, 0b00000000,
    0b11111111, 0b00001000, 0b00000000, 0b00000000,

    0b10000000, 0b00000000, 0b00000000, 0b00000010,
    0b01000000, 0b00000001, 0b00000000, 0b00000100,
    0b00100000, 0b00000010, 0b10000000, 0b00001000,
    0b00010000, 0b00000100, 0b01000000, 0b00010000,
    0b00001000, 0b00000010, 0b10000000, 0b00100000,
    0b00000100, 0b00000001, 0b00000000, 0b01000000,
    0b00000010, 0b00000000, 0b00000000, 0b10000000,
    0b00000001, 0b00000000, 0b00000001, 0b00000000,

    0b00000000, 0b10000000, 0b00000010, 0b00000000,
    0b00000000, 0b01000000, 0b00000100, 0b00000000,
    0b00000000, 0b00100000, 0b00001000, 0b00000000,
    0b00000000, 0b00010000, 0b00010000, 0b00000000,
    0b00000000, 0b00001000, 0b00100000, 0b00000000,
    0b00000000, 0b00000100, 0b01000000, 0b00000000,
    0b00000000, 0b00000010, 0b10000000, 0b00000000,
    0b00000000, 0b00000001, 0b00000000, 0b00000000,
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
    for(uint8_t i = 0; i < NUM_MATRICES; i++) {
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

void MAX7219_init(uint8_t cs)
{
    MAX7219_cmd(MAX7219_REG_SCANLIMIT, 0x07, cs);
    MAX7219_cmd(MAX7219_REG_DECODEMODE, 0x00, cs);
    MAX7219_cmd(MAX7219_REG_DISPLAYTEST, 0x00, cs);
    MAX7219_cmd(MAX7219_REG_INTENSITY, 0x07, cs);
    MAX7219_cmd(MAX7219_REG_SHUTDOWN, 0x01, cs);
}

uint8_t rev(uint8_t d)
{
    uint8_t n = 0;
    for(int i = 0; i < 8; i++) {
        if(d & (1 << i)) {
            n |= 1 << (7-i);

        }
    }
    return n;
}

void fb_show(void)
{
    int n = 3;

    n = 3;
    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS1);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(rev(framebuffer[n--]));
        }
        SPI_CS_high(CS1);
        n += 8;
    }

    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS2);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(rev(framebuffer[n--]));
        }
        SPI_CS_high(CS2);
        n += 8;
    }

    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS3);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(rev(framebuffer[n--]));
        }
        SPI_CS_high(CS3);
        n += 8;
    }

    for(uint8_t y = 8; y >= 1; y--) {
        SPI_CS_low(CS4);
        for(uint8_t x = 0; x < 4; x++) {
            SPI_transfer(y);
            SPI_transfer(rev(framebuffer[n--]));
        }
        SPI_CS_high(CS4);
        n += 8;
    }
}

#define fb_transfer_byte(y, index) do { SPI_transfer(REV(y)); SPI_transfer(framebuffer[index]); } while(0)

#define fb_transfer_line(cs, y, index) do {                \
    SPI_CS_low(cs);                                     \
    fb_transfer_byte(y, index + 3);                        \
    fb_transfer_byte(y, index + 2);                        \
    fb_transfer_byte(y, index + 1);                        \
    fb_transfer_byte(y, index + 0);                        \
    SPI_CS_high(cs);                                    \
    } while(0)

#if 0
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

int main(void)
{
    IO_init();
    SPI_init();

    MAX7219_init(CS1 | CS2 | CS3 | CS4);
    MAX7219_blank(CS1 | CS2 | CS3 | CS4);


    for(;;) {
        MAX7219_blank(CS1 | CS2 | CS3 | CS4);
        SPI_CS_low(CS1 | CS2 | CS3 | CS4);

        for(int j = 1; j <= 4; j++) {
            SPI_transfer(j);
            SPI_transfer(0x81);
        }

        SPI_CS_high(CS1 | CS2 | CS3 | CS4);

        SPI_CS_low(CS1 | CS2 | CS3 | CS4);

        for(int j = 1; j <= 4; j++) {
            SPI_transfer(9-j);
            SPI_transfer(0x81);
        }

        SPI_CS_high(CS1 | CS2 | CS3 | CS4);


        _delay_ms(500);

        fb_show();

        _delay_ms(500);
    }
}
