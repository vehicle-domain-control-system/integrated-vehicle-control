#include "../include/hal_i2c.h"
#include "hal_s32k344_cfg.h"

#define HAL_I2C_TIMEOUT       1000U
#define HAL_I2C_MAX_WRITE_LEN 4U

/* The vendor send call takes a non-const buffer, so writes go through this
 * copy. Every register write on the board is at most 4 bytes. */
static uint8 s_txBuffer[HAL_I2C_MAX_WRITE_LEN];

HalI2cStatus HalI2c_Write(uint8_t addr7, const uint8_t *data, uint32_t len, bool sendStop)
{
    Lpi2c_Ip_StatusType status;
    uint32_t i;

    if ((data == NULL_PTR) || (len == 0U) || (len > HAL_I2C_MAX_WRITE_LEN))
    {
        return HAL_I2C_ERROR;
    }
    for (i = 0U; i < len; ++i)
    {
        s_txBuffer[i] = data[i];
    }

    Lpi2c_Ip_MasterSetSlaveAddr(HAL_CFG_LPI2C_INSTANCE, addr7, FALSE);
    status = Lpi2c_Ip_MasterSendDataBlocking(HAL_CFG_LPI2C_INSTANCE, s_txBuffer, len,
                                             sendStop ? TRUE : FALSE, HAL_I2C_TIMEOUT);
    return (status == LPI2C_IP_SUCCESS_STATUS) ? HAL_I2C_OK : HAL_I2C_ERROR;
}

HalI2cStatus HalI2c_Read(uint8_t addr7, uint8_t *data, uint32_t len, bool sendStop)
{
    Lpi2c_Ip_StatusType status;

    if ((data == NULL_PTR) || (len == 0U))
    {
        return HAL_I2C_ERROR;
    }

    /* Setting the address only updates the driver state; it does not touch
     * the bus, so repeating it between a register-select write and this
     * read keeps the bus sequence unchanged. */
    Lpi2c_Ip_MasterSetSlaveAddr(HAL_CFG_LPI2C_INSTANCE, addr7, FALSE);
    status = Lpi2c_Ip_MasterReceiveDataBlocking(HAL_CFG_LPI2C_INSTANCE, data, len,
                                                sendStop ? TRUE : FALSE, HAL_I2C_TIMEOUT);
    return (status == LPI2C_IP_SUCCESS_STATUS) ? HAL_I2C_OK : HAL_I2C_ERROR;
}
