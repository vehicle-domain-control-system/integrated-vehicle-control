#ifndef HAL_MOCK_H
#define HAL_MOCK_H

/* Host stand-in for every function in src/hal/include. Records I2C
 * transfers and timer waits in one ordered log, serves scripted read data,
 * owns the audio ring and lets a test raise the DMA boundary and SAI error
 * interrupts. The audio transmit stand-in keeps the same start and stop
 * latches as the board adapter: one submission, one stop, no start after a
 * stop or a failed init. */

#include <stdbool.h>
#include <stdint.h>

#include "../../src/hal/include/hal_board.h"
#include "../../src/hal/include/hal_timer.h"
#include "../../src/hal/include/hal_i2c.h"
#include "../../src/hal/include/hal_audio_tx.h"
#include "../../src/hal/include/hal_gpio.h"

#define HAL_MOCK_LOG_MAX       128u
#define HAL_MOCK_MAX_BYTES     4u
#define HAL_MOCK_READ_MAX      64u
#define HAL_MOCK_RING_WORDS    (HAL_AUDIO_PRIME_WORDS + 2u * HAL_AUDIO_HALF_WORDS)

typedef enum {
    HAL_MOCK_I2C_WRITE = 0,
    HAL_MOCK_I2C_READ,
    HAL_MOCK_WAIT
} HalMockOpKind;

typedef struct {
    HalMockOpKind kind;
    uint8_t  addr;
    uint8_t  len;
    uint8_t  bytes[HAL_MOCK_MAX_BYTES];  /* written bytes, or bytes returned by a read */
    bool     sendStop;
    uint32_t ticks;                      /* HAL_MOCK_WAIT */
    bool     failed;
} HalMockOp;

typedef struct {
    HalMockOp ops[HAL_MOCK_LOG_MAX];
    uint32_t  count;
    bool      overflow;
} HalMockLog;

/* Clears every recorded and scripted state. */
void HalMock_Reset(void);

const HalMockLog *HalMock_Log(void);
void HalMock_ClearLog(void);

/* Read data is served in the order queued; an empty queue returns zeros. */
void HalMock_QueueRead(const uint8_t *bytes, uint32_t len);

/* The I2C transfer with this 0-based index (counted from the last reset or
 * log clear, writes and reads together) returns HAL_I2C_ERROR. */
void HalMock_FailI2cAt(uint32_t index);

void     HalMock_SetTicks(uint32_t ticks);
uint32_t HalMock_TickReads(void);

/* Raises the DMA boundary interrupt the next time HalTimer_GetTicks() has
 * been called this many more times (1 = on the next call), before that call
 * returns. Used to land a boundary inside a ring write. */
void HalMock_BoundaryOnTickRead(uint32_t nthRead);

/* Called right after a boundary raised by HalMock_BoundaryOnTickRead, still
 * inside the interrupted call, so a test can observe the interrupt's effect
 * before the main-context code resumes. NULL disables it. */
void HalMock_SetBoundaryProbe(void (*probe)(void));

/* Raises the SAI error interrupt the next time HalTimer_GetTicks() has been
 * called this many more times, like HalMock_BoundaryOnTickRead. */
void HalMock_SaiErrorOnTickRead(uint32_t nthRead);

void HalMock_SetBoardInitResult(bool ok);
void HalMock_SetTimerInitResult(bool ok);
void HalMock_SetAudioInitResult(bool ok);

/* Interrupt entry points: call the callbacks registered by HalAudioTx_Init. */
void HalMock_Boundary(void);
void HalMock_SaiError(void);

/* Result HalAudioTx_AbortState reports once the transfer was stopped
 * (default HAL_AUDIO_TX_ABORT_STOPPED). */
void HalMock_SetAbortResult(HalAudioTxAbortState state);

uint32_t HalMock_AudioInitCalls(void);
uint32_t HalMock_AudioStartCalls(void);   /* every HalAudioTx_Start call */
uint32_t HalMock_AudioSubmits(void);      /* calls that submitted the ring */
uint32_t HalMock_AudioAbortCalls(void);   /* every HalAudioTx_Abort call */
uint32_t HalMock_AudioStops(void);        /* calls that stopped the transfer */
uint16_t *HalMock_RingWords(void);

void HalMock_SetSw2(bool pressed);

#endif /* HAL_MOCK_H */
