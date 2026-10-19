#include "sgtl5000.h"
#include "../../hal/include/hal_i2c.h"
#include "../../hal/include/hal_timer.h"

#define SGTL5000_I2C_ADDR          0x0AU

#define SGTL5000_REG_CHIP_ID       0x0000U
#define SGTL5000_REG_DIG_POWER     0x0002U
#define SGTL5000_REG_CLK_CTRL      0x0004U
#define SGTL5000_REG_I2S_CTRL      0x0006U
#define SGTL5000_REG_SSS_CTRL      0x000AU
#define SGTL5000_REG_ADCDAC_CTRL   0x000EU
#define SGTL5000_REG_DAC_VOL       0x0010U
#define SGTL5000_REG_ANA_HP_CTRL   0x0022U
#define SGTL5000_REG_ANA_CTRL      0x0024U
#define SGTL5000_REG_ANA_POWER     0x0030U

#define SGTL5000_EXPECTED_PART_ID  0xA0U

#define SGTL5000_CLK_CTRL_VALUE    0x0008U
#define SGTL5000_I2S_CTRL_VALUE    0x0130U
#define SGTL5000_SSS_CTRL_VALUE    0x0010U
#define SGTL5000_DAC_VOL_VALUE     0x3C3CU
#define SGTL5000_HP_VOL_VALUE      0x5050U
#define SGTL5000_DIG_POWER_MASK    0x0021U
#define SGTL5000_ANA_POWER_MASK    0x40B8U

#define SGTL5000_DAC_MUTE_MASK     0x000CU  /* ADCDAC_CTRL: DAC mute left/right */
#define SGTL5000_ANA_CTRL_ROUTE    0x0070U  /* ANA_CTRL bits rewritten at configure */
#define SGTL5000_ANA_CTRL_ROUTE_ON 0x0030U
#define SGTL5000_HP_MUTE_MASK      0x0010U  /* ANA_CTRL: headphone mute */

/* Headphone level: 1 % maps to code 0x7F (lowest) and 100 % to 0x30
 * (-12 dB); 0x18 would be 0 dB, so full scale is capped. 0 % = muted. */
#define SGTL5000_HP_CODE_MIN       0x7FU
#define SGTL5000_HP_CODE_MAX       0x30U
#define SGTL5000_VOLUME_PERCENT_MAX 100U

/* Power-up settling between analog power and the output levels. */
#define SGTL5000_CONFIG_WAIT_TICKS 320U

static uint32_t s_i2cErrors;
static bool s_configured;
/* Set only by the safety mute, never cleared before reset. */
static bool s_safetyMuteLatched;

/* Last values, for the debugger. */
static volatile uint16_t s_chipId = 0xFFFFU;
static volatile uint16_t s_hpRegister = 0x7F7FU;

static bool Sgtl5000_Check(HalI2cStatus status)
{
    if (status != HAL_I2C_OK)
    {
        s_i2cErrors++;
        return false;
    }
    return true;
}

static bool Sgtl5000_WriteReg(uint16_t reg, uint16_t value)
{
    uint8_t tx[4];

    tx[0] = (uint8_t)((reg >> 8U) & 0xFFU);
    tx[1] = (uint8_t)(reg & 0xFFU);
    tx[2] = (uint8_t)((value >> 8U) & 0xFFU);
    tx[3] = (uint8_t)(value & 0xFFU);
    return Sgtl5000_Check(HalI2c_Write(SGTL5000_I2C_ADDR, tx, 4U, true));
}

/* Register address with STOP, then a two-byte read with STOP. */
static bool Sgtl5000_ReadReg(uint16_t reg, uint16_t *value)
{
    uint8_t addr[2];
    uint8_t rx[2];

    addr[0] = (uint8_t)((reg >> 8U) & 0xFFU);
    addr[1] = (uint8_t)(reg & 0xFFU);
    if (!Sgtl5000_Check(HalI2c_Write(SGTL5000_I2C_ADDR, addr, 2U, true)))
    {
        return false;
    }
    if (!Sgtl5000_Check(HalI2c_Read(SGTL5000_I2C_ADDR, rx, 2U, true)))
    {
        return false;
    }
    *value = (uint16_t)(((uint16_t)rx[0] << 8U) | (uint16_t)rx[1]);
    return true;
}

static bool Sgtl5000_ModifyReg(uint16_t reg, uint16_t clearMask, uint16_t setMask)
{
    uint16_t value;

    if (!Sgtl5000_ReadReg(reg, &value))
    {
        return false;
    }
    value = (uint16_t)((value & (uint16_t)(~clearMask)) | setMask);
    return Sgtl5000_WriteReg(reg, value);
}

static uint8_t Sgtl5000_PercentToHpCode(uint8_t percent)
{
    uint32_t span;
    uint32_t step;

    if (percent == 0U)
    {
        return SGTL5000_HP_CODE_MIN;
    }
    if (percent >= SGTL5000_VOLUME_PERCENT_MAX)
    {
        return SGTL5000_HP_CODE_MAX;
    }
    span = (uint32_t)SGTL5000_HP_CODE_MIN - (uint32_t)SGTL5000_HP_CODE_MAX;
    step = (((uint32_t)(percent - 1U) * span) + 49U) / 99U;
    return (uint8_t)((uint32_t)SGTL5000_HP_CODE_MIN - step);
}

static bool Sgtl5000_ApplyHeadphoneVolume(uint8_t percent)
{
    uint8_t code;
    uint16_t hpReg;

    if (percent > SGTL5000_VOLUME_PERCENT_MAX)
    {
        percent = SGTL5000_VOLUME_PERCENT_MAX;
    }
    code = Sgtl5000_PercentToHpCode(percent);
    hpReg = (uint16_t)(((uint16_t)code << 8U) | (uint16_t)code);
    s_hpRegister = hpReg;

    if (percent == 0U)
    {
        /* Mute first, then drop the level. */
        if (!Sgtl5000_ModifyReg(SGTL5000_REG_ANA_CTRL, SGTL5000_HP_MUTE_MASK, SGTL5000_HP_MUTE_MASK))
        {
            return false;
        }
        return Sgtl5000_WriteReg(SGTL5000_REG_ANA_HP_CTRL, hpReg);
    }

    /* Set the level, then unmute. */
    if (!Sgtl5000_WriteReg(SGTL5000_REG_ANA_HP_CTRL, hpReg))
    {
        return false;
    }
    return Sgtl5000_ModifyReg(SGTL5000_REG_ANA_CTRL, SGTL5000_HP_MUTE_MASK, 0x0000U);
}

bool AudioControl_Sgtl5000Probe(void)
{
    uint16_t chipId = 0U;

    if (!Sgtl5000_ReadReg(SGTL5000_REG_CHIP_ID, &chipId))
    {
        return false;
    }
    s_chipId = chipId;
    return (uint8_t)((chipId >> 8U) & 0xFFU) == SGTL5000_EXPECTED_PART_ID;
}

bool AudioControl_Sgtl5000Configure(void)
{
    s_configured = false;

    if (!Sgtl5000_WriteReg(SGTL5000_REG_CLK_CTRL, SGTL5000_CLK_CTRL_VALUE) ||
        !Sgtl5000_WriteReg(SGTL5000_REG_I2S_CTRL, SGTL5000_I2S_CTRL_VALUE) ||
        !Sgtl5000_WriteReg(SGTL5000_REG_SSS_CTRL, SGTL5000_SSS_CTRL_VALUE) ||
        !Sgtl5000_ModifyReg(SGTL5000_REG_DIG_POWER, 0x0000U, SGTL5000_DIG_POWER_MASK) ||
        !Sgtl5000_ModifyReg(SGTL5000_REG_ANA_POWER, 0x0000U, SGTL5000_ANA_POWER_MASK))
    {
        return false;
    }

    HalTimer_WaitTicks(SGTL5000_CONFIG_WAIT_TICKS);

    if (!Sgtl5000_WriteReg(SGTL5000_REG_DAC_VOL, SGTL5000_DAC_VOL_VALUE) ||
        !Sgtl5000_WriteReg(SGTL5000_REG_ANA_HP_CTRL, SGTL5000_HP_VOL_VALUE) ||
        !Sgtl5000_ModifyReg(SGTL5000_REG_ADCDAC_CTRL, SGTL5000_DAC_MUTE_MASK, SGTL5000_DAC_MUTE_MASK) ||
        !Sgtl5000_ModifyReg(SGTL5000_REG_ANA_CTRL, SGTL5000_ANA_CTRL_ROUTE, SGTL5000_ANA_CTRL_ROUTE_ON))
    {
        return false;
    }

    s_configured = true;
    return true;
}

bool AudioControl_Sgtl5000StartAudio(uint8_t volumePercent)
{
    if (s_safetyMuteLatched)
    {
        return false;
    }
    if (!Sgtl5000_ModifyReg(SGTL5000_REG_ADCDAC_CTRL, SGTL5000_DAC_MUTE_MASK, 0x0000U))
    {
        return false;
    }
    return Sgtl5000_ApplyHeadphoneVolume(volumePercent);
}

bool AudioControl_Sgtl5000LatchMute(void)
{
    /* Latched before any slow I2C access, so a failed transfer still blocks
     * every later unmute. */
    s_safetyMuteLatched = true;
    if (!s_configured)
    {
        return false;
    }
    return Sgtl5000_ApplyHeadphoneVolume(0U);
}

uint32_t AudioControl_Sgtl5000I2cErrors(void)
{
    return s_i2cErrors;
}
