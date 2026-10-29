#include "bcm_door.h"
#include "bcm_config.h"
#include "bcm_result.h"

typedef enum
{
    PHASE_IDLE,
    PHASE_DRIVING,
    PHASE_CONFIRMING
} DoorPhase;

typedef enum
{
    CONTACT_NOT_READ,
    CONTACT_UNREADABLE,
    CONTACT_READABLE
} ContactSource;

typedef struct
{
    ContactSource    contact;
    bool             rawOpen;
    uint32_t         rawSinceMs;
    NetDoorOpenState open;
    NetDoorLockState lock;

    DoorPhase        phase;
    BcmCommandRef    command;
    NetDoorTarget    target;
    uint32_t         phaseStartMs;
    uint32_t         attempts;
    bool             sustainAllowed;

    uint32_t         nextOccurrenceId;
    bool             actuatorFault;
} DoorContext;

static DoorContext s_door;

static NetDoorCompositeState Composite(NetDoorLockState lock, NetDoorOpenState open)
{
    if ((lock == NET_DOOR_LOCK_UNKNOWN) || (open == NET_DOOR_OPEN_UNKNOWN))
    {
        return NET_DOOR_COMPOSITE_UNTRUSTED;
    }
    if ((lock == NET_DOOR_LOCK_LOCKED) && (open == NET_DOOR_OPEN_OPEN))
    {
        return NET_DOOR_COMPOSITE_INCONSISTENT;
    }
    return NET_DOOR_COMPOSITE_NORMAL;
}

static uint32_t TakeOccurrenceId(void)
{
    uint32_t id = s_door.nextOccurrenceId;
    s_door.nextOccurrenceId++;
    if (s_door.nextOccurrenceId == 0u)
    {
        s_door.nextOccurrenceId = 1u;
    }
    return id;
}

static NetBcmResult DoorResult(BcmCommandRef command, NetDoorTarget target, NetVehicleResult result,
                               NetReason reason, NetDoorEvent event)
{
    NetBcmResult msg = BcmResult_Make(NET_BCM_COMMAND_DOOR, command, result, reason);
    msg.door_target = target;
    msg.door_event = event;
    if (event != NET_DOOR_EVENT_NONE)
    {
        msg.occurrence_id = TakeOccurrenceId();
    }
    return msg;
}

static NetDoorEvent CompletedEvent(NetDoorTarget target)
{
    return (target == NET_DOOR_TARGET_LOCK) ? NET_DOOR_EVENT_LOCK_COMPLETED : NET_DOOR_EVENT_UNLOCK_COMPLETED;
}

static void EnterPhase(DoorPhase phase, uint32_t nowMs)
{
    s_door.phase = phase;
    s_door.phaseStartMs = nowMs;
}

void BcmDoor_Init(void)
{
    s_door.contact = CONTACT_NOT_READ;
    s_door.rawOpen = false;
    s_door.rawSinceMs = 0u;
    s_door.open = NET_DOOR_OPEN_UNKNOWN;
    s_door.lock = NET_DOOR_LOCK_UNKNOWN;
    s_door.phase = PHASE_IDLE;
    s_door.attempts = 0u;
    s_door.sustainAllowed = false;
    s_door.nextOccurrenceId = 1u;
    s_door.actuatorFault = false;
}

static void UpdateContact(const BcmDoorInputs *inputs, uint32_t nowMs)
{
    if (!inputs->contactValid)
    {
        s_door.contact = CONTACT_UNREADABLE;
        s_door.open = NET_DOOR_OPEN_UNKNOWN;
        return;
    }
    if ((s_door.contact != CONTACT_READABLE) || (inputs->contactOpen != s_door.rawOpen))
    {
        s_door.contact = CONTACT_READABLE;
        s_door.rawOpen = inputs->contactOpen;
        s_door.rawSinceMs = nowMs;
    }
    if ((nowMs - s_door.rawSinceMs) >= BCM_DOOR_CONTACT_DEBOUNCE_MS)
    {
        s_door.open = s_door.rawOpen ? NET_DOOR_OPEN_OPEN : NET_DOOR_OPEN_CLOSED;
    }
}

void BcmDoor_UpdateInputs(const BcmDoorInputs *inputs, uint32_t nowMs)
{
    UpdateContact(inputs, nowMs);
    s_door.lock = inputs->lockMeasured;
    s_door.sustainAllowed = inputs->sustainAllowed;
}

bool BcmDoor_IsSettled(void)
{
    return (s_door.contact == CONTACT_UNREADABLE) || (s_door.open != NET_DOOR_OPEN_UNKNOWN);
}

NetBcmResult BcmDoor_Rejected(BcmCommandRef ref, NetDoorTarget target, NetReason reason)
{
    return DoorResult(ref, target, NET_RESULT_REJECTED, reason, NET_DOOR_EVENT_REQUEST_REJECTED);
}

NetBcmResult BcmDoor_Command(const NetBcmDoorCommand *command, BcmCommandRef ref, uint32_t nowMs)
{
    NetDoorTarget target = command->door_lock_target;

    if (s_door.phase != PHASE_IDLE)
    {
        return BcmDoor_Rejected(ref, target, NET_REASON_NOT_READY);
    }
    if (Composite(s_door.lock, s_door.open) != NET_DOOR_COMPOSITE_NORMAL)
    {
        return BcmDoor_Rejected(ref, target, NET_REASON_STATE_UNTRUSTED);
    }
    if ((target == NET_DOOR_TARGET_LOCK) && (s_door.open != NET_DOOR_OPEN_CLOSED))
    {
        return BcmDoor_Rejected(ref, target, NET_REASON_DOOR_OPEN);
    }
    if (s_door.lock == target)
    {
        return DoorResult(ref, target, NET_RESULT_DONE, NET_REASON_ALREADY_AT_TARGET,
                          NET_DOOR_EVENT_ALREADY_AT_TARGET);
    }

    s_door.command = ref;
    s_door.target = target;
    s_door.attempts = 1u;
    EnterPhase(PHASE_DRIVING, nowMs);
    return DoorResult(ref, target, NET_RESULT_ACCEPTED, NET_REASON_NONE, NET_DOOR_EVENT_NONE);
}

static bool Finish(NetVehicleResult result, NetReason reason, NetDoorEvent event, NetBcmResult *out)
{
    *out = DoorResult(s_door.command, s_door.target, result, reason, event);
    s_door.phase = PHASE_IDLE;
    return true;
}

bool BcmDoor_Tick(uint32_t nowMs, NetBcmResult *result)
{
    uint32_t elapsed = nowMs - s_door.phaseStartMs;

    switch (s_door.phase)
    {
    case PHASE_DRIVING:
        if (elapsed >= BCM_DOOR_DRIVE_LIMIT_MS)
        {
            s_door.actuatorFault = true;
            return Finish(NET_RESULT_FAILED, NET_REASON_DRIVE_LIMIT_EXCEEDED, NET_DOOR_EVENT_REQUEST_FAILED, result);
        }
        if ((elapsed >= BCM_DOOR_DRIVE_MS) || !s_door.sustainAllowed)
        {
            EnterPhase(PHASE_CONFIRMING, nowMs);
        }
        return false;

    case PHASE_CONFIRMING:
        if (s_door.lock == s_door.target)
        {
            s_door.actuatorFault = false;
            return Finish(NET_RESULT_DONE, NET_REASON_NONE, CompletedEvent(s_door.target), result);
        }
        if (elapsed < BCM_DOOR_CONFIRM_LIMIT_MS)
        {
            return false;
        }
        if ((s_door.attempts <= BCM_DOOR_RETRY_COUNT) && s_door.sustainAllowed)
        {
            s_door.attempts++;
            EnterPhase(PHASE_DRIVING, nowMs);
            return false;
        }
        s_door.actuatorFault = true;
        return Finish(NET_RESULT_FAILED, NET_REASON_NO_FEEDBACK, NET_DOOR_EVENT_REQUEST_FAILED, result);

    case PHASE_IDLE:
    default:
        return false;
    }
}

BcmDoorOutputs BcmDoor_Outputs(void)
{
    BcmDoorOutputs out;
    out.driveActive = (s_door.phase == PHASE_DRIVING);
    out.driveTarget = out.driveActive ? s_door.target : NET_DOOR_TARGET_NOT_APPLICABLE;
    return out;
}

static NetValueQuality UnknownOpenQuality(void)
{
    return (s_door.contact == CONTACT_UNREADABLE) ? NET_QUALITY_INVALID : NET_QUALITY_NO_DATA;
}

NetBcmDoorState BcmDoor_State(void)
{
    NetBcmDoorState msg;
    msg.door_lock_state = s_door.lock;
    msg.door_open_state = s_door.open;
    msg.door_composite_state = Composite(s_door.lock, s_door.open);
    msg.lock_quality = (s_door.lock == NET_DOOR_LOCK_UNKNOWN) ? NET_QUALITY_NO_DATA : NET_QUALITY_OK;
    msg.open_quality = (s_door.open == NET_DOOR_OPEN_UNKNOWN) ? UnknownOpenQuality() : NET_QUALITY_OK;
    msg.composite_quality = (msg.door_composite_state == NET_DOOR_COMPOSITE_UNTRUSTED) ? UnknownOpenQuality() : NET_QUALITY_OK;
    msg.update_sequence = 0u;
    msg.source_age_ms = 0u;
    return msg;
}

bool BcmDoor_ActuatorFault(void)
{
    return s_door.actuatorFault;
}

bool BcmDoor_SensorFault(void)
{
    return BcmDoor_IsSettled() && (Composite(s_door.lock, s_door.open) != NET_DOOR_COMPOSITE_NORMAL);
}
