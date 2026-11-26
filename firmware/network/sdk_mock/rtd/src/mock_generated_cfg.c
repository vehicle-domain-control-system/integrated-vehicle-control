/* Host mock of the ConfigTools-generated FlexCAN_Ip_Sa_PBcfg.c of
 * network_344_can_bringup: same symbol names, same callback names,
 * max_num_mb 16, FD + BRS. The callbacks are defined by the Network HAL
 * (NET_BOARD_CAN_CALLBACK / NET_BOARD_CAN_ERROR_CALLBACK).
 * Independently written; NOT the NXP file. */
#include "FlexCAN_Ip_Sa_PBcfg.h"

extern void Can0_BringupCallback(uint8 instance, Flexcan_Ip_EventType eventType,
                                 uint32 buffIdx, const Flexcan_Ip_StateType * flexcanState);
extern void Can0_BringupErrorCallback(uint8 instance, Flexcan_Ip_EventType eventType,
                                      uint32 u32ErrStatus, const Flexcan_Ip_StateType * flexcanState);

Flexcan_Ip_StateType FlexCAN_State0;

const Flexcan_Ip_ConfigType FlexCAN_Config0 = {
    16U,                         /* max_num_mb */
    TRUE,                        /* fd_enable */
    TRUE,                        /* bitRateSwitch */
    &Can0_BringupCallback,
    &Can0_BringupErrorCallback
};
