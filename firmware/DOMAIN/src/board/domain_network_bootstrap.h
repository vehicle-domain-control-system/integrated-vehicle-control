#ifndef DOMAIN_NETWORK_BOOTSTRAP_H
#define DOMAIN_NETWORK_BOOTSTRAP_H

#include <stdbool.h>

#include "domain_network_bringup.h"

typedef enum
{
    DOMAIN_NETWORK_BOOTSTRAP_NOT_INITIALIZED = 0,
    DOMAIN_NETWORK_BOOTSTRAP_READY,
    DOMAIN_NETWORK_BOOTSTRAP_DEFERRED_HARDWARE_GATE,
    DOMAIN_NETWORK_BOOTSTRAP_NOT_APPROVED,
    DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING,
    DOMAIN_NETWORK_BOOTSTRAP_PREPARED_NORMAL_BLOCKED,
    DOMAIN_NETWORK_BOOTSTRAP_FAULT,
    DOMAIN_NETWORK_BOOTSTRAP_INVALID_STATE,
    DOMAIN_NETWORK_BOOTSTRAP_ALREADY
} DomainNetworkBootstrap_Status_t;

/* Caller-owned, single non-ISR Bootstrap state. */
typedef struct
{
    bool initialized;
    bool netInitCalled;
    DomainNetworkBringup_t *bringup;
    DomainNetworkBootstrap_Status_t status;
} DomainNetworkBootstrap_t;

DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Init(
    DomainNetworkBootstrap_t *context,
    DomainNetworkBringup_t *bringup);

/*
 * Starts Network initialization exactly once when hardwareGateOpen is true.
 * It never requests Transceiver Normal and never calls Network main/flush.
 */
DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Start(
    DomainNetworkBootstrap_t *context,
    bool hardwareGateOpen);

/*
 * Observes an asynchronous initialization after Start returned INITIALIZING.
 * No timeout is applied: the Production deadline remains BINDING_OPEN.
 */
DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Poll(
    DomainNetworkBootstrap_t *context);

/*
 * Latches an initialization fault when the external Bootstrap owner reaches
 * its approved deadline. This module deliberately defines no timeout value.
 */
DomainNetworkBootstrap_Status_t
DomainNetworkBootstrap_ReportInitializationTimeout(
    DomainNetworkBootstrap_t *context);

DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_GetStatus(
    const DomainNetworkBootstrap_t *context);

bool DomainNetworkBootstrap_WasNetInitCalled(
    const DomainNetworkBootstrap_t *context);

#endif /* DOMAIN_NETWORK_BOOTSTRAP_H */
