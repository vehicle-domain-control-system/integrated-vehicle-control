#include "LinkStateManager.h"

#include <string.h>

#define LINK_STATE_MANAGER_LINK_COUNT (2U)

typedef struct
{
    LinkStateManager_LinkSnapshot_t snapshot;
} LinkStateManager_Entry_t;

static LinkStateManager_Entry_t s_links[LINK_STATE_MANAGER_LINK_COUNT];
static bool s_initialized = false;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static bool LinkStateManager_IsValidLink(
    Gateway_LinkId_t link)
{
    return (link == GATEWAY_LINK_BLUETOOTH)
        || (link == GATEWAY_LINK_DOMAIN_UART);
}

static LinkStateManager_Entry_t *LinkStateManager_GetEntry(
    Gateway_LinkId_t link)
{
    if (!LinkStateManager_IsValidLink(link))
    {
        return NULL;
    }

    return &s_links[(unsigned int)link];
}

static const LinkStateManager_Entry_t *LinkStateManager_GetConstEntry(
    Gateway_LinkId_t link)
{
    if (!LinkStateManager_IsValidLink(link))
    {
        return NULL;
    }

    return &s_links[(unsigned int)link];
}

static void LinkStateManager_SetState(
    LinkStateManager_Entry_t *entry,
    Gateway_LinkState_t state,
    Gateway_TimeMs_t now_ms)
{
    if (entry->snapshot.state != state)
    {
        entry->snapshot.state = state;
        entry->snapshot.state_since_ms = now_ms;
    }
}

static Gateway_TimeMs_t LinkStateManager_GetTimeoutBasisMs(
    const LinkStateManager_Entry_t *entry)
{
    if (entry->snapshot.has_valid_rx)
    {
        return entry->snapshot.last_valid_rx_ms;
    }

    return entry->snapshot.state_since_ms;
}

static void LinkStateManager_InvalidateCurrentRxEvidence(
    LinkStateManager_Entry_t *entry)
{
    /*
     * last_valid_rx_ms 값 자체는 진단/관찰용으로 남겨둘 수 있지만,
     * 이전 연결 구간의 RX를 현재 연결 구간의 Timeout 근거로 재사용하지 않는다.
     */
    entry->snapshot.has_valid_rx = false;
}


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_Init(void)
{
    size_t i;

    memset(s_links, 0, sizeof(s_links));

    for (i = 0U; i < LINK_STATE_MANAGER_LINK_COUNT; ++i)
    {
        s_links[i].snapshot.state = GATEWAY_LINK_STATE_UNKNOWN;
        s_links[i].snapshot.state_since_ms = Gateway_Time_GetMs();
        s_links[i].snapshot.has_valid_rx = false;
        s_links[i].snapshot.last_valid_rx_ms = 0U;
        s_links[i].snapshot.timeout.enabled = false;
        s_links[i].snapshot.timeout.timeout_ms = 0U;
    }

    s_initialized = true;

    return GATEWAY_STATUS_OK;
}

void LinkStateManager_Reset(void)
{
    memset(s_links, 0, sizeof(s_links));
    s_initialized = false;
}

bool LinkStateManager_IsInitialized(void)
{
    return s_initialized;
}


/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_ConfigureTimeout(
    Gateway_LinkId_t link,
    LinkStateManager_TimeoutConfig_t config)
{
    LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    entry = LinkStateManager_GetEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (config.enabled && (config.timeout_ms == 0U))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    entry->snapshot.timeout = config;

    if (!config.enabled)
    {
        entry->snapshot.timeout.timeout_ms = 0U;
    }

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Explicit state events                                                      */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_MarkAvailableAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms)
{
    LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    entry = LinkStateManager_GetEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (entry->snapshot.state != GATEWAY_LINK_STATE_AVAILABLE)
    {
        LinkStateManager_InvalidateCurrentRxEvidence(entry);
    }

    LinkStateManager_SetState(
        entry,
        GATEWAY_LINK_STATE_AVAILABLE,
        now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t LinkStateManager_MarkUnavailableAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms)
{
    LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    entry = LinkStateManager_GetEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    LinkStateManager_InvalidateCurrentRxEvidence(entry);

    LinkStateManager_SetState(
        entry,
        GATEWAY_LINK_STATE_UNAVAILABLE,
        now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t LinkStateManager_BeginRecoveryAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms)
{
    LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    entry = LinkStateManager_GetEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    LinkStateManager_InvalidateCurrentRxEvidence(entry);

    LinkStateManager_SetState(
        entry,
        GATEWAY_LINK_STATE_RECOVERING,
        now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t LinkStateManager_OnValidRxAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms)
{
    LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    entry = LinkStateManager_GetEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    entry->snapshot.has_valid_rx = true;
    entry->snapshot.last_valid_rx_ms = now_ms;

    LinkStateManager_SetState(
        entry,
        GATEWAY_LINK_STATE_AVAILABLE,
        now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t LinkStateManager_MarkAvailable(
    Gateway_LinkId_t link)
{
    return LinkStateManager_MarkAvailableAt(
        link,
        Gateway_Time_GetMs());
}

Gateway_Status_t LinkStateManager_MarkUnavailable(
    Gateway_LinkId_t link)
{
    return LinkStateManager_MarkUnavailableAt(
        link,
        Gateway_Time_GetMs());
}

Gateway_Status_t LinkStateManager_BeginRecovery(
    Gateway_LinkId_t link)
{
    return LinkStateManager_BeginRecoveryAt(
        link,
        Gateway_Time_GetMs());
}

Gateway_Status_t LinkStateManager_OnValidRx(
    Gateway_LinkId_t link)
{
    return LinkStateManager_OnValidRxAt(
        link,
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Periodic supervision                                                       */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_UpdateAt(
    Gateway_TimeMs_t now_ms)
{
    size_t i;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    for (i = 0U; i < LINK_STATE_MANAGER_LINK_COUNT; ++i)
    {
        LinkStateManager_Entry_t *entry = &s_links[i];
        const LinkStateManager_TimeoutConfig_t timeout =
            entry->snapshot.timeout;

        if (!timeout.enabled)
        {
            continue;
        }

        if (entry->snapshot.state != GATEWAY_LINK_STATE_AVAILABLE)
        {
            continue;
        }

        if (Gateway_Time_HasElapsed(
                LinkStateManager_GetTimeoutBasisMs(entry),
                timeout.timeout_ms,
                now_ms))
        {
            LinkStateManager_InvalidateCurrentRxEvidence(entry);

            LinkStateManager_SetState(
                entry,
                GATEWAY_LINK_STATE_UNAVAILABLE,
                now_ms);
        }
    }

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t LinkStateManager_UpdateNow(void)
{
    return LinkStateManager_UpdateAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Query                                                                      */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_GetSnapshot(
    Gateway_LinkId_t link,
    LinkStateManager_LinkSnapshot_t *snapshot)
{
    const LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (snapshot == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    entry = LinkStateManager_GetConstEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    *snapshot = entry->snapshot;

    return GATEWAY_STATUS_OK;
}

Gateway_LinkState_t LinkStateManager_GetState(
    Gateway_LinkId_t link)
{
    const LinkStateManager_Entry_t *entry;

    if (!s_initialized)
    {
        return GATEWAY_LINK_STATE_UNKNOWN;
    }

    entry = LinkStateManager_GetConstEntry(link);

    if (entry == NULL)
    {
        return GATEWAY_LINK_STATE_UNKNOWN;
    }

    return entry->snapshot.state;
}

bool LinkStateManager_IsAvailable(
    Gateway_LinkId_t link)
{
    return LinkStateManager_GetState(link)
        == GATEWAY_LINK_STATE_AVAILABLE;
}
