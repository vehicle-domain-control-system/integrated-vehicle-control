#include "bcm_result.h"

NetBcmResult BcmResult_Make(NetBcmCommandKind kind, BcmCommandRef command, NetVehicleResult result, NetReason reason)
{
    NetBcmResult msg;
    msg.command_domain_boot = command.domainBoot;
    msg.command_id = command.commandId;
    msg.command_kind = kind;
    msg.result = result;
    msg.result_reason = reason;
    msg.door_event = NET_DOOR_EVENT_NONE;
    msg.occurrence_id = 0u;
    msg.event_age_ms = 0u;
    msg.event_quality = NET_QUALITY_OK;
    msg.door_target = NET_DOOR_TARGET_NOT_APPLICABLE;
    return msg;
}

BcmCommandRef BcmResult_RefOf(const NetCommandMeta *command, const NetCanRxMeta *meta)
{
    BcmCommandRef ref;
    ref.domainBoot = meta->source.boot_id;
    ref.commandId = command->command_id;
    return ref;
}
