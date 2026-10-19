#ifndef NET_STUB_H
#define NET_STUB_H

/* Stand-in for the Network module (VSS_CFG_NET_STUB=1). Net_Init and
 * Net_MainFunction do nothing; VssNet_PublishStatus accepts every non-NULL
 * report and keeps a copy of the last one for the debugger and the tests.
 * Not part of the Network interface. */

#include <stdbool.h>
#include <stdint.h>

#include "vss_net_api.h"

/* True and the last accepted report once one has been published. */
bool NetStub_ReadLastStatus(NetVssStatus *out);

/* Number of reports accepted since start. */
uint32_t NetStub_PublishCount(void);

#endif /* NET_STUB_H */
