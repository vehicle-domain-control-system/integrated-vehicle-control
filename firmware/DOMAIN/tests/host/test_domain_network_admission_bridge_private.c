#include "domain_network_admission_bridge_private.h"

/* Reuse the 28B scenario suite verbatim against the Production-private
 * implementation. This keeps Host-only and Production behavior comparable
 * without duplicating the test vectors. */
#define DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_H
#define DomainNetworkAdmissionBridgeHost_t \
    DomainNetworkAdmissionBridgePrivate_t
#define DomainNetworkAdmissionBridgeHost_Init \
    DomainNetworkAdmissionBridgePrivate_Init
#define DomainNetworkAdmissionBridgeHost_BindApi \
    DomainNetworkAdmissionBridgePrivate_BindApi
#define DomainNetworkAdmissionBridgeHost_WrapOwner \
    DomainNetworkAdmissionBridgePrivate_WrapOwner
#define DomainNetworkAdmissionBridgeHost_OnLinkDown \
    DomainNetworkAdmissionBridgePrivate_OnLinkDown
#define DomainNetworkAdmissionBridgeHost_OnPortEpochChanged \
    DomainNetworkAdmissionBridgePrivate_OnPortEpochChanged
#define DomainNetworkAdmissionBridgeHost_OnNetworkBootChanged \
    DomainNetworkAdmissionBridgePrivate_OnNetworkBootChanged
#define DomainNetworkAdmissionBridgeHost_ResetBinding \
    DomainNetworkAdmissionBridgePrivate_ResetBinding
#define DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK \
    DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK
#define main DomainNetworkAdmissionBridgePrivate_ScenarioMain
#include "test_domain_network_admission_bridge.c"
#undef main

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "default_disabled") == 0))
    {
        DomainNetworkAdmissionBridgePrivate_t bridge;

        (void)memset(&bridge, 0, sizeof(bridge));
        CHECK(!DomainNetworkAdmissionBridgePrivate_IsEnabled(&bridge));
        (void)printf("Domain admission bridge default_disabled PASS\n");
        return 0;
    }
    if ((argc == 2) && (strcmp(argv[1], "passive_query") == 0))
    {
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) ==
              DOMAIN_IF_NOT_AVAILABLE);
        CHECK(TestStart(true, 100U) == 0);
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) ==
              DOMAIN_IF_NOT_AVAILABLE);
        CHECK(TestDeliverContext(TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH,
                                 1U, 100U, true) == NET_OK);
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) ==
              DOMAIN_IF_OK);
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE + 1U,
                                                    TEST_SESSION) ==
              DOMAIN_IF_REJECTED);
        g_provider.p6Available = false;
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) ==
              DOMAIN_IF_REJECTED);
        (void)printf("Domain passive query validation PASS\n");
        return 0;
    }
    return DomainNetworkAdmissionBridgePrivate_ScenarioMain(argc, argv);
}
