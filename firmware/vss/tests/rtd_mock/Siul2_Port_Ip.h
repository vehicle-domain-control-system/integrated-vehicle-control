#ifndef MOCK_SIUL2_PORT_IP_H
#define MOCK_SIUL2_PORT_IP_H

#include "Std_Types.h"
#include "Siul2_Dio_Ip.h"

typedef enum {
    SIUL2_PORT_SUCCESS = E_OK,
    SIUL2_PORT_ERROR   = E_NOT_OK
} Siul2_Port_Ip_PortStatusType;

typedef struct { uint8 mock; } Siul2_Port_Ip_PinSettingsConfig;

/* Generated pin table and the board's reference-clock pin (PTA26, bit 10 of
 * the upper half-port). */
#define NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0 9
extern const Siul2_Port_Ip_PinSettingsConfig
    g_pin_mux_InitConfigArr_PortContainer_0_VS_0[NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0];
#define CS2100_REF_PIN  10u
#define CS2100_REF_PORT PTA_H_HALF

Siul2_Port_Ip_PortStatusType Siul2_Port_Ip_Init(uint32 pinCount,
                                                const Siul2_Port_Ip_PinSettingsConfig configArray[]);

#endif /* MOCK_SIUL2_PORT_IP_H */
