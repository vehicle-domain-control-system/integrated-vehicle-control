/* Network public entry (not a layer): Net_Init() / Net_MainFunction(nowMs)
 * (network/app/net_if_entry.h) wrap the composition root NetNode_Init() /
 * NetNode_Step() (net_node.c) and add nothing to their meaning. They are the
 * public entry of every node profile (VSS, CIS, ...); the VSS firmware is the
 * first caller and fixes the names and arguments. Net_TxFlush(nowMs) wraps the
 * send half of the pass (NetNode_TxFlush) for a loop that runs the
 * application after Net_MainFunction; Net_GetBootId() reads the node's BOOT_ID.
 *
 * VSS contract (firmware/vss src/net_if/vss_net_api.h, src/app/vss_main.c,
 * src/infra/vss_time.h):
 *   boot  : ... VssTime_Init(HalTimer_GetTicks) ... Flow_Init -> Net_Init()
 *   loop  : Net_MainFunction(VssTime_NowMs32()); VssApp_On* run inside it,
 *           never from an interrupt
 *   clock : received_at_ms is on the VSS clock axis: the CAN ISR records the
 *           raw tick only, the tick is converted in the main context
 *   time  : one pass is short (a few ms, never above 15 ms); the ISR ring
 *           drain is bounded per pass
 *
 * Node configuration: Net_Init() has no argument, so BOOT_ID and the epochs
 * come from the board hook NET_BOARD_NODE_CONFIG_HOOK (net_board_cfg.h). The
 * product BOOT_ID source (NVM / noinit counter) is the integrator's choice.
 *
 * Time: nowMs (application ms, 32-bit wrap) is extended to 64 bits here and
 * handed to the RTD backend as Network now (net_hal_rtd_set_now; used by the
 * application clock time base). */
#include <string.h>
#include "network/net_node.h"
#include "network/app/net_if_entry.h"
#include "net_board_cfg.h"

#ifdef NET_BOARD_NODE_CONFIG_HOOK
/* Board / application owned: fill out->boot_id (nonzero, new on every boot);
 * the epochs arrive as 1 and may be replaced. false = no configuration. */
extern bool NET_BOARD_NODE_CONFIG_HOOK(NetNodeConfig *out);
#endif

typedef struct {
    uint8_t init_result;            /* NetEntryInitResult */
    uint32_t init_calls, main_calls, now_backward, main_max_us;
    bool now_seen;
    uint64_t now_ms;
} NetEntry;

static NetEntry entry;

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

void Net_Init(void)
{
#ifdef NET_BOARD_NODE_CONFIG_HOOK
    NetNodeConfig cfg;
#endif
    inc(&entry.init_calls);
    /* Once per power cycle: a later call never touches the node, whether the
     * first one succeeded or failed (NetNode_Init itself is fresh-storage only). */
    if (entry.init_result != (uint8_t)NET_ENTRY_INIT_NONE) return;
#ifdef NET_BOARD_NODE_CONFIG_HOOK
    memset(&cfg, 0, sizeof(cfg));
    cfg.local_epoch = 1u;           /* NetNodeConfig: any nonzero value, 1 is fine */
    cfg.can_port_epoch = 1u;
    cfg.uart_m_port_epoch = 1u;     /* ignored for a link the node profile lacks */
    cfg.uart_c_port_epoch = 1u;
    if (!NET_BOARD_NODE_CONFIG_HOOK(&cfg)) {
        entry.init_result = (uint8_t)NET_ENTRY_INIT_CONFIG_REFUSED;
        return;
    }
    /* BOOT_ID 0 is reserved: docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md */
    if (cfg.boot_id == 0u) {
        entry.init_result = (uint8_t)NET_ENTRY_INIT_BOOT_ID_ZERO;
        return;
    }
    entry.init_result = (uint8_t)(NetNode_Init(&cfg) == NET_OK ? NET_ENTRY_INIT_OK
                                                                : NET_ENTRY_INIT_NODE_FAILED);
#else
    entry.init_result = (uint8_t)NET_ENTRY_INIT_NO_CONFIG_HOOK;
#endif
}

/* nowMs on the 64-bit axis. The first value is taken as it is; afterwards the
 * modulo-2^32 step is added, so a 32-bit wrap is a small step forward. A value
 * behind the previous one is a clock fault: Network time is held. */
static uint64_t extend_now(uint32_t now_ms)
{
    uint32_t ahead;
    if (!entry.now_seen) {
        entry.now_seen = true;
        entry.now_ms = now_ms;
        return entry.now_ms;
    }
    ahead = now_ms - (uint32_t)entry.now_ms;
    if (ahead > 0x7FFFFFFFu) {
        inc(&entry.now_backward);
        return entry.now_ms;
    }
    entry.now_ms += ahead;
    return entry.now_ms;
}

void Net_MainFunction(uint32_t nowMs)
{
#ifdef NET_BOARD_US_HOOK
    uint32_t t0, took;
#endif
    inc(&entry.main_calls);
    if (entry.init_result != (uint8_t)NET_ENTRY_INIT_OK) return;
#ifdef NET_BOARD_US_HOOK
    t0 = NET_BOARD_US_HOOK();
#endif
    (void)net_hal_rtd_set_now(extend_now(nowMs));
    NetNode_Step();                 /* ISR ring drain first, then the node pass */
#ifdef NET_BOARD_US_HOOK
    took = NET_BOARD_US_HOOK() - t0; /* free-running counter: modulo 2^32 */
    if (took > entry.main_max_us) entry.main_max_us = took;
#endif
}

/* Send-only pass after the application ran: the same nowMs rule as
 * Net_MainFunction, then the send half of the node pass. No receive delivery
 * and no application callback inside it. */
void Net_TxFlush(uint32_t nowMs)
{
    if (entry.init_result != (uint8_t)NET_ENTRY_INIT_OK) return;
    (void)net_hal_rtd_set_now(extend_now(nowMs));
    NetNode_TxFlush();
}

uint32_t Net_GetBootId(void)
{
    return entry.init_result == (uint8_t)NET_ENTRY_INIT_OK ? NetNode_GetBootId() : 0u;
}

void net_entry_diag(NetNodeDiag *out)
{
    if (out == NULL) return;
    out->entry_init_result = entry.init_result;
    out->entry_init_calls = entry.init_calls;
    out->entry_main_calls = entry.main_calls;
    out->entry_now_backward = entry.now_backward;
    out->entry_now_ms = entry.now_ms;
    out->main_max_us = entry.main_max_us;
}
