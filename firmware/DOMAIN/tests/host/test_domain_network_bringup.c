#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domain_can0_transceiver.h"
#include "domain_network_bringup.h"
#include "network/app/net_if_entry.h"

static bool g_controlAvailable;
static bool g_normalSucceeds;
static bool g_standbySucceeds;
static unsigned g_normalCalls;
static unsigned g_standbyCalls;
static DomainCan0Transceiver_Mode_t g_mode;
static NetHardwareInitStatus g_networkStatus;
static uint32_t g_networkBootId;

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
    g_normalSucceeds = true;
    g_standbySucceeds = true;
    g_normalCalls = 0U;
    g_standbyCalls = 0U;
    g_mode = DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY;
    g_networkStatus = NET_HARDWARE_INIT_NOT_INITIALIZED;
    g_networkBootId = 7U;
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
    if (!g_controlAvailable || !g_normalSucceeds)
    {
        return DOMAIN_CAN0_TRANSCEIVER_NOT_INITIALIZED;
    }
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

NetHardwareInitStatus Net_GetHardwareInitStatus(void)
{
    return g_networkStatus;
}

uint32_t Net_GetBootId(void)
{
    return g_networkBootId;
}

static int PrepareCoordinator(DomainNetworkBringup_t *context, bool approved)
{
    (void)memset(context, 0, sizeof(*context));
    CHECK(DomainNetworkBringup_Init(context, approved) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    CHECK(DomainNetworkBringup_EnterStandby(context) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    return 0;
}

static int PrepareNetwork(DomainNetworkBringup_t *context)
{
    CHECK(PrepareCoordinator(context, true) == 0);
    CHECK(DomainNetworkBringup_BeginNetworkInitialization(context) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    g_networkStatus = NET_HARDWARE_INIT_PREPARED;
    CHECK(DomainNetworkBringup_ObserveNetworkStatus(context) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    return 0;
}

static int TestStandbyRegression(void)
{
    DomainNetworkBringup_t context;
    ResetFakes();
    CHECK(PrepareCoordinator(&context, false) == 0);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_STANDBY);
    CHECK(g_normalCalls == 0U);
    CHECK(g_standbyCalls == 0U);
    return 0;
}

static int TestNormalPathAndDuplicate(void)
{
    DomainNetworkBringup_t context;
    ResetFakes();
    CHECK(PrepareNetwork(&context) == 0);
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 8U, 3U, 100U, 200U) ==
          DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT);
    CHECK(g_normalCalls == 0U);
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 3U, 100U, 200U) == DOMAIN_NETWORK_BRINGUP_OK);
    CHECK(g_normalCalls == 1U);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED);
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 3U, 101U, 201U) == DOMAIN_NETWORK_BRINGUP_ALREADY);
    CHECK(g_normalCalls == 1U);
    return 0;
}

static int TestApprovalAndPreparedGates(void)
{
    DomainNetworkBringup_t context;
    ResetFakes();
    CHECK(PrepareCoordinator(&context, false) == 0);
    CHECK(DomainNetworkBringup_BeginNetworkInitialization(&context) ==
          DOMAIN_NETWORK_BRINGUP_NOT_APPROVED);
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 1U, 1U, 10U, 20U) ==
          DOMAIN_NETWORK_BRINGUP_INVALID_STATE);
    CHECK(g_normalCalls == 0U);

    ResetFakes();
    CHECK(PrepareNetwork(&context) == 0);
    g_networkStatus = NET_HARDWARE_INIT_INITIALIZING;
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 1U, 10U, 20U) ==
          DOMAIN_NETWORK_BRINGUP_INVALID_STATE);
    CHECK(g_normalCalls == 0U);
    return 0;
}

static int TestNormalFailureAndRecovery(void)
{
    DomainNetworkBringup_t context;
    ResetFakes();
    CHECK(PrepareNetwork(&context) == 0);
    g_normalSucceeds = false;
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 1U, 10U, 20U) ==
          DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NORMAL_REQUEST_FAILED);
    CHECK(g_normalCalls == 1U);
    CHECK(g_standbyCalls == 1U);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT);
    CHECK(DomainNetworkBringup_GetFault(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_NORMAL_REQUEST);
    CHECK(DomainNetworkBringup_WasStandbyRecoveryAttempted(&context));
    CHECK(!DomainNetworkBringup_DidStandbyRecoveryFail(&context));
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 1U, 11U, 21U) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED);
    CHECK(g_normalCalls == 1U);
    return 0;
}

static int TestRecoveryFailureRecorded(void)
{
    DomainNetworkBringup_t context;
    ResetFakes();
    CHECK(PrepareNetwork(&context) == 0);
    g_normalSucceeds = false;
    g_standbySucceeds = false;
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 1U, 10U, 20U) ==
          DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NORMAL_REQUEST_FAILED);
    CHECK(DomainNetworkBringup_DidStandbyRecoveryFail(&context));
    CHECK(g_standbyCalls == 1U);
    return 0;
}

static int TestNetworkFailuresReturnStandby(void)
{
    DomainNetworkBringup_t context;
    ResetFakes();
    CHECK(PrepareCoordinator(&context, true) == 0);
    CHECK(DomainNetworkBringup_BeginNetworkInitialization(&context) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    g_networkStatus = NET_HARDWARE_INIT_FAILED;
    CHECK(DomainNetworkBringup_ObserveNetworkStatus(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED);
    CHECK(g_standbyCalls == 1U);
    CHECK(DomainNetworkBringup_GetFault(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);

    ResetFakes();
    CHECK(PrepareCoordinator(&context, true) == 0);
    CHECK(DomainNetworkBringup_BeginNetworkInitialization(&context) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    g_networkStatus = NET_HARDWARE_INIT_ROLLBACK_FAILED;
    CHECK(DomainNetworkBringup_ObserveNetworkStatus(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED);
    CHECK(g_standbyCalls == 1U);
    CHECK(DomainNetworkBringup_GetFault(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_ROLLBACK);
    return 0;
}

static DomainNetworkBringup_ActiveEvidence_t ValidShapedEvidence(void)
{
    DomainNetworkBringup_ActiveEvidence_t evidence;
    evidence.source = DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CAN_RX_ACCEPTED;
    evidence.networkBootId = 7U;
    evidence.portEpoch = 3U;
    evidence.observedAtMs = 110U;
    evidence.expiresAtMs = 150U;
    return evidence;
}

static int TestActiveEvidenceFailClosed(void)
{
    DomainNetworkBringup_t context;
    DomainNetworkBringup_ActiveEvidence_t evidence;
    ResetFakes();
    CHECK(PrepareNetwork(&context) == 0);
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 3U, 100U, 200U) == DOMAIN_NETWORK_BRINGUP_OK);

    evidence = ValidShapedEvidence();
    evidence.portEpoch = 4U;
    CHECK(DomainNetworkBringup_ConfirmActive(&context, &evidence, 120U) ==
          DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_INVALID);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED);

    evidence = ValidShapedEvidence();
    evidence.observedAtMs = 130U;
    CHECK(DomainNetworkBringup_ConfirmActive(&context, &evidence, 120U) ==
          DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_INVALID);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED);

    evidence = ValidShapedEvidence();
    CHECK(DomainNetworkBringup_ConfirmActive(&context, &evidence, 150U) ==
          DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_EXPIRED);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED);

    CHECK(DomainNetworkBringup_ConfirmActive(&context, &evidence, 120U) ==
          DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CONTRACT_OPEN);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED);
    return 0;
}

static int TestEvidenceTimeoutAndLateSuccess(void)
{
    DomainNetworkBringup_t context;
    DomainNetworkBringup_ActiveEvidence_t evidence;
    ResetFakes();
    CHECK(PrepareNetwork(&context) == 0);
    CHECK(DomainNetworkBringup_RequestNormal(
              &context, 7U, 3U, UINT32_MAX - 5U, 5U) ==
          DOMAIN_NETWORK_BRINGUP_OK);
    CHECK(DomainNetworkBringup_CheckActiveEvidenceDeadline(&context, 4U) ==
          DOMAIN_NETWORK_BRINGUP_PENDING);
    CHECK(DomainNetworkBringup_CheckActiveEvidenceDeadline(&context, 5U) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED);
    CHECK(g_standbyCalls == 1U);
    CHECK(DomainNetworkBringup_GetFault(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_ACTIVE_EVIDENCE_TIMEOUT);

    evidence = ValidShapedEvidence();
    CHECK(DomainNetworkBringup_ConfirmActive(&context, &evidence, 6U) ==
          DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED);
    CHECK(DomainNetworkBringup_GetState(&context) ==
          DOMAIN_NETWORK_BRINGUP_FAULT);
    CHECK(g_standbyCalls == 1U);
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += TestStandbyRegression();
    failures += TestNormalPathAndDuplicate();
    failures += TestApprovalAndPreparedGates();
    failures += TestNormalFailureAndRecovery();
    failures += TestRecoveryFailureRecorded();
    failures += TestNetworkFailuresReturnStandby();
    failures += TestActiveEvidenceFailClosed();
    failures += TestEvidenceTimeoutAndLateSuccess();
    if (failures == 0)
    {
        (void)printf("domain_network_bringup: 8/8 PASS\n");
    }
    return failures;
}
