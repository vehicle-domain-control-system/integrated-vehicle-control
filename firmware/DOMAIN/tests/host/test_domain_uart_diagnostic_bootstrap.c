#include <stdio.h>
#include <string.h>

#include "domain_uart_diagnostic_bootstrap.h"
#include "network/app/net_uart_alive_diagnostic.h"

#define CHECK(condition_) do { if (!(condition_)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition_); return 1; \
} } while (0)

static unsigned initCalls, stepCalls, stopCalls;
static NetStatus initResult, stepResult, stopResult;
static NetUartAliveDiagnosticStatus networkStatus;
static NetUartAliveDiagnosticConfig lastConfig;
static uint32_t lastNow;

static void ResetFakes(void)
{
    initCalls = stepCalls = stopCalls = 0U;
    initResult = stepResult = stopResult = NET_OK;
    networkStatus = NET_UART_ALIVE_DIAG_DISABLED;
    (void)memset(&lastConfig, 0, sizeof(lastConfig));
    lastNow = 0U;
}

NetStatus NetUartAliveDiagnostic_Init(const NetUartAliveDiagnosticConfig *config)
{
    ++initCalls;
    lastConfig = *config;
    if (initResult == NET_OK) networkStatus = NET_UART_ALIVE_DIAG_RUNNING;
    return initResult;
}

NetStatus NetUartAliveDiagnostic_Step(uint32_t nowMs)
{
    ++stepCalls;
    lastNow = nowMs;
    return stepResult;
}

NetStatus NetUartAliveDiagnostic_Stop(void)
{
    ++stopCalls;
    if (stopResult == NET_OK) networkStatus = NET_UART_ALIVE_DIAG_DISABLED;
    return stopResult;
}

NetUartAliveDiagnosticStatus NetUartAliveDiagnostic_GetStatus(void)
{
    return networkStatus;
}

static DomainUartDiagnosticBootstrap_Config_t Config(bool approved)
{
    DomainUartDiagnosticBootstrap_Config_t config;
    config.approved = approved;
    config.domainBootId = 1U;
    config.portEpoch = 1U;
    config.aliveIntervalMs = 250U;
    return config;
}

static int TestDisabled(void)
{
    DomainUartDiagnosticBootstrap_t context = {0};
    DomainUartDiagnosticBootstrap_Config_t config = Config(false);
    ResetFakes();
    CHECK(DomainUartDiagnosticBootstrap_Init(&context, &config) ==
          DOMAIN_UART_DIAGNOSTIC_DISABLED);
    CHECK(initCalls == 0U && stepCalls == 0U && stopCalls == 0U);
    CHECK(DomainUartDiagnosticBootstrap_Poll(&context, 1U) ==
          DOMAIN_UART_DIAGNOSTIC_ALREADY);
    return 0;
}

static int TestApprovedAndValues(void)
{
    DomainUartDiagnosticBootstrap_t context = {0};
    DomainUartDiagnosticBootstrap_Config_t config = Config(true);
    ResetFakes();
    CHECK(DomainUartDiagnosticBootstrap_Init(&context, &config) ==
          DOMAIN_UART_DIAGNOSTIC_RUNNING);
    CHECK(initCalls == 1U && lastConfig.enabled);
    CHECK(lastConfig.domain_boot_id == 1U && lastConfig.port_epoch == 1U);
    CHECK(lastConfig.period_ms == 250U);
    CHECK(DomainUartDiagnosticBootstrap_Poll(&context, 0xFFFFFFF0U) ==
          DOMAIN_UART_DIAGNOSTIC_RUNNING);
    CHECK(stepCalls == 1U && lastNow == 0xFFFFFFF0U);
    CHECK(DomainUartDiagnosticBootstrap_Stop(&context) ==
          DOMAIN_UART_DIAGNOSTIC_STOPPED);
    CHECK(stopCalls == 1U);
    return 0;
}

static int TestStepFailureStopsOnce(void)
{
    DomainUartDiagnosticBootstrap_t context = {0};
    DomainUartDiagnosticBootstrap_Config_t config = Config(true);
    ResetFakes();
    CHECK(DomainUartDiagnosticBootstrap_Init(&context, &config) ==
          DOMAIN_UART_DIAGNOSTIC_RUNNING);
    stepResult = NET_HOLD;
    CHECK(DomainUartDiagnosticBootstrap_Poll(&context, 250U) ==
          DOMAIN_UART_DIAGNOSTIC_FAULT);
    CHECK(stopCalls == 1U);
    CHECK(DomainUartDiagnosticBootstrap_Poll(&context, 500U) ==
          DOMAIN_UART_DIAGNOSTIC_FAULT);
    CHECK(stepCalls == 1U && stopCalls == 1U);
    return 0;
}

static int TestStopFailureLatchesFault(void)
{
    DomainUartDiagnosticBootstrap_t context = {0};
    DomainUartDiagnosticBootstrap_Config_t config = Config(true);
    ResetFakes();
    CHECK(DomainUartDiagnosticBootstrap_Init(&context, &config) ==
          DOMAIN_UART_DIAGNOSTIC_RUNNING);
    stopResult = NET_HOLD;
    CHECK(DomainUartDiagnosticBootstrap_Stop(&context) ==
          DOMAIN_UART_DIAGNOSTIC_FAULT);
    CHECK(stopCalls == 1U);
    CHECK(DomainUartDiagnosticBootstrap_Stop(&context) ==
          DOMAIN_UART_DIAGNOSTIC_FAULT);
    CHECK(stopCalls == 1U);
    return 0;
}

static int TestInitFailureNoRestart(void)
{
    DomainUartDiagnosticBootstrap_t context = {0};
    DomainUartDiagnosticBootstrap_Config_t config = Config(true);
    ResetFakes();
    initResult = NET_HOLD;
    CHECK(DomainUartDiagnosticBootstrap_Init(&context, &config) ==
          DOMAIN_UART_DIAGNOSTIC_FAULT);
    CHECK(initCalls == 1U && stopCalls == 0U);
    CHECK(DomainUartDiagnosticBootstrap_Init(&context, &config) ==
          DOMAIN_UART_DIAGNOSTIC_ALREADY);
    CHECK(initCalls == 1U);
    return 0;
}

int main(void)
{
    int result = 0;
    result |= TestDisabled();
    result |= TestApprovedAndValues();
    result |= TestStepFailureStopsOnce();
    result |= TestStopFailureLatchesFault();
    result |= TestInitFailureNoRestart();
    return result;
}
