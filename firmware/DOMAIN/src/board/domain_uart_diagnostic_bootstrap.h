#ifndef DOMAIN_UART_DIAGNOSTIC_BOOTSTRAP_H
#define DOMAIN_UART_DIAGNOSTIC_BOOTSTRAP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DOMAIN_UART_DIAGNOSTIC_NOT_INITIALIZED = 0,
    DOMAIN_UART_DIAGNOSTIC_DISABLED,
    DOMAIN_UART_DIAGNOSTIC_RUNNING,
    DOMAIN_UART_DIAGNOSTIC_STOPPED,
    DOMAIN_UART_DIAGNOSTIC_FAULT,
    DOMAIN_UART_DIAGNOSTIC_INVALID_ARGUMENT,
    DOMAIN_UART_DIAGNOSTIC_ALREADY
} DomainUartDiagnosticBootstrap_Status_t;

typedef struct
{
    bool approved;
    uint32_t domainBootId;
    uint64_t portEpoch;
    uint32_t aliveIntervalMs;
} DomainUartDiagnosticBootstrap_Config_t;

typedef struct
{
    bool initialized;
    bool stopAttempted;
    DomainUartDiagnosticBootstrap_Status_t status;
} DomainUartDiagnosticBootstrap_t;

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_Init(
    DomainUartDiagnosticBootstrap_t *context,
    const DomainUartDiagnosticBootstrap_Config_t *config);

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_Poll(
    DomainUartDiagnosticBootstrap_t *context,
    uint32_t nowMs);

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_Stop(
    DomainUartDiagnosticBootstrap_t *context);

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_GetStatus(
    const DomainUartDiagnosticBootstrap_t *context);

#endif /* DOMAIN_UART_DIAGNOSTIC_BOOTSTRAP_H */
