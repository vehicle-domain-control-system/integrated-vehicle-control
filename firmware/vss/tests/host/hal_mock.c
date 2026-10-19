#include "hal_mock.h"

#include <string.h>

static HalMockLog s_log;

static uint8_t  s_readData[HAL_MOCK_READ_MAX];
static uint32_t s_readCount;
static uint32_t s_readPos;

static uint32_t s_i2cIndex;
static uint32_t s_failIndex;
static bool     s_failArmed;

static uint32_t s_ticks;
static uint32_t s_tickReads;
static uint32_t s_boundaryCountdown;
static uint32_t s_saiErrorCountdown;
static void (*s_boundaryProbe)(void);

static bool s_boardInitResult;
static bool s_audioInitResult;
static HalAudioTxIsrFn s_onBoundary;
static HalAudioTxIsrFn s_onError;
static uint32_t s_audioInitCalls;
static uint32_t s_audioStartCalls;
static uint32_t s_audioSubmits;
static uint32_t s_audioAbortCalls;
static uint32_t s_audioStops;
static bool s_audioInitOk;
static bool s_audioStarted;
static bool s_audioAborted;
static HalAudioTxAbortState s_abortResult;
static uint16_t s_ring[HAL_MOCK_RING_WORDS];

static bool s_sw2;

void HalMock_Reset(void)
{
    memset(&s_log, 0, sizeof(s_log));
    memset(s_readData, 0, sizeof(s_readData));
    s_readCount = 0u;
    s_readPos = 0u;
    s_i2cIndex = 0u;
    s_failIndex = 0u;
    s_failArmed = false;
    s_ticks = 0u;
    s_tickReads = 0u;
    s_boundaryCountdown = 0u;
    s_saiErrorCountdown = 0u;
    s_boundaryProbe = NULL;
    s_boardInitResult = true;
    s_audioInitResult = true;
    s_onBoundary = NULL;
    s_onError = NULL;
    s_audioInitCalls = 0u;
    s_audioStartCalls = 0u;
    s_audioSubmits = 0u;
    s_audioAbortCalls = 0u;
    s_audioStops = 0u;
    s_audioInitOk = false;
    s_audioStarted = false;
    s_audioAborted = false;
    s_abortResult = HAL_AUDIO_TX_ABORT_STOPPED;
    /* Non-zero so a test can see that initialization cleared the ring. */
    memset(s_ring, 0xA5, sizeof(s_ring));
    s_sw2 = false;
}

const HalMockLog *HalMock_Log(void)
{
    return &s_log;
}

void HalMock_ClearLog(void)
{
    memset(&s_log, 0, sizeof(s_log));
    s_i2cIndex = 0u;
}

void HalMock_QueueRead(const uint8_t *bytes, uint32_t len)
{
    uint32_t i;

    for (i = 0u; i < len && s_readCount < HAL_MOCK_READ_MAX; ++i)
    {
        s_readData[s_readCount++] = bytes[i];
    }
}

void HalMock_FailI2cAt(uint32_t index)
{
    s_failIndex = index;
    s_failArmed = true;
}

static HalMockOp *HalMock_Append(HalMockOpKind kind)
{
    HalMockOp *op;

    if (s_log.count >= HAL_MOCK_LOG_MAX)
    {
        s_log.overflow = true;
        return NULL;
    }
    op = &s_log.ops[s_log.count++];
    memset(op, 0, sizeof(*op));
    op->kind = kind;
    return op;
}

static bool HalMock_I2cFails(void)
{
    bool fail = s_failArmed && (s_i2cIndex == s_failIndex);

    s_i2cIndex++;
    return fail;
}

/* ---- hal_board.h ---------------------------------------------------- */

bool HalBoard_Init(void)
{
    return s_boardInitResult;
}

/* ---- hal_timer.h ---------------------------------------------------- */

void HalTimer_Init(void)
{
}

uint32_t HalTimer_GetTicks(void)
{
    s_tickReads++;
    if (s_boundaryCountdown != 0u)
    {
        s_boundaryCountdown--;
        if (s_boundaryCountdown == 0u)
        {
            HalMock_Boundary();
            if (s_boundaryProbe != NULL)
            {
                s_boundaryProbe();
            }
        }
    }
    if (s_saiErrorCountdown != 0u)
    {
        s_saiErrorCountdown--;
        if (s_saiErrorCountdown == 0u)
        {
            HalMock_SaiError();
        }
    }
    return s_ticks;
}

void HalTimer_WaitTicks(uint32_t ticks)
{
    HalMockOp *op = HalMock_Append(HAL_MOCK_WAIT);

    if (op != NULL)
    {
        op->ticks = ticks;
    }
    s_ticks += ticks;
}

void HalMock_SetTicks(uint32_t ticks)
{
    s_ticks = ticks;
}

uint32_t HalMock_TickReads(void)
{
    return s_tickReads;
}

void HalMock_BoundaryOnTickRead(uint32_t nthRead)
{
    s_boundaryCountdown = nthRead;
}

void HalMock_SetBoundaryProbe(void (*probe)(void))
{
    s_boundaryProbe = probe;
}

void HalMock_SaiErrorOnTickRead(uint32_t nthRead)
{
    s_saiErrorCountdown = nthRead;
}

/* ---- hal_i2c.h ------------------------------------------------------ */

HalI2cStatus HalI2c_Write(uint8_t addr7, const uint8_t *data, uint32_t len, bool sendStop)
{
    HalMockOp *op = HalMock_Append(HAL_MOCK_I2C_WRITE);
    bool fail = HalMock_I2cFails();
    uint32_t i;

    if (op != NULL)
    {
        op->addr = addr7;
        op->len = (uint8_t)len;
        op->sendStop = sendStop;
        op->failed = fail;
        for (i = 0u; i < len && i < HAL_MOCK_MAX_BYTES; ++i)
        {
            op->bytes[i] = data[i];
        }
    }
    if (len == 0u || len > HAL_MOCK_MAX_BYTES)
    {
        return HAL_I2C_ERROR;
    }
    return fail ? HAL_I2C_ERROR : HAL_I2C_OK;
}

HalI2cStatus HalI2c_Read(uint8_t addr7, uint8_t *data, uint32_t len, bool sendStop)
{
    HalMockOp *op = HalMock_Append(HAL_MOCK_I2C_READ);
    bool fail = HalMock_I2cFails();
    uint32_t i;

    for (i = 0u; i < len; ++i)
    {
        data[i] = (s_readPos < s_readCount) ? s_readData[s_readPos++] : 0u;
    }
    if (op != NULL)
    {
        op->addr = addr7;
        op->len = (uint8_t)len;
        op->sendStop = sendStop;
        op->failed = fail;
        for (i = 0u; i < len && i < HAL_MOCK_MAX_BYTES; ++i)
        {
            op->bytes[i] = data[i];
        }
    }
    return fail ? HAL_I2C_ERROR : HAL_I2C_OK;
}

/* ---- hal_audio_tx.h ------------------------------------------------- */

uint16_t *HalAudioTx_Ring(void)
{
    return s_ring;
}

bool HalAudioTx_Init(HalAudioTxIsrFn onBoundary, HalAudioTxIsrFn onError)
{
    s_audioInitCalls++;
    s_onBoundary = onBoundary;
    s_onError = onError;
    s_audioInitOk = s_audioInitResult;
    return s_audioInitResult;
}

bool HalAudioTx_Start(void)
{
    s_audioStartCalls++;
    if (s_audioStarted || s_audioAborted || !s_audioInitOk)
    {
        return false;
    }
    s_audioStarted = true;
    s_audioSubmits++;
    return true;
}

void HalAudioTx_Abort(void)
{
    s_audioAbortCalls++;
    if (s_audioAborted)
    {
        return;
    }
    s_audioAborted = true;
    s_audioStops++;
}

HalAudioTxAbortState HalAudioTx_AbortState(void)
{
    return s_audioAborted ? s_abortResult : HAL_AUDIO_TX_ABORT_NONE;
}

void HalMock_SetAbortResult(HalAudioTxAbortState state)
{
    s_abortResult = state;
}

void HalMock_SetBoardInitResult(bool ok)
{
    s_boardInitResult = ok;
}

void HalMock_SetAudioInitResult(bool ok)
{
    s_audioInitResult = ok;
}

void HalMock_Boundary(void)
{
    if (s_onBoundary != NULL)
    {
        s_onBoundary();
    }
}

void HalMock_SaiError(void)
{
    if (s_onError != NULL)
    {
        s_onError();
    }
}

uint32_t HalMock_AudioInitCalls(void)
{
    return s_audioInitCalls;
}

uint32_t HalMock_AudioStartCalls(void)
{
    return s_audioStartCalls;
}

uint32_t HalMock_AudioSubmits(void)
{
    return s_audioSubmits;
}

uint32_t HalMock_AudioAbortCalls(void)
{
    return s_audioAbortCalls;
}

uint32_t HalMock_AudioStops(void)
{
    return s_audioStops;
}

uint16_t *HalMock_RingWords(void)
{
    return s_ring;
}

/* ---- hal_gpio.h ----------------------------------------------------- */

bool HalGpio_ReadSw2(void)
{
    return s_sw2;
}

void HalMock_SetSw2(bool pressed)
{
    s_sw2 = pressed;
}
