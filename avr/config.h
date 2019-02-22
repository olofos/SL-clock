#ifndef CONFIG_H_
#define CONFIG_H_


//// Pins /////////////////////////////////////////////////////////////////////

// Bus out

#define PIN_MOSI PORTA1
#define PIN_MOSI_DDR DDRA
#define PIN_MOSI_PIN PINA
#define PIN_MOSI_PORT PORTA

#define PIN_SCK  PORTA3
#define PIN_SCK_DDR DDRA
#define PIN_SCK_PIN PINA
#define PIN_SCK_PORT PORTA

#define PIN_CS1  PORTA2
#define PIN_CS1_DDR DDRA
#define PIN_CS1_PIN PINA
#define PIN_CS1_PORT PORTA


#define PIN_CS2  PORTB2
#define PIN_CS2_DDR DDRB
#define PIN_CS2_PIN PINB
#define PIN_CS2_PORT PORTB

#define PIN_CS3  PORTB1
#define PIN_CS3_DDR DDRB
#define PIN_CS3_PIN PINB
#define PIN_CS3_PORT PORTB

#define PIN_CS4  PORTB0
#define PIN_CS4_DDR DDRB
#define PIN_CS4_PIN PINB
#define PIN_CS4_PORT PORTB

#define PIN_SCL  PORTA4
#define PIN_SCL_DDR DDRA
#define PIN_SCL_PIN PINA
#define PIN_SCL_PORT PORTA

#define PIN_SDA  PORTA6
#define PIN_SDA_DDR DDRA
#define PIN_SDA_PIN PINA
#define PIN_SDA_PORT PORTA

#define PIN_LDR  PORTA7
#define PIN_LDR_DDR DDRA
#define PIN_LDR_PIN PINA
#define PIN_LDR_PORT PORTA
#define PIN_LDR_ADC 7

#endif
