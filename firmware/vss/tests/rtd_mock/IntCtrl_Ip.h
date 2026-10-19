#ifndef MOCK_INTCTRL_IP_H
#define MOCK_INTCTRL_IP_H

#include "Std_Types.h"

typedef enum {
    INTCTRL_IP_STATUS_SUCCESS = 0U,
    INTCTRL_IP_STATUS_ERROR   = 1U
} IntCtrl_Ip_StatusType;

typedef struct IntCtrl_Ip_CtrlConfigType IntCtrl_Ip_CtrlConfigType;

extern const IntCtrl_Ip_CtrlConfigType intCtrlConfig;

IntCtrl_Ip_StatusType IntCtrl_Ip_Init(const IntCtrl_Ip_CtrlConfigType *pIntCtrlCtrlConfig);

#endif /* MOCK_INTCTRL_IP_H */
