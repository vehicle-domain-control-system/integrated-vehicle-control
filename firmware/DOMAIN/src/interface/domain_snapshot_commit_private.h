#ifndef DOMAIN_SNAPSHOT_COMMIT_PRIVATE_H
#define DOMAIN_SNAPSHOT_COMMIT_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

/* Private C2 commit notification, serialized Domain owner only.
 * A NULL value invalidates prior evidence.
 * Notify is issued only at the actual Manager commit/invalidation boundary.
 * No receipt or source metadata is manufactured by this contract. */
typedef void (*DomainSnapshotCommit_Observer_t)(
    uint16_t canId, const void *value, size_t size);
typedef void (*DomainSnapshotCommit_Publisher_t)(uint32_t nowMs);

void DomainSnapshotCommit_Bind(DomainSnapshotCommit_Observer_t observer,
                               DomainSnapshotCommit_Publisher_t publisher);
void DomainSnapshotCommit_Notify(uint16_t canId, const void *value, size_t size);
void DomainSnapshotCommit_Poll(uint32_t nowMs);
/* Optional existing-context revalidation before pending state publication. */
void DomainSnapshotCommit_SetContextPoll(DomainSnapshotCommit_Publisher_t poll);

#endif
