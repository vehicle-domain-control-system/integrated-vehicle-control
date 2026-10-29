#include "bcm_light.h"
#include "bcm_result.h"

#define PCT_MAX       100u
#define COLOR_RAW_MAX 255u

static const BcmRgbPct k_typeColor[BCM_LIGHT_TYPE_COUNT] =
{
    { 100u, 100u, 100u },
    {   0u,  70u,  70u },
    { 100u,  70u,   0u },
    { 100u,   0u,   0u }
};

typedef enum
{
    OUTPUT_NONE,
    OUTPUT_TARGET,
    OUTPUT_OFF
} OutputRequest;

typedef struct
{
    bool                hasCommand;
    NetLightType        type;
    uint8_t             levelPct;
    NetRgb              color;

    OutputRequest       request;
    bool                outputOn;
    NetLightApplyResult apply;
    bool                applyFault;
    bool                sustainAllowed;
    bool                sustainCut;

    bool                pending;
    bool                pendingFailed;
    bool                resultReady;
    BcmCommandRef       command;
} LightContext;

static LightContext s_light;

static uint8_t RawToPct(uint8_t raw)
{
    return (uint8_t)((((uint32_t)raw * PCT_MAX) + (COLOR_RAW_MAX / 2u)) / COLOR_RAW_MAX);
}

static uint8_t Scale(uint8_t colorPct, uint8_t levelPct)
{
    return (uint8_t)(((uint32_t)colorPct * levelPct) / PCT_MAX);
}

static BcmRgbPct TargetOutput(void)
{
    BcmRgbPct color = k_typeColor[s_light.type];
    BcmRgbPct out;
    if (s_light.type == NET_LIGHT_NORMAL)
    {
        color.redPct = RawToPct(s_light.color.r);
        color.greenPct = RawToPct(s_light.color.g);
        color.bluePct = RawToPct(s_light.color.b);
    }
    out.redPct = Scale(color.redPct, s_light.levelPct);
    out.greenPct = Scale(color.greenPct, s_light.levelPct);
    out.bluePct = Scale(color.bluePct, s_light.levelPct);
    return out;
}

void BcmLight_Init(void)
{
    LightContext zero = { 0 };
    s_light = zero;
    s_light.type = NET_LIGHT_NORMAL;
    s_light.apply = NET_LIGHT_APPLY_UNKNOWN;
    s_light.request = OUTPUT_OFF;
}

static bool IsStop(uint8_t levelPct)
{
    return levelPct == 0u;
}

bool BcmLight_IsStopCommand(const NetBcmLightCommand *command)
{
    return IsStop(command->light_level_percent);
}

void BcmLight_UpdateSustain(bool sustainAllowed)
{
    bool lostWhileRunning = !sustainAllowed && !IsStop(s_light.levelPct);

    s_light.sustainAllowed = sustainAllowed;
    if (!lostWhileRunning || s_light.sustainCut)
    {
        return;
    }
    s_light.sustainCut = true;
    s_light.request = OUTPUT_OFF;
    if (s_light.pending)
    {
        s_light.pendingFailed = true;
        s_light.resultReady = true;
    }
}

NetBcmResult BcmLight_Command(const NetBcmLightCommand *command, BcmCommandRef ref,
                              NetBcmResult *superseded, bool *hasSuperseded)
{
    *hasSuperseded = s_light.pending;
    if (s_light.pending)
    {
        *superseded = BcmResult_Make(NET_BCM_COMMAND_LIGHT, s_light.command, NET_RESULT_CANCELLED, NET_REASON_SUPERSEDED);
    }
    s_light.hasCommand = true;
    s_light.type = command->interior_light_type;
    s_light.levelPct = command->light_level_percent;
    s_light.color = command->interior_light_color;
    s_light.sustainCut = false;
    s_light.request = OUTPUT_TARGET;
    s_light.command = ref;
    s_light.pending = true;
    s_light.pendingFailed = false;
    s_light.resultReady = false;
    return BcmResult_Make(NET_BCM_COMMAND_LIGHT, ref, NET_RESULT_ACCEPTED, NET_REASON_NONE);
}

bool BcmLight_PendingOutput(BcmRgbPct *out)
{
    static const BcmRgbPct off = { 0u, 0u, 0u };
    switch (s_light.request)
    {
    case OUTPUT_TARGET:
        *out = TargetOutput();
        return true;
    case OUTPUT_OFF:
        *out = off;
        return true;
    case OUTPUT_NONE:
    default:
        return false;
    }
}

void BcmLight_OnApplied(bool applied)
{
    OutputRequest request = s_light.request;
    s_light.request = OUTPUT_NONE;

    if (request == OUTPUT_TARGET)
    {
        if (applied)
        {
            s_light.outputOn = !IsStop(s_light.levelPct);
            s_light.apply = NET_LIGHT_APPLY_APPLIED;
            s_light.applyFault = false;
            s_light.resultReady = s_light.pending;
            return;
        }
        s_light.applyFault = true;
        s_light.outputOn = false;
        s_light.request = OUTPUT_OFF;
        s_light.pendingFailed = true;
        s_light.resultReady = s_light.pending;
        return;
    }
    if (request == OUTPUT_OFF)
    {
        s_light.outputOn = false;
        if (s_light.applyFault)
        {
            s_light.apply = applied ? NET_LIGHT_APPLY_FAILED : NET_LIGHT_APPLY_UNKNOWN;
        }
        else
        {
            s_light.apply = applied ? NET_LIGHT_APPLY_APPLIED : NET_LIGHT_APPLY_UNKNOWN;
            s_light.applyFault = !applied;
        }
    }
}

bool BcmLight_Tick(NetBcmResult *result)
{
    if (!s_light.pending || !s_light.resultReady)
    {
        return false;
    }
    *result = s_light.pendingFailed
        ? BcmResult_Make(NET_BCM_COMMAND_LIGHT, s_light.command, NET_RESULT_FAILED,
                         s_light.applyFault ? NET_REASON_FAULT : NET_REASON_STALE)
        : BcmResult_Make(NET_BCM_COMMAND_LIGHT, s_light.command, NET_RESULT_DONE, NET_REASON_NONE);
    s_light.pending = false;
    s_light.resultReady = false;
    return true;
}

NetBcmLightState BcmLight_State(void)
{
    NetBcmLightState msg;
    NetValueQuality targetQuality = !s_light.hasCommand ? NET_QUALITY_NO_DATA
                                  : (s_light.sustainCut ? NET_QUALITY_STALE : NET_QUALITY_OK);
    msg.interior_light_type_state = s_light.type;
    msg.light_level_percent = s_light.outputOn ? s_light.levelPct : 0u;
    msg.interior_light_color_state = s_light.color;
    msg.interior_light_apply_result = s_light.apply;
    msg.light_physical_feedback_capability = NET_FLAG_NO;
    msg.light_type_quality = targetQuality;
    msg.light_level_quality = (s_light.apply == NET_LIGHT_APPLY_UNKNOWN) ? NET_QUALITY_INVALID : targetQuality;
    msg.light_color_quality = targetQuality;
    msg.light_apply_quality = (s_light.apply == NET_LIGHT_APPLY_UNKNOWN) ? NET_QUALITY_INVALID : NET_QUALITY_OK;
    msg.update_sequence = 0u;
    msg.source_age_ms = 0u;
    return msg;
}

bool BcmLight_ApplyFault(void)
{
    return s_light.applyFault;
}
