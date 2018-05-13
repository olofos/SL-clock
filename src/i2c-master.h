#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_

#define I2C_SDA_MUX PERIPHS_IO_MUX_MTDI_U
#define I2C_SCL_MUX PERIPHS_IO_MUX_MTCK_U
#define I2C_SDA_GPIO 12
#define I2C_SCL_GPIO 13
#define I2C_SDA_FUNC FUNC_GPIO12
#define I2C_SCL_FUNC FUNC_GPIO13

#define I2C_ACK 1
#define I2C_NACK 0

#define I2C_WRITE 0

void i2c_master_init(void);

uint8_t i2c_start(uint8_t address, uint8_t rw);
void i2c_stop(void);

uint8_t i2c_write_byte(uint8_t data);
uint16_t i2c_write(const uint8_t *data, uint16_t len);

#endif
