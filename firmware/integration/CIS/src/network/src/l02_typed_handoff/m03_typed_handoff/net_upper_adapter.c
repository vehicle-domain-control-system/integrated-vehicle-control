#include "network/net_upper.h"
NetStatus net_upper_deliver_can(const NetUpperOwner *owner,
    const NetUpperCanMessage *message, const NetUpperDelivery *delivery)
{
    if (owner == NULL || message == NULL || delivery == NULL) return NET_INVALID_ARGUMENT;
    if (message->id == 0x0a0u && !delivery->freshness_known) return NET_HOLD;
    if (message->id == 0x0a0u && owner->rear != NULL) {
        const NetWireCisRearRaw *raw = &message->data.cis_rear;
        NetUpperRear rear;
        rear.boot_id=raw->boot_id; rear.tx_sequence=raw->tx_sequence;
        rear.update_sequence=raw->update_sequence;
        rear.rear_distance_mm=raw->rear_distance_deci_cm;
        rear.validity=raw->validity; rear.quality_reason=raw->quality_reason;
        rear.proximity_status=raw->proximity_status; rear.source_age_ms=delivery->age_ms;
        rear.distance_available=raw->validity==1u && raw->quality_reason==0u &&
            raw->proximity_status==0u && raw->rear_distance_deci_cm!=UINT16_MAX;
        return owner->rear(owner->context,&rear,delivery);
    }
    return owner->can == NULL ? NET_HOLD : owner->can(owner->context,message,delivery);
}
