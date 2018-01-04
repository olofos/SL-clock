#include <stdint.h>
#include <esp_common.h>

#include "i2c-master.h"
#include "i2c_master.h"

void i2c_master_init(void)
{
    i2c_master_gpio_init();
}

uint8_t i2c_start(uint8_t address, uint8_t rw)
{
    i2c_master_start();
    i2c_master_writeByte(address << 1 | rw);

    if(i2c_master_checkAck())
    {
        return I2C_OK;
    } else {
        i2c_master_stop();
        return I2C_ERROR;
    }
}

uint8_t i2c_start_wait(uint8_t address, uint8_t rw)
{
    for(;;)
    {
        i2c_master_start();
        i2c_master_writeByte(address << 1 | rw);
        if(i2c_master_checkAck())
        {
            return I2C_OK;
        }
//        taskYIELD();
    }
}

void i2c_stop(void)
{
    i2c_master_stop();
}

uint8_t i2c_write_byte(uint8_t data)
{
    i2c_master_writeByte(data);

    if(i2c_master_checkAck())
    {
        return I2C_OK;
    } else {
        return I2C_DONE;
    }
}

uint16_t i2c_write(const uint8_t *data, uint16_t len)
{
    uint8_t status = I2C_OK;

    while((len > 0) && (status == I2C_OK))
    {
        status = i2c_write_byte(*data++);
        len--;
    }

    return len;
}

uint16_t i2c_write_P(const uint8_t *data, uint16_t len)
{
    return i2c_write(data, len);
}

uint8_t i2c_read_ack(void)
{
    uint8_t ret = i2c_master_readByte();
    i2c_master_send_ack();
    return ret;
}

uint8_t i2c_read_nak(void)
{
    uint8_t ret = i2c_master_readByte();
    i2c_master_send_nack();
    return ret;
}
