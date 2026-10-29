#include <stddef.h>
#include "bcm_app.h"
#include "bcm_climate.h"
#include "bcm_config.h"
#include "bcm_diag.h"
#include "bcm_door.h"
#include "bcm_light.h"
#include "bcm_netlink.h"
#include "bcm_port.h"
#include "bcm_result.h"

typedef enum
{
    OUTBOX_RESULT,
    OUTBOX_EVENT
} OutboxKind;

typedef struct
{
    OutboxKind   kind;
    NetBcmResult result;
    NetBcmEvent  event;
    uint32_t     createdMs;
} OutboxEntry;

typedef enum
{
    HANDOFF_DOOR,
    HANDOFF_CLIMATE,
    HANDOFF_LIGHT
} HandoffKind;

typedef struct
{
    HandoffKind kind;
    union
    {
        NetBcmDoorCommand    door;
        NetBcmClimateCommand climate;
        NetBcmLightCommand   light;
    } command;
    NetCanRxMeta meta;
} HandoffEntry;

typedef struct
{
    bool            published;
    uint32_t        updateSequence;
    uint32_t        lastPublishMs;
    NetBcmDoorState last;
} DoorTrack;

typedef struct
{
    bool               published;
    uint32_t           updateSequence;
    uint32_t           lastPublishMs;
    NetBcmClimateState last;
} ClimateTrack;

typedef struct
{
    bool             published;
    uint32_t         updateSequence;
    uint32_t         lastPublishMs;
    NetBcmLightState last;
} LightTrack;

typedef struct
{
    bool         published;
    uint32_t     updateSequence;
    uint32_t     lastPublishMs;
    NetBcmStatus last;
} StatusTrack;

typedef struct
{
    uint32_t       nowMs;
    uint32_t       nextEventOccurrence;
    OutboxEntry    outbox[BCM_APP_OUTBOX_SIZE];
    uint32_t       outboxHead;
    uint32_t       outboxCount;
    HandoffEntry   handoff[BCM_COMMAND_HANDOFF_SIZE];
    uint32_t       handoffHead;
    uint32_t       handoffCount;
    DoorTrack      door;
    ClimateTrack   climate;
    LightTrack     light;
    StatusTrack    status;
    BcmAppCounters counters;
} AppContext;

static const AppContext k_emptyApp;
static AppContext s_app;

static uint32_t Increment(uint32_t value)
{
    return (value == UINT32_MAX) ? value : (value + 1u);
}

static uint32_t NextSequence(uint32_t current)
{
    uint32_t next = current + 1u;
    return (next == 0u) ? 1u : next;
}

static uint16_t ElapsedAge(uint32_t createdMs, uint32_t nowMs)
{
    uint32_t elapsed = nowMs - createdMs;
    return (elapsed >= (uint32_t)NET_AGE_SATURATED_MS) ? (uint16_t)NET_AGE_SATURATED_MS : (uint16_t)elapsed;
}

static NetSubmitResult SubmitEntry(const OutboxEntry *entry)
{
    if (entry->kind == OUTBOX_RESULT)
    {
        NetBcmResult result = entry->result;
        result.event_age_ms = ElapsedAge(entry->createdMs, s_app.nowMs);
        return BcmNet_SendResult(&result);
    }
    else
    {
        NetBcmEvent event = entry->event;
        event.event_age_ms = ElapsedAge(entry->createdMs, s_app.nowMs);
        return BcmNet_SendEvent(&event);
    }
}

static void FlushOutbox(void)
{
    while (s_app.outboxCount > 0u)
    {
        NetSubmitResult submitted = SubmitEntry(&s_app.outbox[s_app.outboxHead]);
        if ((submitted == NET_SUBMIT_NO_CAPACITY) || (submitted == NET_SUBMIT_NOT_READY))
        {
            return;
        }
        s_app.outboxHead = (s_app.outboxHead + 1u) % BCM_APP_OUTBOX_SIZE;
        s_app.outboxCount--;
    }
}

static void Enqueue(const OutboxEntry *entry)
{
    if (s_app.outboxCount >= BCM_APP_OUTBOX_SIZE)
    {
        s_app.counters.outbox_dropped = Increment(s_app.counters.outbox_dropped);
        return;
    }
    s_app.outbox[(s_app.outboxHead + s_app.outboxCount) % BCM_APP_OUTBOX_SIZE] = *entry;
    s_app.outboxCount++;
    FlushOutbox();
}

static void SendResult(const NetBcmResult *result)
{
    OutboxEntry entry;
    entry.kind = OUTBOX_RESULT;
    entry.result = *result;
    entry.createdMs = s_app.nowMs;
    Enqueue(&entry);
}

static void SendClimateEvent(const BcmClimateEvent *climateEvent)
{
    OutboxEntry entry;
    entry.kind = OUTBOX_EVENT;
    entry.event.event_kind = climateEvent->kind;
    entry.event.occurrence_id = s_app.nextEventOccurrence;
    entry.event.event_age_ms = 0u;
    entry.event.event_quality = NET_QUALITY_OK;
    entry.event.related_domain_boot = climateEvent->related.domainBoot;
    entry.event.related_command_id = climateEvent->related.commandId;
    entry.event.affected_function = NET_BCM_FUNCTION_CLIMATE;
    entry.createdMs = s_app.nowMs;
    s_app.nextEventOccurrence = NextSequence(s_app.nextEventOccurrence);
    Enqueue(&entry);
}

static void PushHandoff(const HandoffEntry *entry)
{
    if (s_app.handoffCount >= BCM_COMMAND_HANDOFF_SIZE)
    {
        s_app.counters.command_handoff_full = Increment(s_app.counters.command_handoff_full);
        return;
    }
    s_app.handoff[(s_app.handoffHead + s_app.handoffCount) % BCM_COMMAND_HANDOFF_SIZE] = *entry;
    s_app.handoffCount++;
}

static bool PopHandoff(HandoffEntry *out)
{
    if (s_app.handoffCount == 0u)
    {
        return false;
    }
    *out = s_app.handoff[s_app.handoffHead];
    s_app.handoffHead = (s_app.handoffHead + 1u) % BCM_COMMAND_HANDOFF_SIZE;
    s_app.handoffCount--;
    return true;
}

void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta)
{
    HandoffEntry entry;
    if ((message == NULL) || (meta == NULL))
    {
        return;
    }
    entry.kind = HANDOFF_DOOR;
    entry.command.door = *message;
    entry.meta = *meta;
    PushHandoff(&entry);
}

void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta)
{
    HandoffEntry entry;
    if ((message == NULL) || (meta == NULL))
    {
        return;
    }
    entry.kind = HANDOFF_CLIMATE;
    entry.command.climate = *message;
    entry.meta = *meta;
    PushHandoff(&entry);
}

void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta)
{
    HandoffEntry entry;
    if ((message == NULL) || (meta == NULL))
    {
        return;
    }
    entry.kind = HANDOFF_LIGHT;
    entry.command.light = *message;
    entry.meta = *meta;
    PushHandoff(&entry);
}

static const NetCommandMeta *CommandMetaOf(const HandoffEntry *entry)
{
    switch (entry->kind)
    {
    case HANDOFF_DOOR:    return &entry->command.door.command;
    case HANDOFF_CLIMATE: return &entry->command.climate.command;
    case HANDOFF_LIGHT:
    default:              return &entry->command.light.command;
    }
}

static bool IsExpired(const NetCommandMeta *command, uint32_t receivedMs, uint32_t nowMs)
{
    uint32_t age;
    if (command->accept_limit_ms == 0u)
    {
        return false;
    }
    age = (uint32_t)command->command_age_ms + BCM_CAN_TRANSFER_MARGIN_MS + (nowMs - receivedMs);
    return age >= (uint32_t)command->accept_limit_ms;
}

static bool IsWellFormed(const HandoffEntry *entry)
{
    switch (entry->kind)
    {
    case HANDOFF_DOOR:
        return entry->command.door.door_lock_target <= NET_DOOR_TARGET_UNLOCK;
    case HANDOFF_CLIMATE:
        return (entry->command.climate.fan_target_level <= NET_FAN_HIGH)
            && (entry->command.climate.thermal_direction <= NET_THERMAL_HEAT)
            && (entry->command.climate.thermal_target_percent <= BCM_PCT_MAX);
    case HANDOFF_LIGHT:
    default:
        return (entry->command.light.interior_light_type < BCM_LIGHT_TYPE_COUNT)
            && (entry->command.light.light_level_percent <= BCM_PCT_MAX);
    }
}

static bool CommandBlockedReason(bool stopRequest, NetReason *reason)
{
    if (BcmDiag_EcuState() == NET_CONTROLLER_INIT)
    {
        *reason = NET_REASON_NOT_READY;
        return true;
    }
    if (!BcmNetLink_DomainAlive() && !stopRequest)
    {
        *reason = NET_REASON_STALE;
        return true;
    }
    return false;
}

static void HandleDoor(const NetBcmDoorCommand *message, const NetCanRxMeta *meta)
{
    BcmCommandRef ref = BcmResult_RefOf(&message->command, meta);
    NetBcmResult result;
    NetReason reason;

    if (CommandBlockedReason(false, &reason))
    {
        result = BcmDoor_Rejected(ref, message->door_lock_target, reason);
    }
    else
    {
        result = BcmDoor_Command(message, ref, s_app.nowMs);
    }
    SendResult(&result);
}

static void HandleClimate(const NetBcmClimateCommand *message, const NetCanRxMeta *meta)
{
    BcmCommandRef ref = BcmResult_RefOf(&message->command, meta);
    bool stopRequest = BcmClimate_IsStopCommand(message);
    NetBcmResult superseded;
    NetBcmResult result;
    bool hasSuperseded = false;
    NetReason reason;

    if (CommandBlockedReason(stopRequest, &reason))
    {
        result = BcmResult_Make(NET_BCM_COMMAND_CLIMATE, ref, NET_RESULT_REJECTED, reason);
    }
    else
    {
        result = BcmClimate_Command(message, ref, &superseded, &hasSuperseded);
    }
    if (hasSuperseded)
    {
        SendResult(&superseded);
    }
    SendResult(&result);
}

static void HandleLight(const NetBcmLightCommand *message, const NetCanRxMeta *meta)
{
    BcmCommandRef ref = BcmResult_RefOf(&message->command, meta);
    bool stopRequest = BcmLight_IsStopCommand(message);
    NetBcmResult superseded;
    NetBcmResult result;
    bool hasSuperseded = false;
    NetReason reason;

    if (CommandBlockedReason(stopRequest, &reason))
    {
        result = BcmResult_Make(NET_BCM_COMMAND_LIGHT, ref, NET_RESULT_REJECTED, reason);
    }
    else
    {
        result = BcmLight_Command(message, ref, &superseded, &hasSuperseded);
    }
    if (hasSuperseded)
    {
        SendResult(&superseded);
    }
    SendResult(&result);
}

static void DeliverCommands(void)
{
    HandoffEntry entry;
    while (PopHandoff(&entry))
    {
        const NetCommandMeta *command = CommandMetaOf(&entry);
        if (command->command_id == 0u)
        {
            s_app.counters.command_zero_id = Increment(s_app.counters.command_zero_id);
            continue;
        }
        if (!IsWellFormed(&entry))
        {
            s_app.counters.command_invalid = Increment(s_app.counters.command_invalid);
            continue;
        }
        if (IsExpired(command, entry.meta.received_at_ms, s_app.nowMs))
        {
            s_app.counters.command_expired = Increment(s_app.counters.command_expired);
            continue;
        }
        s_app.counters.command_delivered = Increment(s_app.counters.command_delivered);
        switch (entry.kind)
        {
        case HANDOFF_DOOR:
            HandleDoor(&entry.command.door, &entry.meta);
            break;
        case HANDOFF_CLIMATE:
            HandleClimate(&entry.command.climate, &entry.meta);
            break;
        case HANDOFF_LIGHT:
        default:
            HandleLight(&entry.command.light, &entry.meta);
            break;
        }
    }
}

static void UpdateInputs(bool alive)
{
    BcmDoorInputs door;
    BcmClimateInputs climate;
    uint16_t raw = 0u;
    bool readOk = BcmPort_HeatsinkRaw(&raw);

    door.contactOpen = false;
    door.contactValid = BcmPort_DoorContact(&door.contactOpen);
    door.lockMeasured = BcmPort_LockState();
    door.sustainAllowed = alive;
    BcmDoor_UpdateInputs(&door, s_app.nowMs);

    climate.fanMeasured = BcmPort_FanMeasured();
    climate.heatsinkValid = readOk && (raw >= BCM_TEMP_ADC_OPEN_BELOW) && (raw <= BCM_TEMP_ADC_SHORT_ABOVE);
    climate.heatsinkDeciC = (int16_t)(((uint32_t)raw * (uint32_t)BCM_TEMP_RANGE_DECI_C) / BCM_TEMP_ADC_MAX);
    climate.sustainAllowed = alive;
    BcmClimate_UpdateInputs(&climate, s_app.nowMs);

    BcmLight_UpdateSustain(alive);
}

static void TickFunctions(void)
{
    NetBcmResult result;
    BcmClimateEvent climateEvent;

    if (BcmDoor_Tick(s_app.nowMs, &result))
    {
        SendResult(&result);
    }
    if (BcmClimate_Tick(s_app.nowMs, &result))
    {
        SendResult(&result);
    }
    while (BcmClimate_TakeEvent(&climateEvent))
    {
        SendClimateEvent(&climateEvent);
    }
    if (BcmLight_Tick(&result))
    {
        SendResult(&result);
    }
}

static void ApplyOutputs(void)
{
    BcmDoorOutputs door = BcmDoor_Outputs();
    BcmClimateOutputs climate = BcmClimate_Outputs();
    BcmRgbPct rgb;

    BcmPort_DoorDrive(door.driveActive, door.driveTarget);
    BcmPort_FanCommand(climate.fanCommand);
    BcmPort_Thermal(climate.direction, climate.outputPct);

    while (BcmLight_PendingOutput(&rgb))
    {
        BcmLight_OnApplied(BcmPort_LightApply(rgb.redPct, rgb.greenPct, rgb.bluePct));
    }
}

static void UpdateDiag(bool alive)
{
    BcmDiagInputs diag;
    diag.initFailed = !BcmPort_InitOk() || !BcmNetLink_InitOk();
    diag.inputsSettled = BcmDoor_IsSettled();
    diag.faults[BCM_FAULT_DOOR_SENSOR] = BcmDoor_SensorFault();
    diag.faults[BCM_FAULT_FAN_SENSOR] = BcmClimate_FanSensorFault();
    diag.faults[BCM_FAULT_TEMPERATURE_SENSOR] = BcmClimate_TemperatureSensorFault();
    diag.faults[BCM_FAULT_OVERHEAT] = BcmClimate_Overheat();
    diag.faults[BCM_FAULT_FAN] = BcmClimate_FanFault();
    diag.faults[BCM_FAULT_LOCK_ACTUATOR] = BcmDoor_ActuatorFault();
    diag.faults[BCM_FAULT_LIGHT_APPLY] = BcmLight_ApplyFault();
    diag.faults[BCM_FAULT_COMM_TIMEOUT] = !alive && BcmNetLink_DomainKnown();
    diag.recovering = BcmClimate_Recovering();
    BcmDiag_Update(&diag, s_app.nowMs);
}

static bool SameRgb(NetRgb a, NetRgb b)
{
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
}

static bool SameDoorState(const NetBcmDoorState *a, const NetBcmDoorState *b)
{
    return (a->door_lock_state == b->door_lock_state) && (a->door_open_state == b->door_open_state)
        && (a->door_composite_state == b->door_composite_state) && (a->lock_quality == b->lock_quality)
        && (a->open_quality == b->open_quality) && (a->composite_quality == b->composite_quality);
}

static bool SameClimateState(const NetBcmClimateState *a, const NetBcmClimateState *b)
{
    return (a->fan_command_level == b->fan_command_level) && (a->fan_measured_level == b->fan_measured_level)
        && (a->thermal_direction_state == b->thermal_direction_state)
        && (a->thermal_output_percent == b->thermal_output_percent)
        && (a->heat_removal_state == b->heat_removal_state)
        && (a->fan_command_quality == b->fan_command_quality)
        && (a->fan_measured_quality == b->fan_measured_quality)
        && (a->thermal_direction_quality == b->thermal_direction_quality)
        && (a->thermal_output_quality == b->thermal_output_quality)
        && (a->heat_removal_quality == b->heat_removal_quality);
}

static bool SameLightState(const NetBcmLightState *a, const NetBcmLightState *b)
{
    return (a->interior_light_type_state == b->interior_light_type_state)
        && (a->light_level_percent == b->light_level_percent)
        && SameRgb(a->interior_light_color_state, b->interior_light_color_state)
        && (a->interior_light_apply_result == b->interior_light_apply_result)
        && (a->light_physical_feedback_capability == b->light_physical_feedback_capability)
        && (a->light_type_quality == b->light_type_quality) && (a->light_level_quality == b->light_level_quality)
        && (a->light_color_quality == b->light_color_quality) && (a->light_apply_quality == b->light_apply_quality);
}

static bool SameStatus(const NetBcmStatus *a, const NetBcmStatus *b)
{
    return (a->bcm_ecu_state == b->bcm_ecu_state) && (a->fault_mask == b->fault_mask)
        && (a->active_fault_category == b->active_fault_category)
        && (a->active_fault_code == b->active_fault_code) && (a->affected_function == b->affected_function)
        && (a->recovering == b->recovering);
}

static bool PublishDue(bool published, uint32_t lastPublishMs, uint32_t periodMs)
{
    return !published || ((s_app.nowMs - lastPublishMs) >= periodMs);
}

static void CountRefused(void)
{
    s_app.counters.publish_refused = Increment(s_app.counters.publish_refused);
}

static void PublishDoor(void)
{
    NetBcmDoorState state = BcmDoor_State();
    bool changed = !s_app.door.published || !SameDoorState(&state, &s_app.door.last);
    if (!changed && !PublishDue(s_app.door.published, s_app.door.lastPublishMs, BCM_DOOR_STATE_PERIOD_MS))
    {
        return;
    }
    state.update_sequence = NextSequence(s_app.door.updateSequence);
    state.source_age_ms = 0u;
    if (BcmNet_PublishDoorState(&state) != NET_SUBMIT_OK)
    {
        CountRefused();
        return;
    }
    s_app.door.updateSequence = state.update_sequence;
    s_app.door.lastPublishMs = s_app.nowMs;
    s_app.door.last = state;
    s_app.door.published = true;
}

static void PublishClimate(void)
{
    NetBcmClimateState state = BcmClimate_State();
    bool changed = !s_app.climate.published || !SameClimateState(&state, &s_app.climate.last);
    if (!changed && !PublishDue(s_app.climate.published, s_app.climate.lastPublishMs, BCM_CLIMATE_STATE_PERIOD_MS))
    {
        return;
    }
    state.update_sequence = NextSequence(s_app.climate.updateSequence);
    state.source_age_ms = 0u;
    if (BcmNet_PublishClimateState(&state) != NET_SUBMIT_OK)
    {
        CountRefused();
        return;
    }
    s_app.climate.updateSequence = state.update_sequence;
    s_app.climate.lastPublishMs = s_app.nowMs;
    s_app.climate.last = state;
    s_app.climate.published = true;
}

static void PublishLight(void)
{
    NetBcmLightState state = BcmLight_State();
    bool changed = !s_app.light.published || !SameLightState(&state, &s_app.light.last);
    if (!changed && !PublishDue(s_app.light.published, s_app.light.lastPublishMs, BCM_LIGHT_STATE_PERIOD_MS))
    {
        return;
    }
    state.update_sequence = NextSequence(s_app.light.updateSequence);
    state.source_age_ms = 0u;
    if (BcmNet_PublishLightState(&state) != NET_SUBMIT_OK)
    {
        CountRefused();
        return;
    }
    s_app.light.updateSequence = state.update_sequence;
    s_app.light.lastPublishMs = s_app.nowMs;
    s_app.light.last = state;
    s_app.light.published = true;
}

static void PublishStatus(void)
{
    NetBcmStatus status = BcmDiag_Status();
    bool changed = !s_app.status.published || !SameStatus(&status, &s_app.status.last);
    if (!changed && !PublishDue(s_app.status.published, s_app.status.lastPublishMs, BCM_STATUS_PERIOD_MS))
    {
        return;
    }
    status.update_sequence = NextSequence(s_app.status.updateSequence);
    status.source_age_ms = 0u;
    if (BcmNet_PublishStatus(&status) != NET_SUBMIT_OK)
    {
        CountRefused();
        return;
    }
    s_app.status.updateSequence = status.update_sequence;
    s_app.status.lastPublishMs = s_app.nowMs;
    s_app.status.last = status;
    s_app.status.published = true;
}

void BcmApp_Init(uint32_t nowMs)
{
    s_app = k_emptyApp;
    s_app.nowMs = nowMs;
    s_app.nextEventOccurrence = 1u;
    BcmDoor_Init();
    BcmClimate_Init();
    BcmLight_Init();
    BcmDiag_Init();
}

void BcmApp_Step(uint32_t nowMs)
{
    bool alive;

    s_app.nowMs = nowMs;
    BcmNetLink_Refresh();
    FlushOutbox();
    DeliverCommands();
    alive = BcmNetLink_DomainAlive();
    UpdateInputs(alive);
    TickFunctions();
    ApplyOutputs();
    UpdateDiag(alive);
    PublishDoor();
    PublishClimate();
    PublishLight();
    PublishStatus();
}

void BcmApp_GetCounters(BcmAppCounters *out)
{
    *out = s_app.counters;
    out->climate_events_dropped = BcmClimate_EventsDropped();
}
