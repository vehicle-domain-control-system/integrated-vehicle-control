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

#if defined(VSS_CFG_HOST_TEST) && (VSS_CFG_HOST_TEST != 0)
/* Host tests only: the next count submissions return result (not OK) and
 * are not kept. */
void NetStub_RefuseForTest(NetSubmitResult result, uint32_t count);

/* Every VssNet_PublishStatus call, refused ones included. */
uint32_t NetStub_AttemptCount(void);

void NetStub_ResetForTest(void);
#endif

#endif /* NET_STUB_H */
