#include "cis_node.h"

#ifdef NET_NODE_BUILD
#include "net_board_time144.h"
#include "timebase.h"
#include "cis_app.h"
#include "cis_net_adapter.h"

bool CisNode_Init(void)
{
    bool ok = timebase_init();          /* FTM0 1 MHz counter for the sensor drivers */

    cis_app_init();                     /* sensors */
    cis_net_adapter_init();             /* calls Net_Init() */
    return ok;
}

void CisNode_Service(void)
{
    uint32_t now_ms = Board_NowMs32();

    cis_net_adapter_poll(now_ms);       /* Net_MainFunction(): receive callbacks run in here */
    cis_app_run(now_ms);
    cis_net_adapter_run(now_ms);
}
#else
typedef int cis_node_absent;            /* not the Network project: nothing is compiled */
#endif
