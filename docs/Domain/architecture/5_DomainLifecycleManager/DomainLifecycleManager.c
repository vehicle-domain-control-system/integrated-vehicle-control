/**
 * @file DomainLifecycleManager.c
 */

#include "DomainLifecycleManager.h"

#include <string.h>

typedef struct
{
    bool initialized;
    DomainLifecycleConfig_t config;
    DomainLifecycleSnapshot_t snapshot;
} DomainLifecycleContext_t;

static DomainLifecycleContext_t g_dlm;

static uint32_t Dlm_ElapsedMs(
    uint32_t nowMs,
    uint32_t sinceMs)
{
    return (uint32_t)(nowMs - sinceMs);
}

static bool Dlm_IsSingleDefinedItem(
    DomainLifecycleSyncMask_t item)
{
    if ((item == 0U) ||
        ((item & ~DLM_SYNC_ALL_DEFINED) != 0U))
    {
        return false;
    }

    /* exactly one bit */
    return ((item & (item - 1U)) == 0U);
}

static void Dlm_Transition(
    DomainLifecycleState_t next,
    uint32_t nowMs)
{
    if (g_dlm.snapshot.state != next)
    {
        g_dlm.snapshot.state = next;
        g_dlm.snapshot.lastTransitionAtMs = nowMs;
    }
}

static void Dlm_Reevaluate(
    uint32_t nowMs)
{
    DomainLifecycleSyncMask_t required;
    DomainLifecycleSyncMask_t unresolved;
    DomainLifecycleSyncMask_t unhealthy;

    if (!g_dlm.snapshot.platformReady)
    {
        Dlm_Transition(DOMAIN_LIFECYCLE_STARTUP, nowMs);
        return;
    }

    required = g_dlm.snapshot.requiredMask;
    unresolved = required & ~g_dlm.snapshot.resolvedMask;
    unhealthy = required & ~g_dlm.snapshot.healthyMask;

    if (unresolved == 0U)
    {
        if (unhealthy == 0U)
        {
            g_dlm.snapshot.syncTimedOut = false;
            Dlm_Transition(DOMAIN_LIFECYCLE_READY, nowMs);
        }
        else
        {
            Dlm_Transition(DOMAIN_LIFECYCLE_DEGRADED, nowMs);
        }

        return;
    }

    if (g_dlm.snapshot.syncTimedOut)
    {
        Dlm_Transition(DOMAIN_LIFECYCLE_DEGRADED, nowMs);
    }
    else
    {
        Dlm_Transition(DOMAIN_LIFECYCLE_SYNCING, nowMs);
    }
}

DomainLifecycleStatus_t DomainLifecycle_Init(
    const DomainLifecycleConfig_t *config,
    uint32_t nowMs)
{
    if (config == NULL)
    {
        return DLM_STATUS_INVALID_ARGUMENT;
    }

    if ((config->requiredInitialSyncMask & ~DLM_SYNC_ALL_DEFINED) != 0U)
    {
        return DLM_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(&g_dlm, 0, sizeof(g_dlm));

    g_dlm.config = *config;

    g_dlm.snapshot.state = DOMAIN_LIFECYCLE_STARTUP;
    g_dlm.snapshot.requiredMask = config->requiredInitialSyncMask;
    g_dlm.snapshot.lastTransitionAtMs = nowMs;
    g_dlm.snapshot.syncStartedAtMs = nowMs;
    g_dlm.snapshot.generation = 1U;

    g_dlm.initialized = true;
    return DLM_STATUS_OK;
}

bool DomainLifecycle_IsInitialized(void)
{
    return g_dlm.initialized;
}

DomainLifecycleStatus_t DomainLifecycle_SetPlatformReady(
    bool ready,
    uint32_t nowMs)
{
    if (!g_dlm.initialized)
    {
        return DLM_STATUS_NOT_INITIALIZED;
    }

    if (!ready)
    {
        g_dlm.snapshot.platformReady = false;
        g_dlm.snapshot.syncTimedOut = false;
        g_dlm.snapshot.resolvedMask = 0U;
        g_dlm.snapshot.healthyMask = 0U;
        g_dlm.snapshot.syncStartedAtMs = nowMs;
        ++g_dlm.snapshot.generation;
        Dlm_Transition(DOMAIN_LIFECYCLE_STARTUP, nowMs);
        return DLM_STATUS_OK;
    }

    if (!g_dlm.snapshot.platformReady)
    {
        g_dlm.snapshot.platformReady = true;
        g_dlm.snapshot.syncTimedOut = false;
        g_dlm.snapshot.syncStartedAtMs = nowMs;
        Dlm_Transition(DOMAIN_LIFECYCLE_SYNCING, nowMs);
    }

    Dlm_Reevaluate(nowMs);
    return DLM_STATUS_OK;
}

DomainLifecycleStatus_t DomainLifecycle_MarkHealthy(
    DomainLifecycleSyncMask_t item,
    uint32_t nowMs)
{
    if (!g_dlm.initialized)
    {
        return DLM_STATUS_NOT_INITIALIZED;
    }

    if (!Dlm_IsSingleDefinedItem(item))
    {
        return DLM_STATUS_INVALID_ARGUMENT;
    }

    g_dlm.snapshot.resolvedMask |= item;
    g_dlm.snapshot.healthyMask |= item;

    Dlm_Reevaluate(nowMs);
    return DLM_STATUS_OK;
}

DomainLifecycleStatus_t DomainLifecycle_MarkUnavailable(
    DomainLifecycleSyncMask_t item,
    uint32_t nowMs)
{
    if (!g_dlm.initialized)
    {
        return DLM_STATUS_NOT_INITIALIZED;
    }

    if (!Dlm_IsSingleDefinedItem(item))
    {
        return DLM_STATUS_INVALID_ARGUMENT;
    }

    g_dlm.snapshot.resolvedMask |= item;
    g_dlm.snapshot.healthyMask &= ~item;

    Dlm_Reevaluate(nowMs);
    return DLM_STATUS_OK;
}

DomainLifecycleStatus_t DomainLifecycle_BeginResync(
    DomainLifecycleSyncMask_t mask,
    uint32_t nowMs)
{
    if (!g_dlm.initialized)
    {
        return DLM_STATUS_NOT_INITIALIZED;
    }

    if ((mask == 0U) ||
        ((mask & ~DLM_SYNC_ALL_DEFINED) != 0U))
    {
        return DLM_STATUS_INVALID_ARGUMENT;
    }

    g_dlm.snapshot.resolvedMask &= ~mask;
    g_dlm.snapshot.healthyMask &= ~mask;
    g_dlm.snapshot.syncTimedOut = false;
    g_dlm.snapshot.syncStartedAtMs = nowMs;
    ++g_dlm.snapshot.generation;

    if ((mask & g_dlm.snapshot.requiredMask) != 0U)
    {
        Dlm_Transition(DOMAIN_LIFECYCLE_SYNCING, nowMs);
    }

    Dlm_Reevaluate(nowMs);
    return DLM_STATUS_OK;
}

DomainLifecycleStatus_t DomainLifecycle_Process(
    uint32_t nowMs)
{
    DomainLifecycleSyncMask_t unresolvedRequired;

    if (!g_dlm.initialized)
    {
        return DLM_STATUS_NOT_INITIALIZED;
    }

    if ((!g_dlm.snapshot.platformReady) ||
        (g_dlm.config.syncTimeoutMs == 0U))
    {
        Dlm_Reevaluate(nowMs);
        return DLM_STATUS_OK;
    }

    unresolvedRequired =
        g_dlm.snapshot.requiredMask & ~g_dlm.snapshot.resolvedMask;

    if ((unresolvedRequired != 0U) &&
        (Dlm_ElapsedMs(nowMs, g_dlm.snapshot.syncStartedAtMs) >=
         g_dlm.config.syncTimeoutMs))
    {
        /*
         * Timeout은 해당 ECU가 FAILED라는 의미가 아니다.
         * 전체 Domain이 무한정 SYNCING에 머무르지 않도록
         * "현재 일부 근거 미확인"의 DEGRADED 운영으로 전환한다.
         */
        g_dlm.snapshot.syncTimedOut = true;
    }

    Dlm_Reevaluate(nowMs);
    return DLM_STATUS_OK;
}

DomainLifecycleStatus_t DomainLifecycle_GetSnapshot(
    DomainLifecycleSnapshot_t *outSnapshot)
{
    if (outSnapshot == NULL)
    {
        return DLM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_dlm.initialized)
    {
        return DLM_STATUS_NOT_INITIALIZED;
    }

    *outSnapshot = g_dlm.snapshot;
    return DLM_STATUS_OK;
}

bool DomainLifecycle_AllowsFeatureEvaluation(void)
{
    if (!g_dlm.initialized)
    {
        return false;
    }

    return ((g_dlm.snapshot.state == DOMAIN_LIFECYCLE_READY) ||
            (g_dlm.snapshot.state == DOMAIN_LIFECYCLE_DEGRADED));
}

bool DomainLifecycle_IsFullyReady(void)
{
    return (g_dlm.initialized &&
            (g_dlm.snapshot.state == DOMAIN_LIFECYCLE_READY));
}
