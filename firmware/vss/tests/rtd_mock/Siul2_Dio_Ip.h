#ifndef MOCK_SIUL2_DIO_IP_H
#define MOCK_SIUL2_DIO_IP_H

#include "Std_Types.h"

typedef struct {
    volatile uint16 PGPDO;
    uint16 RESERVED_1[31];
    volatile uint16 PGPDI;
} Siul2_Dio_Ip_GpioType;

typedef uint16 Siul2_Dio_Ip_PinsChannelType;
typedef uint8  Siul2_Dio_Ip_PinsLevelType;

extern Siul2_Dio_Ip_GpioType g_mockGpioPorts[8];
#define PTA_H_HALF (&g_mockGpioPorts[1])
#define PTC_H_HALF (&g_mockGpioPorts[5])

Siul2_Dio_Ip_PinsLevelType Siul2_Dio_Ip_ReadPin(const Siul2_Dio_Ip_GpioType *const base,
                                                Siul2_Dio_Ip_PinsChannelType pin);
void Siul2_Dio_Ip_TogglePins(Siul2_Dio_Ip_GpioType *const base,
                             Siul2_Dio_Ip_PinsChannelType pins);

#endif /* MOCK_SIUL2_DIO_IP_H */
