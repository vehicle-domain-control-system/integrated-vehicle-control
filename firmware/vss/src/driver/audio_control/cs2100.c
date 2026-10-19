#include "cs2100.h"
#include "../../hal/include/hal_i2c.h"
#include "../../hal/include/hal_timer.h"

#define CS2100_I2C_ADDR               0x4FU

#define CS2100_REG_DEVICE_ID          0x01U
#define CS2100_REG_DEVICE_CTRL        0x02U
#define CS2100_REG_DEVICE_CFG1        0x03U
#define CS2100_REG_GLOBAL_CFG         0x05U
#define CS2100_REG_RATIO_0            0x06U
#define CS2100_REG_RATIO_1            0x07U
#define CS2100_REG_RATIO_2            0x08U
#define CS2100_REG_RATIO_3            0x09U
#define CS2100_REG_FUNC_CFG1          0x16U
#define CS2100_REG_FUNC_CFG2          0x17U

#define CS2100_DEVICE_CTRL_UNLOCK     0x80U

#define CS2100_PLL_LOCK_WAIT_TICKS    500U
#define CS2100_PLL_RECHECK_WAIT_TICKS 40U

typedef struct {
    uint8_t reg;
    uint8_t value;
} Cs2100RegValue;

/* Verified board configuration, written in this order. */
static const Cs2100RegValue s_config[] = {
    { CS2100_REG_FUNC_CFG1,   0x10U },
    { CS2100_REG_FUNC_CFG2,   0x00U },
    { CS2100_REG_RATIO_0,     0x07U },
    { CS2100_REG_RATIO_1,     0x80U },
    { CS2100_REG_RATIO_2,     0x00U },
    { CS2100_REG_RATIO_3,     0x00U },
    { CS2100_REG_DEVICE_CFG1, 0x01U },
    { CS2100_REG_GLOBAL_CFG,  0x01U },
};

static uint32_t s_i2cErrors;

/* Last values read, for the debugger. */
static volatile uint8_t s_deviceId = 0xFFU;
static volatile uint8_t s_deviceCtrlFirst = 0xFFU;
static volatile uint8_t s_deviceCtrl = 0xFFU;

static bool Cs2100_Check(HalI2cStatus status)
{
    if (status != HAL_I2C_OK)
    {
        s_i2cErrors++;
        return false;
    }
    return true;
}

static bool Cs2100_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2];

    tx[0] = reg;
    tx[1] = value;
    return Cs2100_Check(HalI2c_Write(CS2100_I2C_ADDR, tx, 2U, true));
}

/* Register select without STOP, then a one-byte read ending with STOP. */
static bool Cs2100_ReadReg(uint8_t reg, uint8_t *value)
{
    if (!Cs2100_Check(HalI2c_Write(CS2100_I2C_ADDR, &reg, 1U, false)))
    {
        return false;
    }
    return Cs2100_Check(HalI2c_Read(CS2100_I2C_ADDR, value, 1U, true));
}

bool AudioControl_Cs2100Init(void)
{
    uint8_t value = 0xFFU;
    uint32_t i;

    /* The ID read is kept as part of the verified bus sequence; its value
     * does not gate the configuration. */
    if (Cs2100_ReadReg(CS2100_REG_DEVICE_ID, &value))
    {
        s_deviceId = value;
    }

    for (i = 0U; i < (sizeof(s_config) / sizeof(s_config[0])); ++i)
    {
        if (!Cs2100_WriteReg(s_config[i].reg, s_config[i].value))
        {
            return false;
        }
    }

    HalTimer_WaitTicks(CS2100_PLL_LOCK_WAIT_TICKS);
    value = 0xFFU;
    (void)Cs2100_ReadReg(CS2100_REG_DEVICE_CTRL, &value);
    s_deviceCtrlFirst = value;

    HalTimer_WaitTicks(CS2100_PLL_RECHECK_WAIT_TICKS);
    value = 0xFFU;
    if (!Cs2100_ReadReg(CS2100_REG_DEVICE_CTRL, &value))
    {
        return false;
    }
    s_deviceCtrl = value;
    return (value & CS2100_DEVICE_CTRL_UNLOCK) == 0U;
}

uint32_t AudioControl_Cs2100I2cErrors(void)
{
    return s_i2cErrors;
}

#if VSS_CFG_HOST_TEST
void AudioControl_Cs2100ResetForTest(void)
{
    s_i2cErrors = 0U;
}
#endif
