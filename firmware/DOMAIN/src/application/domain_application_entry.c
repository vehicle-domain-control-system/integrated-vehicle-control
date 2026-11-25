#include "domain_application_entry.h"

static DomainBoardRuntime_t g_domainBoardRuntime;
static bool g_initAttempted;
static bool g_ready;
static bool g_faultBlocked;
static DomainApplicationEntry_Status_t g_lastStatus =
    DOMAIN_APPLICATION_ENTRY_NOT_INITIALIZED;
static DomainBoardRuntime_Status_t g_lastBoardStatus =
    DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED;

static DomainApplicationEntry_Status_t DomainApplicationEntry_Block(
    DomainApplicationEntry_Status_t status)
{
    g_ready = false;
    g_faultBlocked = true;
    g_lastStatus = status;
    return status;
}

DomainApplicationEntry_Status_t DomainApplicationEntry_Init(
    const DomainBoardRuntime_Config_t *config)
{
    if (config == NULL)
    {
        return DOMAIN_APPLICATION_ENTRY_INVALID_ARGUMENT;
    }
    if (g_initAttempted)
    {
        return g_ready ? DOMAIN_APPLICATION_ENTRY_ALREADY_INITIALIZED :
                         DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED;
    }

    g_initAttempted = true;
    g_lastBoardStatus = DomainBoardRuntime_Init(
        &g_domainBoardRuntime,
        config);
    if ((g_lastBoardStatus != DOMAIN_BOARD_RUNTIME_OK) ||
        !DomainBoardRuntime_IsReady(&g_domainBoardRuntime))
    {
        return DomainApplicationEntry_Block(
            DOMAIN_APPLICATION_ENTRY_BOARD_INIT_FAILED);
    }

    g_ready = true;
    g_lastStatus = DOMAIN_APPLICATION_ENTRY_OK;
    return DOMAIN_APPLICATION_ENTRY_OK;
}

DomainApplicationEntry_Status_t DomainApplicationEntry_Poll(void)
{
    if (!g_initAttempted)
    {
        return DOMAIN_APPLICATION_ENTRY_NOT_INITIALIZED;
    }
    if (!g_ready || g_faultBlocked)
    {
        return DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED;
    }

    g_lastBoardStatus = DomainBoardRuntime_Poll(&g_domainBoardRuntime);
    if (g_lastBoardStatus != DOMAIN_BOARD_RUNTIME_OK)
    {
        return DomainApplicationEntry_Block(
            DOMAIN_APPLICATION_ENTRY_POLL_FAILED);
    }

    g_lastStatus = DOMAIN_APPLICATION_ENTRY_OK;
    return DOMAIN_APPLICATION_ENTRY_OK;
}

bool DomainApplicationEntry_IsReady(void)
{
    return g_initAttempted && g_ready && !g_faultBlocked &&
           DomainBoardRuntime_IsReady(&g_domainBoardRuntime);
}

bool DomainApplicationEntry_GetNowMs(uint32_t *outNowMs)
{
    uint32_t nowMs;

    if ((outNowMs == NULL) || !DomainApplicationEntry_IsReady())
    {
        return false;
    }
    if (!DomainBoardTimebase_GetNowMs(
            &g_domainBoardRuntime.timebase,
            &nowMs))
    {
        return false;
    }

    *outNowMs = nowMs;
    return true;
}

DomainRuntime_t *DomainApplicationEntry_GetRuntime(void)
{
    if (!DomainApplicationEntry_IsReady())
    {
        return NULL;
    }

    return DomainBoardRuntime_GetDomainRuntime(&g_domainBoardRuntime);
}

DomainApplicationEntry_Status_t DomainApplicationEntry_GetSnapshot(
    DomainApplicationEntry_Snapshot_t *outSnapshot)
{
    DomainApplicationEntry_Snapshot_t snapshot;

    if (outSnapshot == NULL)
    {
        return DOMAIN_APPLICATION_ENTRY_INVALID_ARGUMENT;
    }
    if (!g_initAttempted)
    {
        return DOMAIN_APPLICATION_ENTRY_NOT_INITIALIZED;
    }

    snapshot.initAttempted = g_initAttempted;
    snapshot.ready = DomainApplicationEntry_IsReady();
    snapshot.faultBlocked = g_faultBlocked;
    snapshot.lastStatus = g_lastStatus;
    snapshot.lastBoardStatus = g_lastBoardStatus;
    *outSnapshot = snapshot;
    return DOMAIN_APPLICATION_ENTRY_OK;
}
