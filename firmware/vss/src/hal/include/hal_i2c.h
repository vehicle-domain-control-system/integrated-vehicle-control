#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HAL_I2C_OK = 0,
    HAL_I2C_ERROR
} HalI2cStatus;

/* Blocking transfers on the board's single I2C master (1000 ms timeout).
 * addr7 is the 7-bit slave address. sendStop selects a STOP condition after
 * the last byte; false keeps the bus for a repeated start. */
HalI2cStatus HalI2c_Write(uint8_t addr7, const uint8_t *data, uint32_t len, bool sendStop);
HalI2cStatus HalI2c_Read(uint8_t addr7, uint8_t *data, uint32_t len, bool sendStop);

#endif /* HAL_I2C_H */
