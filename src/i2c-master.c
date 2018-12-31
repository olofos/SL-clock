#include <esp_common.h>

#include "gpio.h"
#include "i2c-master.h"

static inline void i2c_sda_high(void)
{
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << I2C_SDA_GPIO);
}

static inline void i2c_sda_low(void)
{
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << I2C_SDA_GPIO);
}

static inline uint16_t i2c_sda_read(void)
{
    return ( GPIO_REG_READ(GPIO_IN_ADDRESS) & (1 << I2C_SDA_GPIO) );
}

static inline void i2c_sda_input(void)
{
    GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, 1 << I2C_SDA_GPIO);
}

static inline void i2c_sda_output(void)
{
    GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, 1 << I2C_SDA_GPIO);
}



static inline void i2c_scl_high(void)
{
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << I2C_SCL_GPIO);
}

static inline void i2c_scl_low(void)
{
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << I2C_SCL_GPIO);
}

static inline uint16_t i2c_scl_read(void)
{
    return ( GPIO_REG_READ(GPIO_IN_ADDRESS) & (1 << I2C_SCL_GPIO) );
}

static inline void i2c_scl_input(void)
{
    GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, 1 << I2C_SCL_GPIO);
}

static inline void i2c_scl_output(void)
{
    GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, 1 << I2C_SCL_GPIO);
}

#define i2c_delay() \
do {\
    uint32_t temp1 = 3;\
    asm volatile ("1:\n"\
        "addi.n %0, %0, -1\n"\
        "nop\n"\
        "bnez %0, 1b\n"\
        : "+r" (temp1));\
} while(0)


void IRAM_ATTR i2c_master_init(void)
{
    ETS_GPIO_INTR_DISABLE();

    PIN_FUNC_SELECT(I2C_SDA_MUX, I2C_SDA_FUNC);
    PIN_FUNC_SELECT(I2C_SCL_MUX, I2C_SCL_FUNC);

    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_SDA_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_SDA_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_SDA_GPIO));
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_SCL_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_SCL_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_SCL_GPIO));

    ETS_GPIO_INTR_ENABLE();
}

uint8_t IRAM_ATTR i2c_start(uint8_t address, uint8_t rw)
{
    i2c_sda_high();
    i2c_delay();

    i2c_scl_high(); // TODO: wait for SCL to be released
    i2c_delay();

    i2c_sda_low();
    i2c_delay();

    i2c_scl_low();
    i2c_delay();

    return i2c_write_byte((address << 1) | rw);
}

void IRAM_ATTR i2c_stop(void)
{
    i2c_sda_low();
    i2c_delay();

    i2c_scl_high(); // TODO: wait for SCL to be released
    i2c_delay();

    i2c_sda_high();
    i2c_delay();
}

static void i2c_write_bit(uint8_t bit)
{
    if(bit) {
        i2c_sda_high();
    } else {
        i2c_sda_low();
    }

    i2c_delay();

    i2c_scl_high();
    i2c_delay();
    i2c_delay();

    i2c_scl_low();
    i2c_delay();
}

static uint8_t i2c_read_ack(void)
{
    i2c_sda_input();

    i2c_delay();
    i2c_scl_high();

    uint8_t ack = i2c_sda_read() ? I2C_NACK : I2C_ACK;

    i2c_delay();
    i2c_delay();

    i2c_scl_low();

    i2c_delay();

    i2c_sda_output();
    i2c_sda_high();

    return ack;
}

uint8_t IRAM_ATTR i2c_write_byte(uint8_t data)
{
    for(int i = 0; i < 8; i++)
    {
        i2c_write_bit(data & 0x80);
        data <<= 1;
    }

    return i2c_read_ack();
}

uint16_t IRAM_ATTR i2c_write(const uint8_t *data, uint16_t len)
{
    uint8_t status = I2C_ACK;
    while((len > 0) && status) {
        status = i2c_write_byte(*data++);
        len--;
    }

    return len;
}


uint8_t IRAM_ATTR i2c_write_byte_lsb_first(uint8_t data)
{
    for(int i = 0; i < 8; i++)
    {
        i2c_write_bit(data & 0x01);

        data >>= 1;
    }

    return i2c_read_ack();
}

uint16_t IRAM_ATTR i2c_write_lsb_first(const uint8_t *data, uint16_t len)
{
    uint8_t status = I2C_ACK;
    while((len > 0) && status) {
        status = i2c_write_byte_lsb_first(*data++);
        len--;
    }

    return len;
}
