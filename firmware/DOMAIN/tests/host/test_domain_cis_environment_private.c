#include "domain_cis_environment_private.h"

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

static ValueQuality_t NormalQuality(uint32_t sequence)
{
    ValueQuality_t quality;

    quality.validity = VALUE_VALIDITY_VALID;
    quality.reason = QUALITY_REASON_NONE;
    quality.meta.quality = DATA_QUALITY_OK;
    quality.meta.ageMs = 5U;
    quality.meta.updateSequence = sequence;
    return quality;
}

static CisCabinEnvironmentState_t NormalState(void)
{
    CisCabinEnvironmentState_t state;

    state.cabinTemperature = 2200;
    state.temperatureQuality = NormalQuality(900U);
    state.cabinHumidity = 4500U;
    state.humidityQuality = NormalQuality(901U);
    state.cabinIlluminance = 1000U;
    state.illuminanceQuality = NormalQuality(902U);
    return state;
}

static DomainCisEnvironmentPriv_Transition_t Transition(
    uint32_t bootId,
    uint64_t epoch)
{
    DomainCisEnvironmentPriv_Transition_t transition;

    transition.admittedCisBootId = bootId;
    transition.transitionEpoch = epoch;
    return transition;
}

static void BeginTest(const char *name)
{
    ++g_tests;
    (void)printf("%s: ", name);
}

static void EndTest(int failuresBefore)
{
    (void)printf((g_failures == failuresBefore) ? "PASS\n" : "FAIL\n");
}

static void Test_FirstTransitionInvalidatesBeforeReceipt(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t environment;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_Snapshot_t privateSnapshot;
    CisCabinEnvironmentState_t state = NormalState();
    DomainCisEnvironmentPriv_Transition_t transition = Transition(10U, 1U);
    int before = g_failures;

    BeginTest("Test_FirstTransitionInvalidatesBeforeReceipt");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(receipt.transitionEpoch == 1U);
    CHECK(receipt.admittedCisBootId == 10U);
    CHECK(receipt.domainBarrierRevision == 1U);
    CHECK(receipt.environmentInvalidated);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 101U, &environment) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(environment.invalidated);
    CHECK(environment.original.temperatureQuality.meta.updateSequence ==
          900U);
    CHECK(environment.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(&context, &privateSnapshot) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(privateSnapshot.state ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_WAITING_NEW_DATA);
    CHECK(privateSnapshot.newDataAdmissionAllowed);
    EndTest(before);
}

static void Test_ExactDuplicateIsIdempotent(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_t savedManager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t first;
    DomainCisEnvironmentPriv_TransitionReceipt_t second;
    DomainCisEnvironmentPriv_Transition_t transition = Transition(20U, 1U);
    int before = g_failures;

    BeginTest("Test_ExactDuplicateIsIdempotent");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, &first) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    savedManager = manager;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, &second) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED);
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
    CHECK(memcmp(&manager, &savedManager, sizeof(manager)) == 0);
    CHECK(context.domainBarrierRevision == 1U);
    EndTest(before);
}

static void Test_SameEpochDifferentBootIsConflict(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_t savedContext;
    DomainCisEnvironmentPriv_TransitionReceipt_t output;
    DomainCisEnvironmentPriv_TransitionReceipt_t sentinel;
    DomainCisEnvironmentPriv_Transition_t first = Transition(30U, 1U);
    DomainCisEnvironmentPriv_Transition_t conflict = Transition(31U, 1U);
    int before = g_failures;

    BeginTest("Test_SameEpochDifferentBootIsConflict");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &first, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    savedContext = context;
    (void)memset(&output, 0xA5, sizeof(output));
    sentinel = output;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &conflict, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_ID_CONFLICT);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(memcmp(&context, &savedContext, sizeof(context)) == 0);
    EndTest(before);
}

static void Test_SequentialEpochsTwoAndThree(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    CisCabinEnvironmentState_t state = NormalState();
    DomainCisEnvironmentPriv_Transition_t first = Transition(40U, 1U);
    DomainCisEnvironmentPriv_Transition_t second = Transition(41U, 2U);
    DomainCisEnvironmentPriv_Transition_t third = Transition(42U, 3U);
    int before = g_failures;

    BeginTest("Test_SequentialEpochsTwoAndThree");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &first, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 10U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &second, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(receipt.transitionEpoch == 2U);
    CHECK(receipt.domainBarrierRevision == 2U);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 20U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &third, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(receipt.transitionEpoch == 3U);
    CHECK(receipt.domainBarrierRevision == 3U);
    EndTest(before);
}

static void Test_PastAndSkippedEpochsAreRejected(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_t savedContext;
    DomainCisEnvironmentPriv_TransitionReceipt_t output;
    DomainCisEnvironmentPriv_TransitionReceipt_t sentinel;
    DomainCisEnvironmentPriv_Transition_t initialSkipped = Transition(42U, 2U);
    DomainCisEnvironmentPriv_Transition_t first = Transition(43U, 1U);
    DomainCisEnvironmentPriv_Transition_t past = Transition(44U, 0U);
    DomainCisEnvironmentPriv_Transition_t skipped = Transition(45U, 3U);
    int before = g_failures;

    BeginTest("Test_PastAndSkippedEpochsAreRejected");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    (void)memset(&output, 0xA5, sizeof(output));
    sentinel = output;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &initialSkipped, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &first, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    savedContext = context;
    (void)memset(&output, 0xA5, sizeof(output));
    sentinel = output;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &past, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &skipped, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(memcmp(&context, &savedContext, sizeof(context)) == 0);

    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &(DomainCisEnvironmentPriv_Transition_t){46U, 2U},
              &output) == DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    savedContext = context;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &(DomainCisEnvironmentPriv_Transition_t){47U, 1U},
              &output) == DOMAIN_CIS_ENVIRONMENT_PRIV_STALE);
    CHECK(memcmp(&context, &savedContext, sizeof(context)) == 0);
    EndTest(before);
}

static void Test_InvalidationFailureStaysBlockedWithoutImplicitRetry(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t output;
    DomainCisEnvironmentPriv_TransitionReceipt_t sentinel;
    DomainCisEnvironmentPriv_Snapshot_t snapshot;
    CisEnvironmentManager_Snapshot_t environment;
    CisCabinEnvironmentState_t state = NormalState();
    DomainCisEnvironmentPriv_Transition_t first = Transition(50U, 1U);
    DomainCisEnvironmentPriv_Transition_t failed = Transition(51U, 2U);
    DomainCisEnvironmentPriv_Transition_t next = Transition(52U, 3U);
    int before = g_failures;

    BeginTest("Test_InvalidationFailureStaysBlockedWithoutImplicitRetry");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &first, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);

    /* Simulate a manager-side fault before the next synchronous barrier. */
    manager.initialized = false;
    (void)memset(&output, 0x5A, sizeof(output));
    sentinel = output;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &failed, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(&context, &snapshot) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(snapshot.state == DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_FAULT_BLOCKED);
    CHECK(!snapshot.newDataAdmissionAllowed);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 0U, &environment) ==
          CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED);

    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &failed, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &next, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    EndTest(before);
}

static void Test_BusyAndInvalidBootAreRejected(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t output;
    DomainCisEnvironmentPriv_TransitionReceipt_t sentinel;
    DomainCisEnvironmentPriv_Transition_t invalidBoot = Transition(0U, 1U);
    DomainCisEnvironmentPriv_Transition_t invalidEpoch = Transition(60U, 0U);
    DomainCisEnvironmentPriv_Transition_t valid = Transition(60U, 1U);
    int before = g_failures;

    BeginTest("Test_BusyAndInvalidBootAreRejected");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    (void)memset(&output, 0xA5, sizeof(output));
    sentinel = output;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &invalidBoot, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &invalidEpoch, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    context.transitionInProgress = true;
    context.state = DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_TRANSITION_IN_PROGRESS;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &valid, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_BUSY);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    EndTest(before);
}

static void Test_RevisionRolloverIsTokenOnly(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_Transition_t transition = Transition(70U, 1U);
    int before = g_failures;

    BeginTest("Test_RevisionRolloverIsTokenOnly");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    context.domainBarrierRevision = UINT32_MAX;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(receipt.domainBarrierRevision == 0U);
    CHECK(context.domainBarrierRevision == 0U);
    EndTest(before);
}

static void Test_EpochMaximumNeverWraps(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_t savedContext;
    DomainCisEnvironmentPriv_TransitionReceipt_t output;
    DomainCisEnvironmentPriv_TransitionReceipt_t sentinel;
    DomainCisEnvironmentPriv_Transition_t maximum =
        Transition(72U, UINT64_MAX);
    DomainCisEnvironmentPriv_Transition_t wrapped = Transition(73U, 1U);
    int before = g_failures;

    BeginTest("Test_EpochMaximumNeverWraps");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);

    /* Seed a valid completed predecessor to exercise the numeric boundary. */
    context.currentTransition = Transition(71U, UINT64_MAX - UINT64_C(1));
    context.currentReceipt.transitionEpoch = UINT64_MAX - UINT64_C(1);
    context.currentReceipt.admittedCisBootId = 71U;
    context.currentReceipt.domainBarrierRevision = 10U;
    context.currentReceipt.environmentInvalidated = true;
    context.domainBarrierRevision = 10U;
    context.state = DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_WAITING_NEW_DATA;
    context.hasTransitionIdentity = true;
    context.hasAppliedTransition = true;
    context.newDataAdmissionAllowed = true;

    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &maximum, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(output.transitionEpoch == UINT64_MAX);
    savedContext = context;
    (void)memset(&output, 0xA5, sizeof(output));
    sentinel = output;
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &wrapped, &output) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(memcmp(&context, &savedContext, sizeof(context)) == 0);
    EndTest(before);
}

static void Test_NewBootAdmittedStoreAcceptsSequenceReset(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_Transition_t transition = Transition(74U, 1U);
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_NewBootAdmittedStoreAcceptsSequenceReset");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, UINT32_MAX - 2U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);

    state.temperatureQuality.meta.updateSequence = 0U;
    state.humidityQuality.meta.updateSequence = 0U;
    state.illuminanceQuality.meta.updateSequence = 0U;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 1U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 2U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(!snapshot.invalidated);
    CHECK(snapshot.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_OK);
    CHECK(snapshot.original.temperatureQuality.meta.updateSequence == 0U);
    CHECK(snapshot.original.humidityQuality.meta.updateSequence == 0U);
    CHECK(snapshot.original.illuminanceQuality.meta.updateSequence == 0U);
    EndTest(before);
}

static void Test_ReinitializationRequiresEpochOneAgain(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_Transition_t first = Transition(75U, 1U);
    DomainCisEnvironmentPriv_Transition_t second = Transition(76U, 2U);
    int before = g_failures;

    BeginTest("Test_ReinitializationRequiresEpochOneAgain");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &first, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);

    /* Caller contract requires Network queue/callback drain before this Init. */
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &second, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &first, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    EndTest(before);
}

static void Test_NullAndNotInitializedLeaveOutputsUnchanged(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_TransitionReceipt_t receiptSentinel;
    DomainCisEnvironmentPriv_Snapshot_t snapshot;
    DomainCisEnvironmentPriv_Snapshot_t snapshotSentinel;
    DomainCisEnvironmentPriv_Transition_t transition = Transition(80U, 1U);
    int before = g_failures;

    BeginTest("Test_NullAndNotInitializedLeaveOutputsUnchanged");
    (void)memset(&manager, 0, sizeof(manager));
    (void)memset(&context, 0, sizeof(context));
    (void)memset(&receipt, 0xA5, sizeof(receipt));
    (void)memset(&snapshot, 0x5A, sizeof(snapshot));
    receiptSentinel = receipt;
    snapshotSentinel = snapshot;
    CHECK(DomainCisEnvironmentPriv_Init(NULL, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_Init(&context, NULL) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              NULL, &transition, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_NOT_INITIALIZED);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, NULL, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &context, &transition, NULL) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(&context, &snapshot) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_NOT_INITIALIZED);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(NULL, &snapshot) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(&context, NULL) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT);
    CHECK(memcmp(&receipt, &receiptSentinel, sizeof(receipt)) == 0);
    CHECK(memcmp(&snapshot, &snapshotSentinel, sizeof(snapshot)) == 0);
    EndTest(before);
}

static void Test_PrivateGetterIsReadOnly(void)
{
    CisEnvironmentManager_t manager;
    DomainCisEnvironmentPriv_t context;
    DomainCisEnvironmentPriv_t saved;
    DomainCisEnvironmentPriv_Snapshot_t first;
    DomainCisEnvironmentPriv_Snapshot_t second;
    int before = g_failures;

    BeginTest("Test_PrivateGetterIsReadOnly");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(DomainCisEnvironmentPriv_Init(&context, &manager) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    saved = context;
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(&context, &first) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(&context, &second) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(memcmp(&context, &saved, sizeof(context)) == 0);
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
    EndTest(before);
}

int main(void)
{
    Test_FirstTransitionInvalidatesBeforeReceipt();
    Test_ExactDuplicateIsIdempotent();
    Test_SameEpochDifferentBootIsConflict();
    Test_SequentialEpochsTwoAndThree();
    Test_PastAndSkippedEpochsAreRejected();
    Test_InvalidationFailureStaysBlockedWithoutImplicitRetry();
    Test_BusyAndInvalidBootAreRejected();
    Test_RevisionRolloverIsTokenOnly();
    Test_EpochMaximumNeverWraps();
    Test_NewBootAdmittedStoreAcceptsSequenceReset();
    Test_ReinitializationRequiresEpochOneAgain();
    Test_NullAndNotInitializedLeaveOutputsUnchanged();
    Test_PrivateGetterIsReadOnly();

    (void)printf("domain_cis_environment_private: %d tests, %d failed checks\n",
                 g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
