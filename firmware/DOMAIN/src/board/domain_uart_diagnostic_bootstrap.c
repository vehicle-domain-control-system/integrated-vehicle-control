#include "domain_uart_diagnostic_bootstrap.h"

#include <stddef.h>
#include <string.h>

#include "network/app/net_uart_alive_diagnostic.h"

static DomainUartDiagnosticBootstrap_Status_t StopAndLatchFault(
    DomainUartDiagnosticBootstrap_t *context)
{
    if (!context->stopAttempted)
    {
        context->stopAttempted = true;
        (void)NetUartAliveDiagnostic_Stop();
    }
    context->status = DOMAIN_UART_DIAGNOSTIC_FAULT;
    return context->status;
}

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_Init(
    DomainUartDiagnosticBootstrap_t *context,
    const DomainUartDiagnosticBootstrap_Config_t *config)
{
    NetUartAliveDiagnosticConfig networkConfig;

    if ((context == NULL) || (config == NULL))
    {
        return DOMAIN_UART_DIAGNOSTIC_INVALID_ARGUMENT;
    }
    if (context->initialized)
    {
        return DOMAIN_UART_DIAGNOSTIC_ALREADY;
    }

    (void)memset(context, 0, sizeof(*context));
    context->initialized = true;
    if (!config->approved)
    {
        context->status = DOMAIN_UART_DIAGNOSTIC_DISABLED;
        return context->status;
    }
    if ((config->domainBootId == 0U) || (config->portEpoch == 0U) ||
        (config->aliveIntervalMs == 0U))
    {
        context->status = DOMAIN_UART_DIAGNOSTIC_INVALID_ARGUMENT;
        return context->status;
    }

    (void)memset(&networkConfig, 0, sizeof(networkConfig));
    networkConfig.enabled = true;
    networkConfig.domain_boot_id = config->domainBootId;
    networkConfig.port_epoch = config->portEpoch;
    networkConfig.period_ms = config->aliveIntervalMs;

    if ((NetUartAliveDiagnostic_Init(&networkConfig) != NET_OK) ||
        (NetUartAliveDiagnostic_GetStatus() != NET_UART_ALIVE_DIAG_RUNNING))
    {
        context->status = DOMAIN_UART_DIAGNOSTIC_FAULT;
        return context->status;
    }

    context->status = DOMAIN_UART_DIAGNOSTIC_RUNNING;
    return context->status;
}

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_Poll(
    DomainUartDiagnosticBootstrap_t *context,
    uint32_t nowMs)
{
    if ((context == NULL) || !context->initialized)
    {
        return DOMAIN_UART_DIAGNOSTIC_NOT_INITIALIZED;
    }
    if (context->status == DOMAIN_UART_DIAGNOSTIC_FAULT)
    {
        return DOMAIN_UART_DIAGNOSTIC_FAULT;
    }
    if (context->status != DOMAIN_UART_DIAGNOSTIC_RUNNING)
    {
        return DOMAIN_UART_DIAGNOSTIC_ALREADY;
    }
    if (NetUartAliveDiagnostic_Step(nowMs) != NET_OK)
    {
        return StopAndLatchFault(context);
    }
    return DOMAIN_UART_DIAGNOSTIC_RUNNING;
}

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_Stop(
    DomainUartDiagnosticBootstrap_t *context)
{
    if ((context == NULL) || !context->initialized)
    {
        return DOMAIN_UART_DIAGNOSTIC_NOT_INITIALIZED;
    }
    if (context->status == DOMAIN_UART_DIAGNOSTIC_FAULT)
    {
        return DOMAIN_UART_DIAGNOSTIC_FAULT;
    }
    if (context->status != DOMAIN_UART_DIAGNOSTIC_RUNNING)
    {
        return DOMAIN_UART_DIAGNOSTIC_ALREADY;
    }

    context->stopAttempted = true;
    if (NetUartAliveDiagnostic_Stop() != NET_OK)
    {
        context->status = DOMAIN_UART_DIAGNOSTIC_FAULT;
        return context->status;
    }
    context->status = DOMAIN_UART_DIAGNOSTIC_STOPPED;
    return context->status;
}

DomainUartDiagnosticBootstrap_Status_t DomainUartDiagnosticBootstrap_GetStatus(
    const DomainUartDiagnosticBootstrap_t *context)
{
    if ((context == NULL) || !context->initialized)
    {
        return DOMAIN_UART_DIAGNOSTIC_NOT_INITIALIZED;
    }
    return context->status;
}
