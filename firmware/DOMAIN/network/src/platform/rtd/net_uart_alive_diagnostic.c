#include "network/app/net_uart_alive_diagnostic.h"
#include "network/net_hal_adapter.h"
#include "network/net_upper.h"
#include "network/net_upper_publish.h"
#include "net_hal_rtd_internal.h"
#include <string.h>

typedef struct {
    NetUartAliveDiagnosticStatus status;
    NetHalAdapter hal;
    NetUpper upper;
    NetUpperPublisher publisher;
    uint32_t boot_id, period_ms;
    uint32_t last_now_ms32;
    uint64_t extended_now_ms;
    bool has_now;
    bool submitted_once;
} NetUartAliveDiagnostic;

static NetUartAliveDiagnostic diagnostic;

static bool extend_now_ms(uint32_t now_ms, uint64_t *extended_out)
{
    uint32_t elapsed;
    if (!diagnostic.has_now) {
        diagnostic.last_now_ms32 = now_ms;
        diagnostic.extended_now_ms = now_ms;
        diagnostic.has_now = true;
    } else {
        elapsed = now_ms - diagnostic.last_now_ms32;
        /* A single-owner superloop must be observed at least once per half
         * uint32 range. A larger delta is indistinguishable from time moving
         * backwards, so fail closed instead of advancing the schedule. */
        if (elapsed > INT32_MAX || diagnostic.extended_now_ms > UINT64_MAX - elapsed)
            return false;
        diagnostic.last_now_ms32 = now_ms;
        diagnostic.extended_now_ms += elapsed;
    }
    *extended_out = diagnostic.extended_now_ms;
    return true;
}

static NetStatus configure_upper(uint64_t epoch)
{
    NetHalConfig hal_config;
    NetUpperConfig upper_config;
    NetStatus st;
    size_t i;
    st = net_hal_rtd_uart_diagnostic_config(&diagnostic.hal, epoch, &hal_config);
    if (st != NET_OK) return st;
    st = net_hal_adapter_init(&diagnostic.hal, &hal_config, NET_HAL_S32K344);
    if (st != NET_OK) return st;
    memset(&upper_config, 0, sizeof(upper_config));
    upper_config.node = NET_WIRE_NODE_DOMAIN;
    upper_config.local_epoch = epoch;
    for (i = 0u; i < 3u; ++i) upper_config.port_epoch[i] = epoch;
    upper_config.profile = NET_WIRE_CANDIDATE_ONLY;
    upper_config.port = net_hal_rtd_port(&diagnostic.hal);
    upper_config.uart_sequence_bound[NET_WIRE_UART_M - 1u] = true;
    upper_config.uart_initial_sequence[NET_WIRE_UART_M - 1u] = 1u;
    return net_upper_init(&diagnostic.upper, &upper_config);
}

NetStatus NetUartAliveDiagnostic_Init(const NetUartAliveDiagnosticConfig *config)
{
    NetStatus st;
    NetUpperPublishEntry entry;
    NetUpperPublishConfig publish_config;
    if (config == NULL) return NET_INVALID_ARGUMENT;
    if (!config->enabled) return NET_NOT_CONFIGURED;
    if (config->domain_boot_id == 0u || config->port_epoch == 0u || config->period_ms == 0u)
        return NET_INVALID_ARGUMENT;
    if (diagnostic.status != NET_UART_ALIVE_DIAG_DISABLED) return NET_BUSY;
    memset(&diagnostic, 0, sizeof(diagnostic));
    st = configure_upper(config->port_epoch);
    if (st != NET_OK) {
        (void)net_hal_rtd_uart_diagnostic_stop();
        return st;
    }
    diagnostic.boot_id = config->domain_boot_id;
    diagnostic.period_ms = config->period_ms;
    memset(&entry, 0, sizeof(entry));
    entry.link = NET_WIRE_UART_M; entry.id = 0x02u; entry.period_ms = config->period_ms;
    entry.age.bound = true; entry.age.maximum_age_ms = UINT64_MAX;
    memset(&publish_config, 0, sizeof(publish_config));
    publish_config.entries = &entry; publish_config.entry_count = 1u;
    st = net_upper_publish_init(&diagnostic.publisher, &diagnostic.upper, &publish_config);
    if (st != NET_OK) {
        (void)net_hal_rtd_uart_diagnostic_stop();
        return st;
    }
    st = net_hal_rtd_uart_diagnostic_start();
    if (st != NET_OK) {
        (void)net_hal_rtd_uart_diagnostic_stop();
        diagnostic.status = NET_UART_ALIVE_DIAG_FAILED;
        return st;
    }
    diagnostic.status = NET_UART_ALIVE_DIAG_RUNNING;
    return NET_OK;
}

static NetStatus submit_alive(void)
{
    NetUpperUartMessage message;
    memset(&message, 0, sizeof(message));
    message.link = NET_WIRE_UART_M;
    message.type = 0x02u;
    message.data.m_domain_alive.domain_boot_id = diagnostic.boot_id;
    message.data.m_domain_alive.update_sequence = 0u;
    message.data.m_domain_alive.source_age_ms = UINT16_MAX;
    message.data.m_domain_alive.domain_state = 0u;
    message.data.m_domain_alive.value_quality = 3u;
    if (net_upper_publish_uart(&diagnostic.publisher, &message, NULL, NULL) != NET_OK) return NET_HOLD;
    diagnostic.submitted_once = true;
    return NET_OK;
}

NetStatus NetUartAliveDiagnostic_Step(uint32_t now_ms)
{
    NetStatus st = NET_OK, publish_status;
    NetUpperPublishView view;
    uint64_t extended_now_ms;
    if (diagnostic.status != NET_UART_ALIVE_DIAG_RUNNING) return NET_NOT_CONFIGURED;
    if (!extend_now_ms(now_ms, &extended_now_ms)) {
        (void)net_hal_rtd_uart_diagnostic_stop();
        diagnostic.status = NET_UART_ALIVE_DIAG_FAILED;
        return NET_HOLD;
    }
    (void)net_hal_rtd_set_now(extended_now_ms);
    (void)net_hal_rtd_uart_diagnostic_drain();
    (void)net_upper_step(&diagnostic.upper, NET_UPPER_POLL_BUDGET);
    if (!diagnostic.submitted_once) st = submit_alive();
    publish_status = net_upper_publish_service(&diagnostic.publisher);
    if (st == NET_OK && publish_status != NET_OK && publish_status != NET_NO_WORK) st = publish_status;
    (void)net_upper_step(&diagnostic.upper, NET_UPPER_POLL_BUDGET);
    net_hal_rtd_uart_diagnostic_service();
    if (st != NET_OK) {
        (void)net_hal_rtd_uart_diagnostic_stop();
        diagnostic.status = NET_UART_ALIVE_DIAG_FAILED;
        return st;
    }
    memset(&view, 0, sizeof(view));
    if (net_upper_publish_view(&diagnostic.publisher, NET_WIRE_UART_M, 0x02u, &view) == NET_OK &&
        (view.counters.not_sent != 0u || view.counters.tx_failed != 0u)) {
        (void)net_hal_rtd_uart_diagnostic_stop();
        diagnostic.status = NET_UART_ALIVE_DIAG_FAILED;
        return NET_HOLD;
    }
    return st;
}

NetStatus NetUartAliveDiagnostic_Stop(void)
{
    NetStatus st;
    if (diagnostic.status == NET_UART_ALIVE_DIAG_DISABLED) return NET_NOT_CONFIGURED;
    st = net_hal_rtd_uart_diagnostic_stop();
    diagnostic.status = (st == NET_OK) ? NET_UART_ALIVE_DIAG_DISABLED : NET_UART_ALIVE_DIAG_FAILED;
    return st;
}

NetUartAliveDiagnosticStatus NetUartAliveDiagnostic_GetStatus(void)
{
    return diagnostic.status;
}
