#ifndef OLEDDISPLAYFONTS_h
#define OLEDDISPLAYFONTS_h

#include <stdint.h>

#define FONT_JUMPTABLE_BYTES 4

#define FONT_JUMPTABLE_MSB   0
#define FONT_JUMPTABLE_LSB   1
#define FONT_JUMPTABLE_SIZE  2
#define FONT_JUMPTABLE_WIDTH 3
#define FONT_JUMPTABLE_START 4

#define FONT_WIDTH_POS 0
#define FONT_HEIGHT_POS 1
#define FONT_FIRST_CHAR_POS 2
#define FONT_CHAR_NUM_POS 3

extern const uint8_t ArialMT_Plain_10[] PROGMEM;
extern const uint8_t ArialMT_Plain_16[] PROGMEM;
extern const uint8_t ArialMT_Plain_24[] PROGMEM;

#endif
