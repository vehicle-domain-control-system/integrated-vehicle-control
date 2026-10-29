#include "bcm_sim.h"

typedef struct
{
    NetDoorLockState lock;
    bool             driving;
    NetDoorTarget    driveTarget;
    uint32_t         driveStartMs;
    bool             lockStuck;

    NetFanLevel      fanCommand;
    NetFanLevel      fanMeasured;
    uint32_t         fanChangedMs;
    bool             fanStuck;
    bool             fanLost;
} SimContext;

static SimContext s_sim;

void BcmSim_Init(void)
{
    s_sim.lock = NET_DOOR_LOCK_LOCKED;
    s_sim.driving = false;
    s_sim.driveTarget = NET_DOOR_TARGET_NOT_APPLICABLE;
    s_sim.driveStartMs = 0u;
    s_sim.lockStuck = false;
    s_sim.fanCommand = NET_FAN_OFF;
    s_sim.fanMeasured = NET_FAN_OFF;
    s_sim.fanChangedMs = 0u;
    s_sim.fanStuck = false;
    s_sim.fanLost = false;
}

void BcmSim_DoorDrive(bool active, NetDoorTarget target, uint32_t nowMs)
{
    if (!active)
    {
        s_sim.driving = false;
        return;
    }
    if (!s_sim.driving || (target != s_sim.driveTarget))
    {
        s_sim.driving = true;
        s_sim.driveTarget = target;
        s_sim.driveStartMs = nowMs;
    }
    if (!s_sim.lockStuck && ((nowMs - s_sim.driveStartMs) >= BCM_SIM_LOCK_TRAVEL_MS))
    {
        s_sim.lock = (target == NET_DOOR_TARGET_LOCK) ? NET_DOOR_LOCK_LOCKED : NET_DOOR_LOCK_UNLOCKED;
    }
}

NetDoorLockState BcmSim_LockState(void)
{
    return s_sim.lock;
}

void BcmSim_FanCommand(NetFanLevel level, uint32_t nowMs)
{
    if (level != s_sim.fanCommand)
    {
        s_sim.fanCommand = level;
        s_sim.fanChangedMs = nowMs;
    }
}

NetFanLevel BcmSim_FanMeasured(uint32_t nowMs)
{
    if (!s_sim.fanStuck && ((nowMs - s_sim.fanChangedMs) >= BCM_SIM_FAN_RESPONSE_MS))
    {
        s_sim.fanMeasured = s_sim.fanCommand;
    }
    return s_sim.fanLost ? NET_FAN_UNKNOWN : s_sim.fanMeasured;
}

void BcmSim_InjectLockStuck(bool stuck)
{
    s_sim.lockStuck = stuck;
}

void BcmSim_InjectFanStuck(bool stuck)
{
    s_sim.fanStuck = stuck;
}

void BcmSim_InjectFanLost(bool lost)
{
    s_sim.fanLost = lost;
}
