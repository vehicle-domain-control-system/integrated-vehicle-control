#include "network/app/net_uart_alive_diagnostic.h"
#include "network/net_hal_rtd.h"
#include "network/net_hal_adapter.h"
#include "network/net_wire_uart_m_core.h"
#include "net_hal_rtd_internal.h"
#include "mock_rtd.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", name, __LINE__, #x); return 1; } } while (0)
static const char *name;

static NetUartAliveDiagnosticConfig cfg(bool enabled)
{
    NetUartAliveDiagnosticConfig c;
    memset(&c, 0, sizeof(c));
    c.enabled = enabled; c.domain_boot_id = 1u;
    c.port_epoch = 1u; c.period_ms = 250u;
    return c;
}

static int disabled(void)
{
    NetUartAliveDiagnosticConfig c = cfg(false);
    MockRtd_Reset();
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_NOT_CONFIGURED);
    CHECK(MockRtd_UartInits(11u) == 0u && MockRtd_UartSends(11u) == 0u);
    CHECK(!MockRtd_CanInitialized(0u) && !MockRtd_CanStarted(0u));
    return 0;
}

static int alive(void)
{
    static const uint8_t golden[] = {
        0xa5u, 0x5au, 0x02u, 0x02u, 0x0cu, 0x00u, 0x01u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xffu, 0xffu, 0x00u, 0x03u, 0xccu, 0xc9u
    };
    NetUartAliveDiagnosticConfig c = cfg(true);
    NetWireUartFrame frame;
    NetWireMDomainAliveRaw value;
    uint16_t sequence;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    uint32_t n;
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    CHECK(MockRtd_UartInits(11u) == 1u);
    CHECK(!MockRtd_UartRxArmed(11u));
    CHECK(NetUartAliveDiagnostic_Step(0u) == NET_OK);
    n = MockRtd_UartTransmitTake(11u, bytes, sizeof(bytes));
    CHECK(n != 0u);
    CHECK(n == sizeof(golden) && memcmp(bytes, golden, sizeof(golden)) == 0);
    memset(&frame, 0, sizeof(frame)); memcpy(frame.bytes, bytes, n); frame.length = n;
    CHECK(net_wire_m_domain_alive_decode(NET_WIRE_CANDIDATE_ONLY, &frame, &value, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 1u && value.domain_boot_id == c.domain_boot_id);
    CHECK(value.domain_state == 0u && value.value_quality == 3u && value.source_age_ms == UINT16_MAX);
    CHECK(NetUartAliveDiagnostic_Step(1u) == NET_OK);
    MockRtd_AdvanceUs(249000u);
    CHECK(NetUartAliveDiagnostic_Step(249u) == NET_OK);
    CHECK(MockRtd_UartSends(11u) == 1u);
    MockRtd_AdvanceUs(1000u);
    CHECK(NetUartAliveDiagnostic_Step(250u) == NET_OK);
    { uint32_t t; for (t = 251u; t <= 260u && MockRtd_UartSends(11u) == 1u; ++t)
        CHECK(NetUartAliveDiagnostic_Step(t) == NET_OK); }
    n = MockRtd_UartTransmitTake(11u, bytes, sizeof(bytes)); CHECK(n != 0u);
    memset(&frame, 0, sizeof(frame)); memcpy(frame.bytes, bytes, n); frame.length = n;
    CHECK(net_wire_m_domain_alive_decode(NET_WIRE_CANDIDATE_ONLY, &frame, &value, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 2u);
    CHECK(!MockRtd_CanInitialized(0u) && !MockRtd_CanStarted(0u) && MockRtd_CanRxArmed(0u) == 0u);
    CHECK(NetUartAliveDiagnostic_Step(252u) == NET_OK);
    CHECK(NetUartAliveDiagnostic_Stop() == NET_OK);
    return 0;
}

static int tx_only_ignores_rx_error(void)
{
    NetUartAliveDiagnosticConfig c = cfg(true);
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    CHECK(!MockRtd_UartRxArmed(11u));
    CHECK(NetUartAliveDiagnostic_Step(0u) == NET_OK);
    CHECK(!MockRtd_UartRxArmed(11u));
    MockRtd_UartRaiseError(11u, LPUART_UART_IP_STATUS_FRAMING_ERROR);
    CHECK(!MockRtd_UartRxArmed(11u));
    CHECK(MockRtd_UartTransmitTake(11u, bytes, sizeof(bytes)) != 0u);
    CHECK(NetUartAliveDiagnostic_Step(1u) == NET_OK);
    CHECK(NetUartAliveDiagnostic_Stop() == NET_OK);
    return 0;
}

static int time_wrap(void)
{
    NetUartAliveDiagnosticConfig c = cfg(true);
    NetWireUartFrame frame;
    NetWireMDomainAliveRaw value;
    uint16_t sequence;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    uint32_t n;
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    CHECK(NetUartAliveDiagnostic_Step(UINT32_C(0xffffff80)) == NET_OK);
    CHECK(MockRtd_UartTransmitTake(11u, bytes, sizeof(bytes)) != 0u);
    MockRtd_AdvanceUs(249000u);
    CHECK(NetUartAliveDiagnostic_Step(UINT32_C(0x00000079)) == NET_OK);
    CHECK(MockRtd_UartSends(11u) == 1u);
    MockRtd_AdvanceUs(1000u);
    CHECK(NetUartAliveDiagnostic_Step(UINT32_C(0x0000007a)) == NET_OK);
    { uint32_t t; for (t = UINT32_C(0x0000007b);
                       t <= UINT32_C(0x00000084) && MockRtd_UartSends(11u) == 1u;
                       ++t)
        CHECK(NetUartAliveDiagnostic_Step(t) == NET_OK); }
    n = MockRtd_UartTransmitTake(11u, bytes, sizeof(bytes));
    CHECK(n != 0u);
    memset(&frame, 0, sizeof(frame)); memcpy(frame.bytes, bytes, n); frame.length = n;
    CHECK(net_wire_m_domain_alive_decode(NET_WIRE_CANDIDATE_ONLY, &frame, &value, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 2u);
    CHECK(NetUartAliveDiagnostic_Stop() == NET_OK);
    return 0;
}

static int exclusive(void)
{
    NetUartAliveDiagnosticConfig c = cfg(true);
    NetHalAdapter adapter; NetHalConfig out; NetHalRtdConfig in;
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    memset(&in, 0, sizeof(in)); in.can_port_epoch = 1u; in.uart_m_port_epoch = 1u;
    CHECK(net_hal_rtd_config(&adapter, &in, &out) == NET_BUSY);
    CHECK(!MockRtd_CanInitialized(0u));
    CHECK(NetUartAliveDiagnostic_Stop() == NET_OK);
    CHECK(net_hal_rtd_config(&adapter, &in, &out) == NET_OK);
    return 0;
}

static int cleanup_failure(void)
{
    NetUartAliveDiagnosticConfig c = cfg(true);
    NetHalAdapter adapter; NetHalConfig out; NetHalRtdConfig in;
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    CHECK(NetUartAliveDiagnostic_Step(0u) == NET_OK);
    MockRtd_UartSetAbortRefused(11u, true);
    CHECK(NetUartAliveDiagnostic_Stop() == NET_HOLD);
    memset(&in, 0, sizeof(in)); in.can_port_epoch = 1u; in.uart_m_port_epoch = 1u;
    CHECK(net_hal_rtd_config(&adapter, &in, &out) == NET_BUSY);
    return 0;
}

static int transmit_failure(void)
{
    NetUartAliveDiagnosticConfig c = cfg(true);
    NetHalAdapter adapter; NetHalConfig out; NetHalRtdConfig in;
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    MockRtd_UartSetSendErrors(11u, 1u);
    CHECK(NetUartAliveDiagnostic_Step(0u) == NET_OK);
    CHECK(NetUartAliveDiagnostic_Step(1u) == NET_HOLD);
    CHECK(NetUartAliveDiagnostic_GetStatus() == NET_UART_ALIVE_DIAG_FAILED);
    memset(&in, 0, sizeof(in)); in.can_port_epoch = 1u; in.uart_m_port_epoch = 1u;
    CHECK(net_hal_rtd_config(&adapter, &in, &out) == NET_OK);
    CHECK(!MockRtd_CanInitialized(0u) && !MockRtd_CanStarted(0u));
    return 0;
}

static NetStatus production_config(NetHalAdapter *adapter)
{
    NetHalConfig out; NetHalRtdConfig in;
    memset(&in, 0, sizeof(in)); in.can_port_epoch = 1u; in.uart_m_port_epoch = 1u;
    { NetStatus st = net_hal_rtd_config(adapter, &in, &out); if (st != NET_OK) return st;
      return net_hal_adapter_init(adapter, &out, NET_HAL_S32K344); }
}

static int production_success(void)
{
    NetHalAdapter adapter; NetUartAliveDiagnosticConfig c = cfg(true);
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(production_config(&adapter) == NET_OK);
    CHECK(net_hal_rtd_start() == NET_OK);
    CHECK(net_hal_rtd_hardware_init_status() == NET_HAL_RTD_HW_PREPARED);
    CHECK(MockRtd_CanInitialized(0u) && MockRtd_CanStarted(0u));
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_BUSY);
    return 0;
}

static int production_rollback(void)
{
    NetHalAdapter adapter; NetUartAliveDiagnosticConfig c = cfg(true);
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(production_config(&adapter) == NET_OK);
    MockRtd_CanSetReceiveRefused(0u, true);
    CHECK(net_hal_rtd_start() == NET_HOLD);
    CHECK(net_hal_rtd_hardware_init_status() == NET_HAL_RTD_HW_FAILED);
    CHECK(!MockRtd_CanInitialized(0u) && !MockRtd_CanStarted(0u));
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_OK);
    CHECK(NetUartAliveDiagnostic_Stop() == NET_OK);
    return 0;
}

static int production_rollback_failure(void)
{
    NetHalAdapter adapter; NetUartAliveDiagnosticConfig c = cfg(true);
    MockRtd_Reset(); MockRtd_SetSystemTimerEnabled(true);
    CHECK(production_config(&adapter) == NET_OK);
    MockRtd_CanSetReceiveRefused(0u, true);
    MockRtd_CanSetDeinitRefused(0u, true);
    CHECK(net_hal_rtd_start() == NET_HOLD);
    CHECK(net_hal_rtd_hardware_init_status() == NET_HAL_RTD_HW_ROLLBACK_FAILED);
    CHECK(NetUartAliveDiagnostic_Init(&c) == NET_BUSY);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    name = argv[1];
    if (strcmp(name, "disabled") == 0) return disabled();
    if (strcmp(name, "alive") == 0) return alive();
    if (strcmp(name, "tx_only_ignores_rx_error") == 0) return tx_only_ignores_rx_error();
    if (strcmp(name, "time_wrap") == 0) return time_wrap();
    if (strcmp(name, "exclusive") == 0) return exclusive();
    if (strcmp(name, "cleanup_failure") == 0) return cleanup_failure();
    if (strcmp(name, "transmit_failure") == 0) return transmit_failure();
    if (strcmp(name, "production_success") == 0) return production_success();
    if (strcmp(name, "production_rollback") == 0) return production_rollback();
    if (strcmp(name, "production_rollback_failure") == 0) return production_rollback_failure();
    return 2;
}
