#ifndef BCM_RESULT_H
#define BCM_RESULT_H

#include "bcm_types.h"

NetBcmResult BcmResult_Make(NetBcmCommandKind kind, BcmCommandRef command, NetVehicleResult result, NetReason reason);

BcmCommandRef BcmResult_RefOf(const NetCommandMeta *command, const NetCanRxMeta *meta);

#endif
