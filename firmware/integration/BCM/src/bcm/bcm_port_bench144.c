#include "bcm_port_bench144.h"
#include "bcm_port_config.h"
#include "port/bcm_sim.h"
#include "bcm/bcm_port.h"
#include "net_board_time144.h"
#include "network/app/net_if_entry.h"

#include "Ftm_Pwm_Ip.h"

#if BCM_PORT_HAS_DOOR_CONTACT
#include "S32K144_GPIO.h"
#endif
#if BCM_PORT_HAS_HEATSINK_ADC
#include "Adc_Ip.h"
#endif

#define PCT_MAX  100u

static bool s_initOk;

static uint16 PctToDuty(uint8_t pct)
{
    uint32_t p = (pct > PCT_MAX) ? PCT_MAX : pct;
    return (uint16)((p * BCM_PORT_PWM_DUTY_FULL) / PCT_MAX);
}

static void SetDuty(uint8 channel, uint8_t pct)
{
    Ftm_Pwm_Ip_UpdatePwmDutyCycleChannel(BCM_PORT_FTM_INSTANCE, channel, PctToDuty(pct), TRUE);
}

/* The Network BOOT_ID (Board_NetNodeConfig); no boot counter on the bench. */
uint32_t BcmPort_BootId(void)
{
    return Net_GetBootId();
}

bool BcmPort_InitOk(void)
{
    return s_initOk;
}

void BcmPortBench_Init(void)
{
    BcmSim_Init();
    BcmPort_Thermal(NET_THERMAL_IDLE, 0u);
    (void)BcmPort_LightApply(0u, 0u, 0u);
#if BCM_PORT_HAS_HEATSINK_ADC
    (void)Adc_Ip_DoCalibration(BCM_PORT_ADC_INSTANCE);
#endif
    s_initOk = true;
}

bool BcmPort_DoorContact(bool *isOpen)
{
#if BCM_PORT_HAS_DOOR_CONTACT
    uint32_t level = (IP_PTC->PDIR >> BCM_PORT_DOOR_CONTACT_PIN) & 1u;
    *isOpen = (level == BCM_PORT_DOOR_OPEN_LEVEL);
    return true;
#else
    *isOpen = (BCM_PORT_SIM_DOOR_OPEN != 0u);
    return true;
#endif
}

NetDoorLockState BcmPort_LockState(void)
{
    return BcmSim_LockState();
}

void BcmPort_DoorDrive(bool active, NetDoorTarget target)
{
    BcmSim_DoorDrive(active, target, Board_NowMs32());
}

void BcmPort_FanCommand(NetFanLevel level)
{
    BcmSim_FanCommand(level, Board_NowMs32());
}

NetFanLevel BcmPort_FanMeasured(void)
{
    return BcmSim_FanMeasured(Board_NowMs32());
}

void BcmPort_Thermal(NetThermalDirection direction, uint8_t outputPct)
{
#if BCM_PORT_HAS_THERMAL_LED
    SetDuty(BCM_PORT_CH_THERMAL_HEAT, (direction == NET_THERMAL_HEAT) ? outputPct : 0u);
    SetDuty(BCM_PORT_CH_THERMAL_COOL, (direction == NET_THERMAL_COOL) ? outputPct : 0u);
#else
    (void)direction;
    (void)outputPct;
#endif
}

bool BcmPort_HeatsinkRaw(uint16_t *raw)
{
#if BCM_PORT_HAS_HEATSINK_ADC
    uint32_t polls;
    Adc_Ip_StartConversion(BCM_PORT_ADC_INSTANCE, BCM_PORT_ADC_CHANNEL, FALSE);
    for (polls = 0u; polls < BCM_PORT_ADC_WAIT_POLLS; polls++)
    {
        if (Adc_Ip_GetConvCompleteFlag(BCM_PORT_ADC_INSTANCE, 0u))
        {
            *raw = Adc_Ip_GetConvData(BCM_PORT_ADC_INSTANCE, 0u);
            return true;
        }
    }
    return false;
#else
    *raw = BCM_PORT_SIM_HEATSINK_RAW;
    return true;
#endif
}

bool BcmPort_LightApply(uint8_t redPct, uint8_t greenPct, uint8_t bluePct)
{
#if BCM_PORT_HAS_INTERIOR_LIGHT
    SetDuty(BCM_PORT_CH_LIGHT_RED, redPct);
    SetDuty(BCM_PORT_CH_LIGHT_GREEN, greenPct);
    SetDuty(BCM_PORT_CH_LIGHT_BLUE, bluePct);
    return true;
#else
    (void)redPct;
    (void)greenPct;
    (void)bluePct;
    return false;
#endif
}
