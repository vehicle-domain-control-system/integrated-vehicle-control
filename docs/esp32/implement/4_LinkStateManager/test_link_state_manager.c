#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "LinkStateManager.h"

static void test_initial_state(void)
{
    LinkStateManager_LinkSnapshot_t snapshot;

    LinkStateManager_Reset();

    assert(LinkStateManager_GetSnapshot(
        GATEWAY_LINK_BLUETOOTH,
        &snapshot) == GATEWAY_STATUS_NOT_READY);

    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);
    assert(LinkStateManager_IsInitialized());

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_BLUETOOTH) == GATEWAY_LINK_STATE_UNKNOWN);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_UNKNOWN);

    assert(!LinkStateManager_IsAvailable(
        GATEWAY_LINK_BLUETOOTH));
}

static void test_independent_links(void)
{
    assert(LinkStateManager_MarkAvailableAt(
        GATEWAY_LINK_BLUETOOTH,
        100U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_BLUETOOTH) == GATEWAY_LINK_STATE_AVAILABLE);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_UNKNOWN);

    assert(LinkStateManager_MarkUnavailableAt(
        GATEWAY_LINK_DOMAIN_UART,
        110U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_BLUETOOTH) == GATEWAY_LINK_STATE_AVAILABLE);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_UNAVAILABLE);
}

static void test_valid_rx_marks_available(void)
{
    LinkStateManager_LinkSnapshot_t snapshot;

    assert(LinkStateManager_BeginRecoveryAt(
        GATEWAY_LINK_DOMAIN_UART,
        200U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_RECOVERING);

    assert(LinkStateManager_OnValidRxAt(
        GATEWAY_LINK_DOMAIN_UART,
        250U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetSnapshot(
        GATEWAY_LINK_DOMAIN_UART,
        &snapshot) == GATEWAY_STATUS_OK);

    assert(snapshot.state == GATEWAY_LINK_STATE_AVAILABLE);
    assert(snapshot.state_since_ms == 250U);
    assert(snapshot.has_valid_rx);
    assert(snapshot.last_valid_rx_ms == 250U);
}

static void test_timeout_disabled_by_default(void)
{
    assert(LinkStateManager_MarkAvailableAt(
        GATEWAY_LINK_BLUETOOTH,
        1000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_UpdateAt(
        100000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_BLUETOOTH) == GATEWAY_LINK_STATE_AVAILABLE);
}

static void test_timeout_from_available_entry(void)
{
    const LinkStateManager_TimeoutConfig_t config = {
        .enabled = true,
        .timeout_ms = 100U
    };

    LinkStateManager_Reset();
    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);

    assert(LinkStateManager_ConfigureTimeout(
        GATEWAY_LINK_DOMAIN_UART,
        config) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_MarkAvailableAt(
        GATEWAY_LINK_DOMAIN_UART,
        1000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_UpdateAt(
        1099U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_AVAILABLE);

    assert(LinkStateManager_UpdateAt(
        1100U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_UNAVAILABLE);
}

static void test_valid_rx_refreshes_timeout_basis(void)
{
    const LinkStateManager_TimeoutConfig_t config = {
        .enabled = true,
        .timeout_ms = 100U
    };

    LinkStateManager_Reset();
    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);

    assert(LinkStateManager_ConfigureTimeout(
        GATEWAY_LINK_DOMAIN_UART,
        config) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_MarkAvailableAt(
        GATEWAY_LINK_DOMAIN_UART,
        1000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_OnValidRxAt(
        GATEWAY_LINK_DOMAIN_UART,
        1050U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_UpdateAt(
        1149U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_AVAILABLE);

    assert(LinkStateManager_UpdateAt(
        1150U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_UNAVAILABLE);
}

static void test_recovering_not_timed_out_by_available_timeout(void)
{
    const LinkStateManager_TimeoutConfig_t config = {
        .enabled = true,
        .timeout_ms = 100U
    };

    LinkStateManager_Reset();
    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);

    assert(LinkStateManager_ConfigureTimeout(
        GATEWAY_LINK_DOMAIN_UART,
        config) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_BeginRecoveryAt(
        GATEWAY_LINK_DOMAIN_UART,
        1000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_UpdateAt(
        5000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_RECOVERING);
}

static void test_timeout_wraparound(void)
{
    const LinkStateManager_TimeoutConfig_t config = {
        .enabled = true,
        .timeout_ms = 25U
    };

    const Gateway_TimeMs_t start_ms = UINT32_MAX - 9U;

    LinkStateManager_Reset();
    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);

    assert(LinkStateManager_ConfigureTimeout(
        GATEWAY_LINK_BLUETOOTH,
        config) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_OnValidRxAt(
        GATEWAY_LINK_BLUETOOTH,
        start_ms) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_UpdateAt(
        14U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_BLUETOOTH) == GATEWAY_LINK_STATE_AVAILABLE);

    assert(LinkStateManager_UpdateAt(
        15U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_BLUETOOTH) == GATEWAY_LINK_STATE_UNAVAILABLE);
}


static void test_old_rx_not_reused_after_reconnect(void)
{
    const LinkStateManager_TimeoutConfig_t config = {
        .enabled = true,
        .timeout_ms = 100U
    };

    LinkStateManager_LinkSnapshot_t snapshot;

    LinkStateManager_Reset();
    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);

    assert(LinkStateManager_ConfigureTimeout(
        GATEWAY_LINK_DOMAIN_UART,
        config) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_OnValidRxAt(
        GATEWAY_LINK_DOMAIN_UART,
        1000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_MarkUnavailableAt(
        GATEWAY_LINK_DOMAIN_UART,
        1050U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_BeginRecoveryAt(
        GATEWAY_LINK_DOMAIN_UART,
        1900U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_MarkAvailableAt(
        GATEWAY_LINK_DOMAIN_UART,
        2000U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetSnapshot(
        GATEWAY_LINK_DOMAIN_UART,
        &snapshot) == GATEWAY_STATUS_OK);

    assert(!snapshot.has_valid_rx);

    /* old 1000 ms RX가 아니라 새 AVAILABLE 진입 2000 ms를 기준으로 감시 */
    assert(LinkStateManager_UpdateAt(
        2099U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_AVAILABLE);

    assert(LinkStateManager_UpdateAt(
        2100U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART) == GATEWAY_LINK_STATE_UNAVAILABLE);
}

static void test_invalid_arguments(void)
{
    const LinkStateManager_TimeoutConfig_t invalid_timeout = {
        .enabled = true,
        .timeout_ms = 0U
    };

    LinkStateManager_LinkSnapshot_t snapshot;

    assert(LinkStateManager_ConfigureTimeout(
        (Gateway_LinkId_t)99,
        invalid_timeout) == GATEWAY_STATUS_INVALID_ARGUMENT);

    assert(LinkStateManager_ConfigureTimeout(
        GATEWAY_LINK_BLUETOOTH,
        invalid_timeout) == GATEWAY_STATUS_INVALID_ARGUMENT);

    assert(LinkStateManager_GetSnapshot(
        GATEWAY_LINK_BLUETOOTH,
        NULL) == GATEWAY_STATUS_INVALID_ARGUMENT);

    assert(LinkStateManager_GetSnapshot(
        (Gateway_LinkId_t)99,
        &snapshot) == GATEWAY_STATUS_INVALID_ARGUMENT);

    assert(LinkStateManager_MarkAvailableAt(
        (Gateway_LinkId_t)99,
        0U) == GATEWAY_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_initial_state();
    test_independent_links();
    test_valid_rx_marks_available();
    test_timeout_disabled_by_default();
    test_timeout_from_available_entry();
    test_valid_rx_refreshes_timeout_basis();
    test_recovering_not_timed_out_by_available_timeout();
    test_timeout_wraparound();
    test_old_rx_not_reused_after_reconnect();
    test_invalid_arguments();

    return 0;
}
