#include "domain_can0_transceiver.h"

#include "Siul2_Dio_Ip.h"

#define DOMAIN_CAN0_STB_SIUL2_INSTANCE (0U)
#define DOMAIN_CAN0_STB_GPDO_INDEX     (197U)

static bool s_domainCan0ControlAvailable = false;
static DomainCan0Transceiver_Mode_t s_domainCan0LastRequestedMode =
    DOMAIN_CAN0_TRANSCEIVER_MODE_UNKNOWN;

DomainCan0Transceiver_Status_t DomainCan0Transceiver_InitAfterPort(void)
{
    s_domainCan0ControlAvailable = false;
    s_domainCan0LastRequestedMode = DOMAIN_CAN0_TRANSCEIVER_MODE_UNKNOWN;

    Siul2_Dio_Ip_SetGPDO(
        DOMAIN_CAN0_STB_SIUL2_INSTANCE,
        DOMAIN_CAN0_STB_GPDO_INDEX);

    s_domainCan0LastRequestedMode = DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY;
    s_domainCan0ControlAvailable = true;

    return DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED;
}

DomainCan0Transceiver_Status_t DomainCan0Transceiver_RequestStandby(void)
{
    if (!s_domainCan0ControlAvailable)
    {
        return DOMAIN_CAN0_TRANSCEIVER_NOT_INITIALIZED;
    }

    Siul2_Dio_Ip_SetGPDO(
        DOMAIN_CAN0_STB_SIUL2_INSTANCE,
        DOMAIN_CAN0_STB_GPDO_INDEX);
    s_domainCan0LastRequestedMode = DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY;

    return DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED;
}

DomainCan0Transceiver_Status_t DomainCan0Transceiver_RequestNormal(void)
{
    if (!s_domainCan0ControlAvailable)
    {
        return DOMAIN_CAN0_TRANSCEIVER_NOT_INITIALIZED;
    }

    Siul2_Dio_Ip_ClearGPDO(
        DOMAIN_CAN0_STB_SIUL2_INSTANCE,
        DOMAIN_CAN0_STB_GPDO_INDEX);
    s_domainCan0LastRequestedMode = DOMAIN_CAN0_TRANSCEIVER_MODE_NORMAL;

    return DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED;
}

bool DomainCan0Transceiver_IsControlAvailable(void)
{
    return s_domainCan0ControlAvailable;
}

DomainCan0Transceiver_Mode_t DomainCan0Transceiver_GetLastRequestedMode(void)
{
    return s_domainCan0LastRequestedMode;
}
