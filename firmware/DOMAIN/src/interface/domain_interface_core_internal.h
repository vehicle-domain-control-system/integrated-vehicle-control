#ifndef DOMAIN_INTERFACE_CORE_INTERNAL_H
#define DOMAIN_INTERFACE_CORE_INTERNAL_H

#include <stdbool.h>

#include "Domain_Interface_v0.5_C2_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private post-commit notification used by Application Rx bindings.
 * The Core-owned observer table remains encapsulated and is never exposed by
 * pointer. A missing observer/callback is a successful no-op.
 */
DomainIf_Status_t DomainIfCore_NotifyBcmCommandResult(
    const DomainIf_BcmCommandResult_t *result);

/*
 * Shared validation entry points for Application-owned Rx bindings.  These
 * keep the public Rx and Tx paths on one C2 validation contract without
 * publishing another DomainIf API.
 */
bool DomainIfCore_IsDoorStateValid(const DoorState_t *state);
bool DomainIfCore_IsBcmStatusValid(
    const DomainIf_BcmStatusReport_t *state);
bool DomainIfCore_IsCisRearValid(const CisRearState_t *state);
bool DomainIfCore_IsCisStatusValid(
    const DomainIf_CisStatusReport_t *state);

/* Optional post-commit notifications for callbacks present in C2 v0.5. */
DomainIf_Status_t DomainIfCore_NotifyBcmStatus(
    const DomainIf_BcmStatusReport_t *status);
DomainIf_Status_t DomainIfCore_NotifyCisStatus(
    const DomainIf_CisStatusReport_t *status);

/*
 * Test-only inspection hooks for the private lifecycle context.
 *
 * Production code must use only the published DomainIf_* API. These hooks are
 * compiled only by the host lifecycle test and return value copies so tests do
 * not gain mutable access to the Core-owned callback tables.
 */
#if defined(DOMAIN_IF_CORE_TEST)
bool DomainIfCore_TestHasRxObserver(void);
bool DomainIfCore_TestHasTxPort(void);
void DomainIfCore_TestCopyRxObserver(DomainIf_RxObserver_t *outObserver);
void DomainIfCore_TestCopyTxPort(DomainIf_TxPort_t *outPort);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_CORE_INTERNAL_H */
