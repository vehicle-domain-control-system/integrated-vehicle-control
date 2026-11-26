#ifndef NETWORK_APP_NET_UART_ALIVE_DIAGNOSTIC_H
#define NETWORK_APP_NET_UART_ALIVE_DIAGNOSTIC_H

#include "network/net_types.h"

typedef struct {
    bool enabled;
    uint32_t domain_boot_id;
    uint64_t port_epoch;
    uint32_t period_ms;
} NetUartAliveDiagnosticConfig;

typedef enum {
    NET_UART_ALIVE_DIAG_DISABLED = 0,
    NET_UART_ALIVE_DIAG_RUNNING,
    NET_UART_ALIVE_DIAG_FAILED
} NetUartAliveDiagnosticStatus;

NetStatus NetUartAliveDiagnostic_Init(const NetUartAliveDiagnosticConfig *config);
NetStatus NetUartAliveDiagnostic_Step(uint32_t now_ms);
NetStatus NetUartAliveDiagnostic_Stop(void);
NetUartAliveDiagnosticStatus NetUartAliveDiagnostic_GetStatus(void);

#endif
