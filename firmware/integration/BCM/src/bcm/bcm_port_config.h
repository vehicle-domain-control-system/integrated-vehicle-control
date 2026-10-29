#ifndef BCM_PORT_CONFIG_H
#define BCM_PORT_CONFIG_H

/* S32K144 BCM bench (Network 144 BCM project). Only the interior light is real
 * hardware: FTM0 CH0/1/2 -> PTD15/PTD16/PTD0 (Ftm_Pwm_Ip_UserCfg0). Without a door
 * contact or a heatsink ADC the port gives the fixed bench values below; the lock
 * actuator and the fan come from bcm_sim.c. */

#define BCM_PORT_HAS_INTERIOR_LIGHT   1

#define BCM_PORT_HAS_THERMAL_LED      0

#define BCM_PORT_HAS_DOOR_CONTACT     0

#define BCM_PORT_HAS_HEATSINK_ADC     0

#define BCM_PORT_DOOR_OPEN_LEVEL      1u
#define BCM_PORT_DOOR_CONTACT_PIN     12u

#define BCM_PORT_PWM_DUTY_FULL        0x8000u
#define BCM_PORT_FTM_INSTANCE         0u
#define BCM_PORT_CH_LIGHT_RED         0u
#define BCM_PORT_CH_LIGHT_GREEN       1u
#define BCM_PORT_CH_LIGHT_BLUE        2u
#define BCM_PORT_CH_THERMAL_HEAT      3u
#define BCM_PORT_CH_THERMAL_COOL      4u

#define BCM_PORT_ADC_INSTANCE         0u
#define BCM_PORT_ADC_CHANNEL          12u
#define BCM_PORT_ADC_WAIT_POLLS       1000u

/* Bench values without a contact / ADC: door closed, heatsink 25.0 degC
 * (bcm_app.c UpdateInputs: deci-degC = raw * 1000 / 4095; valid 41..4054). */
#define BCM_PORT_SIM_DOOR_OPEN        0u
#define BCM_PORT_SIM_HEATSINK_RAW     1024u

#endif
