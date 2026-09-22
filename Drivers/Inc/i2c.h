#ifndef __I2C_H__
#define __I2C_H__

#include "stm32f4xx.h"

#define I2C_OK      0
#define I2C_ERROR   1

/** I2C1 master at 100kHz standard mode on PB8 (SCL) / PB9 (SDA). */
void i2c_init(void);

/** Blocking write of len bytes to a 7-bit address. Returns I2C_OK or I2C_ERROR. */
uint8_t i2c_write(uint8_t addr, const uint8_t *data, uint32_t len);

/** Blocking read of len bytes from a 7-bit address. */
uint8_t i2c_read(uint8_t addr, uint8_t *data, uint32_t len);

/** Write a register address then read len bytes back (repeated start). */
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len);

#endif
