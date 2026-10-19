#include "domain_board_runtime.h"

#include <string.h>

#define DOMAIN_BOARD_RUNTIME_INTEGRITY_MARKER UINT32_C(0x4252544D)

static bool DomainBoardRuntime_HasMarker(
    const DomainBoardRuntime_t *boardRuntime)
{
    uint32_t marker;

    if (boardRuntime == NULL)
    {
        return false;
    }

    (void)memcpy(&marker, &boardRuntime->integrityMarker, sizeof(marker));
    return marker == DOMAIN_BOARD_RUNTIME_INTEGRITY_MARKER;
}

static DomainBoardRuntime_Status_t DomainBoardRuntime_Block(
    DomainBoardRuntime_t *boardRuntime,
    DomainBoardRuntime_Status_t status)
{
    boardRuntime->stage = DOMAIN_BOARD_RUNTIME_STAGE_FAULT_BLOCKED;
    boardRuntime->lastStatus = status;
    boardRuntime->ready = false;
    boardRuntime->faultBlocked = true;
    return status;
}

DomainBoardRuntime_Status_t DomainBoardRuntime_Init(
    DomainBoardRuntime_t *boardRuntime,
    const DomainBoardRuntime_Config_t *config)
{
    DomainBoardTimebase_Config_t timebaseConfig;
    DomainRuntime_Config_t runtimeConfig;
    DomainStm0TickReader_Status_t stmStatus;
    DomainBoardTimebase_Status_t timebaseStatus;
    uint32_t ticksPerMillisecond;

    if ((boardRuntime == NULL) || (config == NULL) ||
        (config->stm0InputClockHz == 0U) ||
        (config->runtimeConfig.timeProvider != NULL) ||
        (config->runtimeConfig.timeProviderContext != NULL))
    {
        return DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT;
    }

    if (DomainBoardRuntime_HasMarker(boardRuntime) &&
        boardRuntime->initAttempted)
    {
        return boardRuntime->ready ?
            DOMAIN_BOARD_RUNTIME_ALREADY_INITIALIZED :
            DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED;
    }

    (void)memset(boardRuntime, 0, sizeof(*boardRuntime));
    boardRuntime->integrityMarker = DOMAIN_BOARD_RUNTIME_INTEGRITY_MARKER;
    boardRuntime->initAttempted = true;
    boardRuntime->stage = DOMAIN_BOARD_RUNTIME_STAGE_CONFIG_VALIDATED;
    boardRuntime->lastStatus = DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED;
    boardRuntime->lastRuntimeStatus = DOMAIN_RUNTIME_NOT_INITIALIZED;

    stmStatus = DomainStm0TickReader_Init(
        &boardRuntime->stm0Reader,
        config->stm0InputClockHz);
    if (stmStatus != DOMAIN_STM0_TICK_READER_OK)
    {
        return DomainBoardRuntime_Block(
            boardRuntime,
            DOMAIN_BOARD_RUNTIME_STM0_INIT_FAILED);
    }
    boardRuntime->stage = DOMAIN_BOARD_RUNTIME_STAGE_STM0_READY;

    if (!DomainStm0TickReader_GetTicksPerMillisecond(
            &boardRuntime->stm0Reader,
            &ticksPerMillisecond))
    {
        return DomainBoardRuntime_Block(
            boardRuntime,
            DOMAIN_BOARD_RUNTIME_TIMEBASE_INIT_FAILED);
    }

    timebaseConfig.tickReader = DomainStm0TickReader_Read;
    timebaseConfig.tickReaderContext = &boardRuntime->stm0Reader;
    timebaseConfig.ticksPerMillisecond = ticksPerMillisecond;
    timebaseStatus = DomainBoardTimebase_Init(
        &boardRuntime->timebase,
        &timebaseConfig);
    if (timebaseStatus != DOMAIN_BOARD_TIMEBASE_OK)
    {
        return DomainBoardRuntime_Block(
            boardRuntime,
            DOMAIN_BOARD_RUNTIME_TIMEBASE_INIT_FAILED);
    }
    boardRuntime->stage = DOMAIN_BOARD_RUNTIME_STAGE_TIMEBASE_READY;

    runtimeConfig = config->runtimeConfig;
    runtimeConfig.timeProvider = DomainBoardTimebase_GetNowMs;
    runtimeConfig.timeProviderContext = &boardRuntime->timebase;
    boardRuntime->lastRuntimeStatus = DomainRuntime_Init(
        &boardRuntime->runtime,
        &runtimeConfig);
    if ((boardRuntime->lastRuntimeStatus != DOMAIN_RUNTIME_OK) ||
        !DomainRuntime_IsReady(&boardRuntime->runtime))
    {
        return DomainBoardRuntime_Block(
            boardRuntime,
            DOMAIN_BOARD_RUNTIME_DOMAIN_INIT_FAILED);
    }

    boardRuntime->stage = DOMAIN_BOARD_RUNTIME_STAGE_DOMAIN_READY;
    boardRuntime->lastStatus = DOMAIN_BOARD_RUNTIME_OK;
    boardRuntime->ready = true;
    return DOMAIN_BOARD_RUNTIME_OK;
}

DomainBoardRuntime_Status_t DomainBoardRuntime_Poll(
    DomainBoardRuntime_t *boardRuntime)
{
    if (boardRuntime == NULL)
    {
        return DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT;
    }
    if (!DomainBoardRuntime_HasMarker(boardRuntime) ||
        !boardRuntime->initAttempted)
    {
        return DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED;
    }
    if (!boardRuntime->ready || boardRuntime->faultBlocked)
    {
        return DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED;
    }

    boardRuntime->lastRuntimeStatus = DomainRuntime_Poll(
        &boardRuntime->runtime);
    if (boardRuntime->lastRuntimeStatus != DOMAIN_RUNTIME_OK)
    {
        return DomainBoardRuntime_Block(
            boardRuntime,
            DOMAIN_BOARD_RUNTIME_RUNTIME_POLL_FAILED);
    }

    boardRuntime->lastStatus = DOMAIN_BOARD_RUNTIME_OK;
    return DOMAIN_BOARD_RUNTIME_OK;
}

bool DomainBoardRuntime_IsReady(
    const DomainBoardRuntime_t *boardRuntime)
{
    return DomainBoardRuntime_HasMarker(boardRuntime) &&
           boardRuntime->initAttempted && boardRuntime->ready &&
           !boardRuntime->faultBlocked &&
           (boardRuntime->stage == DOMAIN_BOARD_RUNTIME_STAGE_DOMAIN_READY) &&
           DomainRuntime_IsReady(&boardRuntime->runtime);
}

DomainBoardRuntime_Status_t DomainBoardRuntime_GetSnapshot(
    const DomainBoardRuntime_t *boardRuntime,
    DomainBoardRuntime_Snapshot_t *outSnapshot)
{
    DomainBoardRuntime_Snapshot_t snapshot;

    if ((boardRuntime == NULL) || (outSnapshot == NULL))
    {
        return DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT;
    }
    if (!DomainBoardRuntime_HasMarker(boardRuntime) ||
        !boardRuntime->initAttempted)
    {
        return DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED;
    }

    snapshot.stage = boardRuntime->stage;
    snapshot.lastStatus = boardRuntime->lastStatus;
    snapshot.lastRuntimeStatus = boardRuntime->lastRuntimeStatus;
    snapshot.initAttempted = boardRuntime->initAttempted;
    snapshot.ready = boardRuntime->ready;
    snapshot.faultBlocked = boardRuntime->faultBlocked;
    *outSnapshot = snapshot;
    return DOMAIN_BOARD_RUNTIME_OK;
}

DomainRuntime_t *DomainBoardRuntime_GetDomainRuntime(
    DomainBoardRuntime_t *boardRuntime)
{
    return DomainBoardRuntime_IsReady(boardRuntime) ?
        &boardRuntime->runtime : NULL;
}
