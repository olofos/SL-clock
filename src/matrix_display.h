#ifndef MATRIX_DISPLAY_H_
#define MATRIX_DISPLAY_H_

#include "../avr/avr-i2c-led-matrix.h"

extern uint16_t matrix_intensity_low[AVR_I2C_NUM_LEVELS];
extern uint16_t matrix_intensity_high[AVR_I2C_NUM_LEVELS];
extern uint8_t matrix_intensity_override;
extern uint8_t matrix_intensity_override_level;
extern uint8_t matrix_intensity_updated;

extern uint8_t matrix_intensity_level;
extern uint16_t matrix_intensity_adc;

extern xSemaphoreHandle matrix_intensity_mutex;

#endif
