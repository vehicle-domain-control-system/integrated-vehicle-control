#ifndef NET_UART_ALIVE_DIAGNOSTIC_H
#define NET_UART_ALIVE_DIAGNOSTIC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { NET_OK = 0, NET_HOLD = 8 } NetStatus;

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

#endif /* NET_UART_ALIVE_DIAGNOSTIC_H */
