#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domain_can0_transceiver.h"
#include "domain_network_bootstrap.h"
#include "network/app/net_if_entry.h"

static bool g_controlAvailable;
static bool g_standbySucceeds;
static unsigned g_standbyCalls;
static unsigned g_normalCalls;
static unsigned g_netInitCalls;
static unsigned g_netMainCalls;
static unsigned g_netFlushCalls;
static DomainCan0Transceiver_Mode_t g_mode;
static NetHardwareInitStatus g_networkStatus;

#define CHECK(condition_)                                                     \
    do                                                                        \
    {                                                                         \
        if (!(condition_))                                                     \
        {                                                                     \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n",                        \
                          __FILE__, __LINE__, #condition_);                    \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static void ResetFakes(void)
{
    g_controlAvailable = true;
    g_standbySucceeds = true;
    g_standbyCalls = 0U;
    g_normalCalls = 0U;
    g_netInitCalls = 0U;
    g_netMainCalls = 0U;
    g_netFlushCalls = 0U;
    g_mode = DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY;
    g_networkStatus = NET_HARDWARE_INIT_NOT_INITIALIZED;
}

DomainCan0Transceiver_Status_t DomainCan0Transceiver_InitAfterPort(void)
{
    g_controlAvailable = true;
    g_mode = DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY;
    return DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED;
}

DomainCan0Transceiver_Status_t DomainCan0Transceiver_RequestStandby(void)
{
    ++g_standbyCalls;
    if (!g_controlAvailable || !g_standbySucceeds)
    {
        return DOMAIN_CAN0_TRANSCEIVER_NOT_INITIALIZED;
    }
    g_mode = DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY;
    return DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED;
}

DomainCan0Transceiver_Status_t DomainCan0Transceiver_RequestNormal(void)
{
    ++g_normalCalls;
    g_mode = DOMAIN_CAN0_TRANSCEIVER_MODE_NORMAL;
    return DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED;
}

bool DomainCan0Transceiver_IsControlAvailable(void)
{
    return g_controlAvailable;
}

DomainCan0Transceiver_Mode_t DomainCan0Transceiver_GetLastRequestedMode(void)
{
    return g_mode;
}

void Net_Init(void)
{
    ++g_netInitCalls;
}

void Net_MainFunction(uint32_t nowMs)
{
    (void)nowMs;
    ++g_netMainCalls;
}

void Net_TxFlush(uint32_t nowMs)
{
    (void)nowMs;
    ++g_netFlushCalls;
}

NetHardwareInitStatus Net_GetHardwareInitStatus(void)
{
    return g_networkStatus;
}

uint32_t Net_GetBootId(void)
{
    return 7U;
}

static int Prepare(
    DomainNetworkBringup_t *bringup,
    DomainNetworkBootstrap_t *bootstrap,
    bool approved)
{
    (void)memset(bringup, 0, sizeof(*bringup));
    (void)memset(bootstrap, 0, sizeof(*bootstrap));
    CHECK(DomainNetworkBringup_Init(bringup, approved) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    CHECK(DomainNetworkBringup_EnterStandby(bringup) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    CHECK(DomainNetworkBootstrap_Init(bootstrap, bringup) ==
          DOMAIN_NETWORK_BOOTSTRAP_READY);
    return 0;
}

static int TestGateFalsePreservesStandby(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, false) == 0);
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, false) ==
          DOMAIN_NETWORK_BOOTSTRAP_DEFERRED_HARDWARE_GATE);
    CHECK(g_netInitCalls == 0U);
    CHECK(g_normalCalls == 0U);
    CHECK(g_netMainCalls == 0U);
    CHECK(g_netFlushCalls == 0U);
    CHECK(g_mode == DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY);
    CHECK(DomainNetworkBringup_GetState(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_STANDBY);
    return 0;
}

static int TestGateCannotOverrideApproval(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, false) == 0);
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_NOT_APPROVED);
    CHECK(g_netInitCalls == 0U);
    CHECK(g_normalCalls == 0U);
    CHECK(DomainNetworkBringup_GetState(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_STANDBY);
    return 0;
}

static int TestOutOfOrderStartLatchesFault(void)
{
    DomainNetworkBringup_t bringup = {0};
    DomainNetworkBootstrap_t bootstrap = {0};
    ResetFakes();
    CHECK(DomainNetworkBringup_Init(&bringup, true) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    CHECK(DomainNetworkBootstrap_Init(&bootstrap, &bringup) ==
          DOMAIN_NETWORK_BOOTSTRAP_READY);
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_FAULT);
    CHECK(g_netInitCalls == 0U);
    CHECK(g_normalCalls == 0U);
    CHECK(g_standbyCalls == 1U);
    CHECK(DomainNetworkBringup_GetState(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_FAULT);
    return 0;
}

static int TestInitializingOneShot(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, true) == 0);
    g_networkStatus = NET_HARDWARE_INIT_INITIALIZING;
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING);
    CHECK(g_netInitCalls == 1U);
    CHECK(g_normalCalls == 0U);
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_ALREADY);
    CHECK(g_netInitCalls == 1U);
    CHECK(DomainNetworkBootstrap_Poll(&bootstrap) ==
          DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING);
    CHECK(g_normalCalls == 0U);
    return 0;
}

static int TestPreparedBlocksNormal(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, true) == 0);
    g_networkStatus = NET_HARDWARE_INIT_PREPARED;
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_PREPARED_NORMAL_BLOCKED);
    CHECK(g_netInitCalls == 1U);
    CHECK(g_normalCalls == 0U);
    CHECK(DomainNetworkBringup_GetState(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED);
    CHECK(DomainNetworkBootstrap_Poll(&bootstrap) ==
          DOMAIN_NETWORK_BOOTSTRAP_ALREADY);
    CHECK(g_netInitCalls == 1U);
    return 0;
}

static int TestFailedAndRollbackFailed(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, true) == 0);
    g_networkStatus = NET_HARDWARE_INIT_FAILED;
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_FAULT);
    CHECK(DomainNetworkBringup_GetFault(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
    CHECK(g_standbyCalls == 1U);
    CHECK(g_normalCalls == 0U);

    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, true) == 0);
    g_networkStatus = NET_HARDWARE_INIT_ROLLBACK_FAILED;
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_FAULT);
    CHECK(DomainNetworkBringup_GetFault(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_ROLLBACK);
    CHECK(g_standbyCalls == 1U);
    CHECK(g_normalCalls == 0U);
    return 0;
}

static int TestDelayedPreparedAfterFaultIsIgnored(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, true) == 0);
    g_networkStatus = NET_HARDWARE_INIT_FAILED;
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_FAULT);
    g_networkStatus = NET_HARDWARE_INIT_PREPARED;
    CHECK(DomainNetworkBootstrap_Poll(&bootstrap) ==
          DOMAIN_NETWORK_BOOTSTRAP_ALREADY);
    CHECK(DomainNetworkBringup_GetState(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_FAULT);
    CHECK(g_netInitCalls == 1U);
    CHECK(g_normalCalls == 0U);
    CHECK(g_standbyCalls == 1U);
    return 0;
}

static int TestExternalInitializationTimeoutFailsSafe(void)
{
    DomainNetworkBringup_t bringup;
    DomainNetworkBootstrap_t bootstrap;
    ResetFakes();
    CHECK(Prepare(&bringup, &bootstrap, true) == 0);
    g_networkStatus = NET_HARDWARE_INIT_INITIALIZING;
    CHECK(DomainNetworkBootstrap_Start(&bootstrap, true) ==
          DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING);
    CHECK(DomainNetworkBootstrap_ReportInitializationTimeout(&bootstrap) ==
          DOMAIN_NETWORK_BOOTSTRAP_FAULT);
    CHECK(DomainNetworkBringup_GetFault(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
    CHECK(g_standbyCalls == 1U);
    CHECK(g_normalCalls == 0U);
    g_networkStatus = NET_HARDWARE_INIT_PREPARED;
    CHECK(DomainNetworkBootstrap_Poll(&bootstrap) ==
          DOMAIN_NETWORK_BOOTSTRAP_ALREADY);
    CHECK(DomainNetworkBringup_GetState(&bringup) ==
          DOMAIN_NETWORK_BRINGUP_FAULT);
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += TestGateFalsePreservesStandby();
    failures += TestGateCannotOverrideApproval();
    failures += TestOutOfOrderStartLatchesFault();
    failures += TestInitializingOneShot();
    failures += TestPreparedBlocksNormal();
    failures += TestFailedAndRollbackFailed();
    failures += TestDelayedPreparedAfterFaultIsIgnored();
    failures += TestExternalInitializationTimeoutFailsSafe();
    if (failures == 0)
    {
        (void)printf("domain_network_bootstrap: 8/8 PASS\n");
    }
    return failures;
}
