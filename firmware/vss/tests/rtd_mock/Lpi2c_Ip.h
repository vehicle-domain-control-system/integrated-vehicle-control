#ifndef MOCK_LPI2C_IP_H
#define MOCK_LPI2C_IP_H

#include "Std_Types.h"

typedef enum {
    LPI2C_IP_SUCCESS_STATUS          = 0x000U,
    LPI2C_IP_ERROR_STATUS            = 0x001U,
    LPI2C_IP_BUSY_STATUS             = 0x002U,
    LPI2C_IP_RECEIVED_NACK_STATUS    = 0x003U,
    LPI2C_IP_TX_UNDERRUN_STATUS      = 0x004U,
    LPI2C_IP_RX_OVERRUN_STATUS       = 0x005U,
    LPI2C_IP_ARBITRATION_LOST_STATUS = 0x006U,
    LPI2C_IP_ABORTED_STATUS          = 0x007U,
    LPI2C_IP_BUS_BUSY_STATUS         = 0x008U,
    LPI2C_IP_TIMEOUT_STATUS          = 0x009U,
    LPI2C_IP_UNSUPPORTED_STATUS      = 0x00AU,
    LPI2C_IP_DMA_ERROR_STATUS        = 0x00BU
} Lpi2c_Ip_StatusType;

typedef struct Lpi2c_Ip_MasterConfigType Lpi2c_Ip_MasterConfigType;

extern const Lpi2c_Ip_MasterConfigType I2c_Lpi2cMaster_HwChannel0_Channel0_VS_0;

void Lpi2c_Ip_MasterInit(uint8 Instance, const Lpi2c_Ip_MasterConfigType *ConfigPtr);
void Lpi2c_Ip_MasterSetSlaveAddr(uint8 Instance, const uint16 Address, const boolean Is10bitAddr);
Lpi2c_Ip_StatusType Lpi2c_Ip_MasterSendDataBlocking(uint8 Instance, uint8 *TxBuff, uint32 TxSize,
                                                    boolean SendStop, uint32 Timeout);
Lpi2c_Ip_StatusType Lpi2c_Ip_MasterReceiveDataBlocking(uint8 Instance, uint8 *RxBuff, uint32 RxSize,
                                                       boolean SendStop, uint32 Timeout);

#endif /* MOCK_LPI2C_IP_H */
