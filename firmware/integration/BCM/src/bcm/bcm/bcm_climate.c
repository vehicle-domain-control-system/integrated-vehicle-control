#include "bcm_climate.h"
#include "bcm_config.h"
#include "bcm_result.h"

#define EVENT_QUEUE_SIZE  4u

typedef struct
{
    bool     running;
    uint32_t sinceMs;
} Hold;

typedef enum
{
    FAN_HEALTH_NORMAL,
    FAN_HEALTH_FAULT,
    FAN_HEALTH_RECOVERING
} FanHealth;

typedef enum
{
    FAN_SENSOR_UNPROVEN,
    FAN_SENSOR_PROVEN,
    FAN_SENSOR_LOST
} FanSensor;

typedef enum
{
    FAN_EVENT_MISMATCH_LIMIT,
    FAN_EVENT_RUN_REQUESTED,
    FAN_EVENT_STOP_REQUESTED,
    FAN_EVENT_SENSOR_LOST,
    FAN_EVENT_MATCH_CONFIRMED
} FanHealthEvent;

typedef enum
{
    FAN_SENSOR_EVENT_UNKNOWN_TIMEOUT,
    FAN_SENSOR_EVENT_VALID_CONFIRMED,
    FAN_SENSOR_EVENT_ROTATION_CONFIRMED,
    FAN_SENSOR_EVENT_FAN_FAULT
} FanSensorEvent;

typedef struct
{
    bool                fanTargetValid;
    NetFanLevel         fanTarget;
    bool                thermalTargetValid;
    NetThermalDirection thermalDirection;
    uint8_t             thermalPct;

    bool                pending;
    BcmCommandRef       command;

    NetFanLevel         fanMeasured;
    bool                heatsinkValid;
    int16_t             heatsinkDeciC;
    bool                sustainAllowed;

    NetFanLevel         fanCommand;
    NetThermalDirection appliedDirection;
    uint8_t             appliedPct;
    NetThermalDirection lastOffDirection;
    uint32_t            offSinceMs;

    bool                overheat;
    bool                temperatureSensorFault;
    FanSensor           fanSensor;
    FanHealth           fanHealth;
    Hold                overheatClear;
    Hold                temperatureValid;
    Hold                fanMismatch;
    Hold                fanUnknown;
    Hold                fanMeasuredValid;
    Hold                fanMatch;

    BcmClimateEvent     events[EVENT_QUEUE_SIZE];
    uint32_t            eventCount;
    uint32_t            eventsDropped;
} ClimateContext;

static ClimateContext s_cl;

static void HoldTrack(Hold *hold, bool condition, uint32_t nowMs)
{
    if (!condition)
    {
        hold->running = false;
    }
    else if (!hold->running)
    {
        hold->running = true;
        hold->sinceMs = nowMs;
    }
}

static bool HoldReached(const Hold *hold, uint32_t nowMs, uint32_t durationMs)
{
    return hold->running && ((nowMs - hold->sinceMs) >= durationMs);
}

static void PushEvent(NetBcmEventKind kind)
{
    if (s_cl.eventCount >= EVENT_QUEUE_SIZE)
    {
        s_cl.eventsDropped = (s_cl.eventsDropped == UINT32_MAX) ? s_cl.eventsDropped : (s_cl.eventsDropped + 1u);
        return;
    }
    s_cl.events[s_cl.eventCount].kind = kind;
    s_cl.events[s_cl.eventCount].related = s_cl.command;
    s_cl.eventCount++;
}

static void DropThermalTarget(void)
{
    s_cl.thermalTargetValid = false;
}

static bool OverheatResumeMeasured(void)
{
    return s_cl.overheat && s_cl.heatsinkValid && (s_cl.heatsinkDeciC <= BCM_OVERHEAT_RESUME_DECI_C);
}

static void EvaluateTemperature(uint32_t nowMs)
{
    HoldTrack(&s_cl.temperatureValid, s_cl.heatsinkValid, nowMs);
    HoldTrack(&s_cl.overheatClear, OverheatResumeMeasured(), nowMs);
    if (!s_cl.heatsinkValid)
    {
        if (!s_cl.temperatureSensorFault)
        {
            s_cl.temperatureSensorFault = true;
            DropThermalTarget();
        }
        return;
    }
    if (s_cl.temperatureSensorFault && HoldReached(&s_cl.temperatureValid, nowMs, BCM_RECOVERY_CONFIRM_MS))
    {
        s_cl.temperatureSensorFault = false;
        PushEvent(NET_BCM_EVENT_RECOVERY_CONFIRMED);
    }

    if (s_cl.heatsinkDeciC > BCM_OVERHEAT_CUT_DECI_C)
    {
        if (!s_cl.overheat)
        {
            s_cl.overheat = true;
            DropThermalTarget();
            PushEvent(NET_BCM_EVENT_OVERHEAT_DETECTED);
        }
    }
    if (s_cl.overheat && (s_cl.fanHealth == FAN_HEALTH_NORMAL) && HoldReached(&s_cl.overheatClear, nowMs, BCM_RECOVERY_CONFIRM_MS))
    {
        s_cl.overheat = false;
        PushEvent(NET_BCM_EVENT_RECOVERY_CONFIRMED);
    }
}

static bool FanSensorLost(void)
{
    return s_cl.fanSensor == FAN_SENSOR_LOST;
}

static FanSensor NextFanSensor(FanSensor current, FanSensorEvent event)
{
    switch (event)
    {
    case FAN_SENSOR_EVENT_UNKNOWN_TIMEOUT:
        return FAN_SENSOR_LOST;
    case FAN_SENSOR_EVENT_VALID_CONFIRMED:
        return (current == FAN_SENSOR_LOST) ? FAN_SENSOR_UNPROVEN : current;
    case FAN_SENSOR_EVENT_ROTATION_CONFIRMED:
        return (current == FAN_SENSOR_LOST) ? FAN_SENSOR_LOST : FAN_SENSOR_PROVEN;
    case FAN_SENSOR_EVENT_FAN_FAULT:
        return (current == FAN_SENSOR_PROVEN) ? FAN_SENSOR_UNPROVEN : current;
    default:
        return current;
    }
}

static void HandleFanSensor(FanSensorEvent event)
{
    FanSensor previous = s_cl.fanSensor;
    FanSensor next = NextFanSensor(previous, event);

    if (next == previous)
    {
        return;
    }
    s_cl.fanSensor = next;
    if (next == FAN_SENSOR_LOST)
    {
        DropThermalTarget();
    }
    else if (previous == FAN_SENSOR_LOST)
    {
        PushEvent(NET_BCM_EVENT_RECOVERY_CONFIRMED);
    }
}

static FanHealth NextFanHealth(FanHealth current, FanHealthEvent event)
{
    switch (event)
    {
    case FAN_EVENT_MISMATCH_LIMIT:
        return FAN_HEALTH_FAULT;
    case FAN_EVENT_RUN_REQUESTED:
        return (current == FAN_HEALTH_NORMAL) ? FAN_HEALTH_NORMAL : FAN_HEALTH_RECOVERING;
    case FAN_EVENT_STOP_REQUESTED:
    case FAN_EVENT_SENSOR_LOST:
        return (current == FAN_HEALTH_RECOVERING) ? FAN_HEALTH_FAULT : current;
    case FAN_EVENT_MATCH_CONFIRMED:
        return (current == FAN_HEALTH_RECOVERING) ? FAN_HEALTH_NORMAL : current;
    default:
        return current;
    }
}

static void EnterFanFault(FanHealthEvent cause)
{
    s_cl.fanTargetValid = false;
    HandleFanSensor(FAN_SENSOR_EVENT_FAN_FAULT);
    DropThermalTarget();
    if (cause == FAN_EVENT_MISMATCH_LIMIT)
    {
        PushEvent(NET_BCM_EVENT_FAN_MISMATCH_DETECTED);
    }
}

static void HandleFanHealth(FanHealthEvent event)
{
    FanHealth next = NextFanHealth(s_cl.fanHealth, event);

    if (next == s_cl.fanHealth)
    {
        return;
    }
    s_cl.fanHealth = next;
    if (next == FAN_HEALTH_FAULT)
    {
        EnterFanFault(event);
    }
    else if (next == FAN_HEALTH_NORMAL)
    {
        PushEvent(NET_BCM_EVENT_RECOVERY_CONFIRMED);
    }
}

static void EvaluateFanSensor(uint32_t nowMs)
{
    bool measured = (s_cl.fanMeasured != NET_FAN_UNKNOWN);

    HoldTrack(&s_cl.fanUnknown, !measured, nowMs);
    HoldTrack(&s_cl.fanMeasuredValid, measured, nowMs);
    if (HoldReached(&s_cl.fanUnknown, nowMs, BCM_FAN_MEASURE_TIMEOUT_MS))
    {
        HandleFanSensor(FAN_SENSOR_EVENT_UNKNOWN_TIMEOUT);
    }
    if (HoldReached(&s_cl.fanMeasuredValid, nowMs, BCM_RECOVERY_CONFIRM_MS))
    {
        HandleFanSensor(FAN_SENSOR_EVENT_VALID_CONFIRMED);
    }
    if (FanSensorLost())
    {
        HandleFanHealth(FAN_EVENT_SENSOR_LOST);
    }
}

static void EvaluateFan(uint32_t nowMs)
{
    bool unknown = (s_cl.fanMeasured == NET_FAN_UNKNOWN);
    bool mismatch = !unknown && (s_cl.fanMeasured != s_cl.fanCommand);
    bool runningMatch = !unknown && !mismatch && (s_cl.fanCommand != NET_FAN_OFF);

    EvaluateFanSensor(nowMs);
    HoldTrack(&s_cl.fanMismatch, mismatch, nowMs);
    HoldTrack(&s_cl.fanMatch, runningMatch, nowMs);
    if (runningMatch)
    {
        HandleFanSensor(FAN_SENSOR_EVENT_ROTATION_CONFIRMED);
    }
    if (HoldReached(&s_cl.fanMismatch, nowMs, BCM_FAN_MISMATCH_LIMIT_MS))
    {
        HandleFanHealth(FAN_EVENT_MISMATCH_LIMIT);
    }
    if (HoldReached(&s_cl.fanMatch, nowMs, BCM_RECOVERY_CONFIRM_MS))
    {
        HandleFanHealth(FAN_EVENT_MATCH_CONFIRMED);
    }
}

static void EvaluateSustain(void)
{
    if (!s_cl.sustainAllowed)
    {
        DropThermalTarget();
        s_cl.fanTargetValid = false;
    }
}

static NetFanLevel DecideFan(void)
{
    if (!s_cl.fanTargetValid || (s_cl.fanHealth == FAN_HEALTH_FAULT))
    {
        return NET_FAN_OFF;
    }
    return s_cl.fanTarget;
}

static bool IsThermalRequest(NetThermalDirection direction, uint8_t pct)
{
    return (direction != NET_THERMAL_IDLE) && (pct > 0u);
}

static bool PendingThermalRequested(void)
{
    return IsThermalRequest(s_cl.thermalDirection, s_cl.thermalPct);
}

static bool ThermalAllowed(void)
{
    bool fanConfirmed = (s_cl.fanCommand != NET_FAN_OFF) && (s_cl.fanMeasured == s_cl.fanCommand);
    return s_cl.thermalTargetValid && s_cl.sustainAllowed && fanConfirmed
        && !s_cl.overheat && !s_cl.temperatureSensorFault && !FanSensorLost()
        && (s_cl.fanHealth == FAN_HEALTH_NORMAL);
}

static void ApplyThermal(uint32_t nowMs)
{
    NetThermalDirection desired = NET_THERMAL_IDLE;
    uint8_t pct = 0u;

    if (ThermalAllowed() && PendingThermalRequested())
    {
        desired = s_cl.thermalDirection;
        pct = (s_cl.thermalPct > BCM_THERMAL_MAX_OUTPUT_PCT) ? (uint8_t)BCM_THERMAL_MAX_OUTPUT_PCT : s_cl.thermalPct;
    }

    if ((s_cl.appliedDirection != NET_THERMAL_IDLE) && (desired != s_cl.appliedDirection))
    {
        s_cl.lastOffDirection = s_cl.appliedDirection;
        s_cl.offSinceMs = nowMs;
        s_cl.appliedDirection = NET_THERMAL_IDLE;
        s_cl.appliedPct = 0u;
    }
    if (desired == NET_THERMAL_IDLE)
    {
        return;
    }
    if ((s_cl.appliedDirection == NET_THERMAL_IDLE) && (s_cl.lastOffDirection != NET_THERMAL_IDLE)
        && (s_cl.lastOffDirection != desired) && ((nowMs - s_cl.offSinceMs) < BCM_THERMAL_REVERSE_DELAY_MS))
    {
        return;
    }
    s_cl.appliedDirection = desired;
    s_cl.appliedPct = pct;
}

static bool TargetReached(void)
{
    bool fanReached = (s_cl.fanHealth == FAN_HEALTH_NORMAL)
        && (s_cl.fanCommand == s_cl.fanTarget) && (s_cl.fanMeasured == s_cl.fanTarget);
    bool thermalReached = !PendingThermalRequested()
        || ((s_cl.appliedDirection == s_cl.thermalDirection) && (s_cl.appliedPct > 0u));
    return fanReached && thermalReached;
}

static bool IsStop(NetFanLevel fanTarget, NetThermalDirection thermalDirection, uint8_t thermalPct)
{
    return (fanTarget == NET_FAN_OFF) && !IsThermalRequest(thermalDirection, thermalPct);
}

bool BcmClimate_IsStopCommand(const NetBcmClimateCommand *command)
{
    return IsStop(command->fan_target_level, command->thermal_direction, command->thermal_target_percent);
}

static bool PendingStep(NetBcmResult *result)
{
    NetReason reason = NET_REASON_NONE;
    bool failed = true;
    bool lostWhileRunning = !s_cl.sustainAllowed && !IsStop(s_cl.fanTarget, s_cl.thermalDirection, s_cl.thermalPct);
    bool thermalBlocked = (s_cl.overheat || s_cl.temperatureSensorFault) && PendingThermalRequested();

    if (!s_cl.pending)
    {
        return false;
    }
    if (lostWhileRunning)
    {
        reason = NET_REASON_STALE;
    }
    else if (s_cl.fanHealth == FAN_HEALTH_FAULT)
    {
        reason = NET_REASON_FAN_MISMATCH;
    }
    else if (thermalBlocked)
    {
        reason = s_cl.overheat ? NET_REASON_OVERHEAT : NET_REASON_FAULT;
    }
    else if (FanSensorLost())
    {
        reason = NET_REASON_FAULT;
    }
    else if (TargetReached())
    {
        failed = false;
    }
    else
    {
        return false;
    }
    *result = BcmResult_Make(NET_BCM_COMMAND_CLIMATE, s_cl.command, failed ? NET_RESULT_FAILED : NET_RESULT_DONE, reason);
    s_cl.pending = false;
    return true;
}

void BcmClimate_Init(void)
{
    ClimateContext zero = { 0 };
    s_cl = zero;
    s_cl.fanTarget = NET_FAN_OFF;
    s_cl.fanMeasured = NET_FAN_UNKNOWN;
    s_cl.fanCommand = NET_FAN_OFF;
    s_cl.thermalDirection = NET_THERMAL_IDLE;
    s_cl.appliedDirection = NET_THERMAL_IDLE;
    s_cl.lastOffDirection = NET_THERMAL_IDLE;
    s_cl.fanHealth = FAN_HEALTH_NORMAL;
    s_cl.fanSensor = FAN_SENSOR_UNPROVEN;
}

void BcmClimate_UpdateInputs(const BcmClimateInputs *inputs, uint32_t nowMs)
{
    (void)nowMs;
    s_cl.fanMeasured = inputs->fanMeasured;
    s_cl.heatsinkValid = inputs->heatsinkValid;
    s_cl.heatsinkDeciC = inputs->heatsinkDeciC;
    s_cl.sustainAllowed = inputs->sustainAllowed;
}

static bool IsInvalidCombination(const NetBcmClimateCommand *command)
{
    bool idleWithOutput = (command->thermal_direction == NET_THERMAL_IDLE) && (command->thermal_target_percent > 0u);
    bool fanOffWithThermal = (command->fan_target_level == NET_FAN_OFF)
        && IsThermalRequest(command->thermal_direction, command->thermal_target_percent);
    return idleWithOutput || fanOffWithThermal;
}

static NetReason RejectReason(const NetBcmClimateCommand *command)
{
    bool thermalRequested = IsThermalRequest(command->thermal_direction, command->thermal_target_percent);

    if (IsInvalidCombination(command))
    {
        return NET_REASON_CMD_INVALID;
    }
    if (thermalRequested && s_cl.overheat)
    {
        return NET_REASON_OVERHEAT;
    }
    if (thermalRequested && ((s_cl.fanHealth != FAN_HEALTH_NORMAL) || FanSensorLost()))
    {
        return NET_REASON_FAN_MISMATCH;
    }
    if (thermalRequested && s_cl.temperatureSensorFault)
    {
        return NET_REASON_FAULT;
    }
    return NET_REASON_NONE;
}

NetBcmResult BcmClimate_Command(const NetBcmClimateCommand *command, BcmCommandRef ref,
                                NetBcmResult *superseded, bool *hasSuperseded)
{
    NetReason reject = RejectReason(command);
    bool runRequested = (command->fan_target_level != NET_FAN_OFF);
    FanHealthEvent fanRequest = runRequested ? FAN_EVENT_RUN_REQUESTED : FAN_EVENT_STOP_REQUESTED;

    *hasSuperseded = false;
    if (reject != NET_REASON_NONE)
    {
        return BcmResult_Make(NET_BCM_COMMAND_CLIMATE, ref, NET_RESULT_REJECTED, reject);
    }
    if (s_cl.pending)
    {
        *superseded = BcmResult_Make(NET_BCM_COMMAND_CLIMATE, s_cl.command, NET_RESULT_CANCELLED, NET_REASON_SUPERSEDED);
        *hasSuperseded = true;
    }

    s_cl.command = ref;
    s_cl.pending = true;
    s_cl.fanTargetValid = true;
    s_cl.fanTarget = command->fan_target_level;
    s_cl.thermalTargetValid = true;
    s_cl.thermalDirection = command->thermal_direction;
    s_cl.thermalPct = command->thermal_target_percent;
    HandleFanHealth(fanRequest);
    return BcmResult_Make(NET_BCM_COMMAND_CLIMATE, ref, NET_RESULT_ACCEPTED, NET_REASON_NONE);
}

bool BcmClimate_Tick(uint32_t nowMs, NetBcmResult *result)
{
    EvaluateSustain();
    EvaluateTemperature(nowMs);
    s_cl.fanCommand = DecideFan();
    EvaluateFan(nowMs);
    s_cl.fanCommand = DecideFan();
    ApplyThermal(nowMs);
    return PendingStep(result);
}

bool BcmClimate_TakeEvent(BcmClimateEvent *event)
{
    uint32_t i;
    if (s_cl.eventCount == 0u)
    {
        return false;
    }
    *event = s_cl.events[0];
    for (i = 1u; i < s_cl.eventCount; i++)
    {
        s_cl.events[i - 1u] = s_cl.events[i];
    }
    s_cl.eventCount--;
    return true;
}

BcmClimateOutputs BcmClimate_Outputs(void)
{
    BcmClimateOutputs out;
    out.fanCommand = s_cl.fanCommand;
    out.direction = s_cl.appliedDirection;
    out.outputPct = s_cl.appliedPct;
    return out;
}

static NetValueQuality FanMeasuredQuality(void)
{
    bool stopUnconfirmed = (s_cl.fanCommand == NET_FAN_OFF) && (s_cl.fanMeasured == NET_FAN_OFF)
        && (s_cl.fanSensor != FAN_SENSOR_PROVEN);

    if (FanSensorLost())
    {
        return NET_QUALITY_INVALID;
    }
    if ((s_cl.fanMeasured == NET_FAN_UNKNOWN) || stopUnconfirmed)
    {
        return NET_QUALITY_NO_DATA;
    }
    return NET_QUALITY_OK;
}

NetBcmClimateState BcmClimate_State(void)
{
    NetBcmClimateState msg;
    msg.fan_command_level = s_cl.fanCommand;
    msg.fan_measured_level = s_cl.fanMeasured;
    msg.thermal_direction_state = s_cl.appliedDirection;
    msg.thermal_output_percent = s_cl.appliedPct;
    if (!s_cl.heatsinkValid || s_cl.temperatureSensorFault)
    {
        msg.heat_removal_state = NET_HEAT_REMOVAL_UNAVAILABLE;
        msg.heat_removal_quality = NET_QUALITY_INVALID;
    }
    else
    {
        msg.heat_removal_state = s_cl.overheat ? NET_HEAT_REMOVAL_OVERHEAT_CUTOFF : NET_HEAT_REMOVAL_NORMAL;
        msg.heat_removal_quality = NET_QUALITY_OK;
    }
    msg.fan_command_quality = NET_QUALITY_OK;
    msg.fan_measured_quality = FanMeasuredQuality();
    msg.thermal_direction_quality = NET_QUALITY_OK;
    msg.thermal_output_quality = NET_QUALITY_OK;
    msg.update_sequence = 0u;
    msg.source_age_ms = 0u;
    return msg;
}

bool BcmClimate_FanFault(void)               { return s_cl.fanHealth != FAN_HEALTH_NORMAL; }
bool BcmClimate_FanSensorFault(void)         { return FanSensorLost(); }
bool BcmClimate_TemperatureSensorFault(void) { return s_cl.temperatureSensorFault; }
bool BcmClimate_Overheat(void)               { return s_cl.overheat; }
bool BcmClimate_Recovering(void)             { return (s_cl.fanHealth == FAN_HEALTH_RECOVERING) || (s_cl.overheat && s_cl.overheatClear.running); }

uint32_t BcmClimate_EventsDropped(void)
{
    return s_cl.eventsDropped;
}
