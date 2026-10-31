#include <stdio.h>
#include <string.h>

#include "domain_network_transition_binding.h"

static int g_failures;
static int g_calls;
static DomainRuntime_t g_runtime;
static DomainCisEnvironmentPriv_t g_environment;
static DomainCisOccupantPriv_t g_occupant;
static DomainCisEnvironmentPriv_Status_t g_environmentStatus;
static DomainCisOccupantPriv_Status_t g_occupantStatus;
static DomainCisEnvironmentPriv_TransitionReceipt_t g_environmentReceipt;
static DomainCisOccupantPriv_TransitionReceipt_t g_occupantReceipt;
static int g_environmentAvailable;
static int g_occupantAvailable;

#define CHECK(condition) do { if (!(condition)) { \
    ++g_failures; printf("FAIL:%d: %s\n", __LINE__, #condition); \
} } while (0)

DomainCisEnvironmentPriv_t *DomainRuntime_GetCisEnvironmentPrivate(
    DomainRuntime_t *runtime)
{
    return runtime == &g_runtime && g_environmentAvailable ? &g_environment : NULL;
}

DomainCisOccupantPriv_t *DomainRuntime_GetCisOccupantPrivate(
    DomainRuntime_t *runtime)
{
    return runtime == &g_runtime && g_occupantAvailable ? &g_occupant : NULL;
}

DomainCisEnvironmentPriv_Status_t
DomainCisEnvironmentPriv_ApplyBootTransition(
    DomainCisEnvironmentPriv_t *context,
    const DomainCisEnvironmentPriv_Transition_t *transition,
    DomainCisEnvironmentPriv_TransitionReceipt_t *outReceipt)
{
    CHECK(context == &g_environment);
    CHECK(transition != NULL);
    CHECK(outReceipt != NULL);
    ++g_calls;
    *outReceipt = g_environmentReceipt;
    return g_environmentStatus;
}

DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_ApplyBootTransition(
    DomainCisOccupantPriv_t *context,
    const DomainCisOccupantPriv_Transition_t *transition,
    DomainCisOccupantPriv_TransitionReceipt_t *outReceipt)
{
    CHECK(context == &g_occupant);
    CHECK(transition != NULL);
    CHECK(outReceipt != NULL);
    ++g_calls;
    *outReceipt = g_occupantReceipt;
    return g_occupantStatus;
}

static void reset(void)
{
    (void)memset(&g_environmentReceipt, 0, sizeof(g_environmentReceipt));
    (void)memset(&g_occupantReceipt, 0, sizeof(g_occupantReceipt));
    g_calls = 0;
    g_environmentAvailable = 1;
    g_occupantAvailable = 1;
    g_environmentStatus = DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED;
    g_occupantStatus = DOMAIN_CIS_OCCUPANT_PRIV_APPLIED;
}

static int is_zero(const NetDomainC2TransitionReceipt *receipt)
{
    return receipt->id == 0u && receipt->epoch == 0u &&
           receipt->revision == 0u;
}

static void test_arguments_and_unavailable(void)
{
    NetDomainC2TransitionBinding binding =
        domain_network_transition_binding(&g_runtime);
    NetDomainC2TransitionReceipt receipt = { 9u, 8u, 7u };

    reset();
    CHECK(binding.environment(binding.context, 1u, 1u, NULL) ==
          NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT);
    CHECK(g_calls == 0);

    g_environmentAvailable = 0;
    CHECK(binding.environment(binding.context, 1u, 1u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_UNAVAILABLE);
    CHECK(is_zero(&receipt));
    CHECK(g_calls == 0);

    receipt.id = 9u;
    receipt.epoch = 8u;
    receipt.revision = 7u;
    CHECK(binding.environment(NULL, 1u, 1u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_UNAVAILABLE);
    CHECK(is_zero(&receipt));

    g_occupantAvailable = 0;
    receipt.id = 9u;
    receipt.epoch = 8u;
    receipt.revision = 7u;
    CHECK(binding.occupant(binding.context, 2u, 1u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_UNAVAILABLE);
    CHECK(is_zero(&receipt));
}

static void test_environment_receipt(void)
{
    NetDomainC2TransitionBinding binding =
        domain_network_transition_binding(&g_runtime);
    NetDomainC2TransitionReceipt receipt = { 0u, 0u, 0u };

    reset();
    g_environmentReceipt.admittedCisBootId = 11u;
    g_environmentReceipt.transitionEpoch = 3u;
    g_environmentReceipt.domainBarrierRevision = 4u;
    g_environmentReceipt.environmentInvalidated = true;
    CHECK(binding.environment(binding.context, 11u, 3u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_APPLIED);
    CHECK(receipt.id == 11u && receipt.epoch == 3u && receipt.revision == 4u);

    g_environmentStatus = DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED;
    CHECK(binding.environment(binding.context, 11u, 3u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED);

    g_environmentReceipt.admittedCisBootId = 12u;
    CHECK(binding.environment(binding.context, 11u, 3u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED);
    CHECK(is_zero(&receipt));
}

static void test_occupant_receipt_and_failure(void)
{
    NetDomainC2TransitionBinding binding =
        domain_network_transition_binding(&g_runtime);
    NetDomainC2TransitionReceipt receipt = { 0u, 0u, 0u };

    reset();
    g_occupantReceipt.admittedSourceGeneration = 21u;
    g_occupantReceipt.transitionEpoch = 2u;
    g_occupantReceipt.domainBarrierRevision = 5u;
    g_occupantReceipt.occupantInvalidated = true;
    CHECK(binding.occupant(binding.context, 21u, 2u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_APPLIED);
    CHECK(receipt.id == 21u && receipt.epoch == 2u && receipt.revision == 5u);

    g_occupantReceipt.domainBarrierRevision = 0u;
    CHECK(binding.occupant(binding.context, 21u, 2u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED);
    CHECK(is_zero(&receipt));

    g_occupantStatus = DOMAIN_CIS_OCCUPANT_PRIV_STALE;
    CHECK(binding.occupant(binding.context, 21u, 2u, &receipt) ==
          NET_DOMAIN_C2_TRANSITION_STALE);
    CHECK(is_zero(&receipt));
}

int main(void)
{
    test_arguments_and_unavailable();
    test_environment_receipt();
    test_occupant_receipt_and_failure();
    if (g_failures == 0) {
        printf("domain_network_transition_binding: PASS\n");
    }
    return g_failures == 0 ? 0 : 1;
}
