#include "domain_cis_occupant_private.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_tests;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static CisOccupantManager_Policy_t Policy(void)
{
    CisOccupantManager_Policy_t policy = {100U, 200U};
    return policy;
}

static DomainCisOccupantPriv_Transition_t Transition(
    SourceGeneration_t generation,
    uint64_t epoch)
{
    DomainCisOccupantPriv_Transition_t transition;
    transition.admittedSourceGeneration = generation;
    transition.transitionEpoch = epoch;
    return transition;
}

static void BeginTest(const char *name)
{
    ++g_tests;
    (void)printf("%s: ", name);
}

static void EndTest(int before)
{
    (void)printf((g_failures == before) ? "PASS\n" : "FAIL\n");
}

static void Test_FirstTransitionAndDuplicate(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Policy_t policy = Policy();
    DomainCisOccupantPriv_t context;
    DomainCisOccupantPriv_TransitionReceipt_t firstReceipt;
    DomainCisOccupantPriv_TransitionReceipt_t duplicateReceipt;
    DomainCisOccupantPriv_Transition_t transition = Transition(10U, 1U);
    int before = g_failures;

    BeginTest("Test_FirstTransitionAndDuplicate");
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(DomainCisOccupantPriv_Init(&context, &manager) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &transition, &firstReceipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
    CHECK(firstReceipt.occupantInvalidated);
    CHECK(manager.invalidated);
    CHECK(manager.admittedSourceGeneration == 10U);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &transition, &duplicateReceipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED);
    CHECK(memcmp(&firstReceipt, &duplicateReceipt,
                 sizeof(firstReceipt)) == 0);
    EndTest(before);
}

static void Test_ConflictStaleAndSkippedEpoch(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Policy_t policy = Policy();
    DomainCisOccupantPriv_t context;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    DomainCisOccupantPriv_Transition_t first = Transition(10U, 1U);
    DomainCisOccupantPriv_Transition_t conflict = Transition(11U, 1U);
    DomainCisOccupantPriv_Transition_t skipped = Transition(12U, 3U);
    DomainCisOccupantPriv_Transition_t second = Transition(12U, 2U);
    int before = g_failures;

    BeginTest("Test_ConflictStaleAndSkippedEpoch");
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(DomainCisOccupantPriv_Init(&context, &manager) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &first, &receipt) == DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &conflict, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_ID_CONFLICT);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &skipped, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &second, &receipt) == DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &first, &receipt) == DOMAIN_CIS_OCCUPANT_PRIV_STALE);
    EndTest(before);
}

static void Test_InvalidationFailureLatchesBlocked(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Policy_t policy = Policy();
    DomainCisOccupantPriv_t context;
    DomainCisOccupantPriv_Snapshot_t snapshot;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    DomainCisOccupantPriv_Transition_t transition = Transition(20U, 1U);
    int before = g_failures;

    BeginTest("Test_InvalidationFailureLatchesBlocked");
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(DomainCisOccupantPriv_Init(&context, &manager) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    manager.initialized = false;
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED);
    CHECK(DomainCisOccupantPriv_GetSnapshot(&context, &snapshot) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    CHECK(snapshot.state == DOMAIN_CIS_OCCUPANT_PRIV_STATE_FAULT_BLOCKED);
    CHECK(!snapshot.newDataAdmissionAllowed);
    CHECK(!snapshot.hasAppliedTransition);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED);
    EndTest(before);
}

static void Test_MaximumEpochNeverWraps(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Policy_t policy = Policy();
    DomainCisOccupantPriv_t context;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    DomainCisOccupantPriv_Transition_t maximum =
        Transition(31U, UINT64_MAX);
    DomainCisOccupantPriv_Transition_t wrapped = Transition(32U, 1U);
    int before = g_failures;

    BeginTest("Test_MaximumEpochNeverWraps");
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(DomainCisOccupantPriv_Init(&context, &manager) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    context.currentTransition = Transition(30U, UINT64_MAX - UINT64_C(1));
    context.hasTransitionIdentity = true;
    context.hasAppliedTransition = true;
    context.newDataAdmissionAllowed = true;
    context.state = DOMAIN_CIS_OCCUPANT_PRIV_STATE_WAITING_NEW_DATA;
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &maximum, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &wrapped, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR);
    EndTest(before);
}

static void Test_NullAndUninitializedLeaveOutputUnchanged(void)
{
    DomainCisOccupantPriv_t context;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    DomainCisOccupantPriv_TransitionReceipt_t saved;
    DomainCisOccupantPriv_Transition_t transition = Transition(40U, 1U);
    int before = g_failures;

    BeginTest("Test_NullAndUninitializedLeaveOutputUnchanged");
    (void)memset(&context, 0, sizeof(context));
    (void)memset(&receipt, 0xA5, sizeof(receipt));
    saved = receipt;
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_NOT_INITIALIZED);
    CHECK(memcmp(&receipt, &saved, sizeof(receipt)) == 0);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              NULL, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, NULL, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &context, &transition, NULL) ==
          DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT);
    EndTest(before);
}

int main(void)
{
    Test_FirstTransitionAndDuplicate();
    Test_ConflictStaleAndSkippedEpoch();
    Test_InvalidationFailureLatchesBlocked();
    Test_MaximumEpochNeverWraps();
    Test_NullAndUninitializedLeaveOutputUnchanged();

    (void)printf("domain_cis_occupant_private: %d tests, %d failures\n",
                 g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
