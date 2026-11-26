#ifndef MOCK_RTD_H
#define MOCK_RTD_H
/* Test hooks of the host mock SDK. They play the role of the physical bus and
 * of the S32K144 debug node: inject frames into armed RX MBs (invoking the
 * generated-config callback exactly like the RTD IRQ handler), and pull
 * frames the node transmitted (invoking TX_COMPLETE). Not part of RTD.
 * Independently written; NOT the NXP file. */
#include <stdbool.h>
#include "FlexCAN_Ip.h"
#include "Lpuart_Uart_Ip.h"
#include "OsIf.h"

typedef struct {
    uint32 id;
    bool fd, brs, extended;
    uint8 dlc;
    uint8 length;
    uint8 data[64];
    /* RX only: the MB reports RX overrun (an earlier frame in it was
     * overwritten before the CPU read it), as the controller does. */
    bool overrun;
} MockCanFrame;

/* Time: OsIf system counter runs at 1 tick per microsecond and wraps at 32 bit. */
void MockRtd_Reset(void);
void MockRtd_AdvanceUs(uint64_t us);
uint64_t MockRtd_NowUs(void);
void MockRtd_SetSystemTimerEnabled(bool enabled); /* false = OsIf timer OFF (as in bring-up cfg) */

/* Bus -> node. Returns the MB index that received it, -1 when no MB was armed
 * (frame lost, as on hardware) or the controller is not started. */
int MockRtd_CanInject(uint8 instance, const MockCanFrame *frame);
/* Node -> bus. Arbitration: lowest identifier among pending TX MBs.
 * Fires TX_COMPLETE for that MB and returns its index, -1 if none pending.
 * With ACK missing, nothing completes (frames stay pending). A frame that
 * went out during an abort (NO_TRANSFER_IN_PROGRESS) comes first, without
 * TX_COMPLETE. */
int MockRtd_CanTransmitNext(uint8 instance, MockCanFrame *out);
void MockRtd_CanSetAckMissing(uint8 instance, bool missing);
void MockRtd_CanRaiseBusOff(uint8 instance);
uint32 MockRtd_CanAborts(uint8 instance);
/* Result of the next AbortTransfer on a pending TX MB (then back to SUCCESS),
 * as the RTD returns it:
 *   SUCCESS (default): aborted; the frame never goes out.
 *   NO_TRANSFER_IN_PROGRESS: the frame went out while the abort ran; it is
 *     taken by MockRtd_CanTransmitNext without TX_COMPLETE (the RTD disabled
 *     the MB interrupt in the abort).
 *   TIMEOUT: the abort did not finish; the MB stays in the controller (its
 *     message buffer flag clear) until MockRtd_CanFinishAbort. */
void MockRtd_CanSetAbortResult(uint8 instance, Flexcan_Ip_StatusType result);
/* The controller finishes a timed-out abort: the MB becomes idle and its
 * message buffer flag is set (no interrupt). */
void MockRtd_CanFinishAbort(uint8 instance);
/* The controller finishes a timed-out abort but the flag is gone: the RTD
 * cleared it right after the timeout, so the MB is idle and its message
 * buffer flag stays clear (nothing shows the MB free again). */
void MockRtd_CanFinishAbortFlagLost(uint8 instance);
/* A TX_COMPLETE interrupt for the MB whose abort timed out (the frame left
 * late): the configured Callback runs with TX_COMPLETE for that MB. The MB
 * stays with the controller and its flag is not set (MockRtd_CanFinishAbort
 * does that). Returns the MB, -1 when no abort is pending. */
int MockRtd_CanCompleteAbortingMb(uint8 instance);
/* Send calls on an MB whose timed-out abort had not finished yet. */
uint32 MockRtd_CanSendsOnAbortingMb(uint8 instance);
/* FlexCAN_Ip_Receive refuses (ERROR) while set and leaves the MB without a
 * reception: models a failed RX re-arm. */
void MockRtd_CanSetReceiveRefused(uint8 instance, bool refused);
/* RX MBs holding a reception (armed) now. */
uint32 MockRtd_CanRxArmed(uint8 instance);
/* The next count FlexCAN_Ip_Send calls on an idle MB return ERROR (a driver
 * refusal; the MB stays idle), then Send works again. */
void MockRtd_CanSetSendErrors(uint8 instance, uint32 count);

/* LPUART virtual line (instances 0..15). The node side uses the
 * Lpuart_Uart_Ip API; these hooks are the far end of the wire.
 * Line -> node: each byte goes into the armed RX buffer, then the configured
 * Callback runs with RX_FULL exactly as the RTD RX IRQ does (the callback
 * re-arms with SetRxBuffer); END_TRANSFER follows only if it did not.
 * A byte that finds no armed reception is lost on this node (counted).
 * Returns the number of bytes the driver took. */
uint32 MockRtd_UartInject(uint8 instance, const uint8 *bytes, uint32 count);
/* Node -> line: takes up to max bytes of the active AsyncSend transfer.
 * When the last byte leaves, TX_EMPTY and then END_TRANSFER run (RTD TX
 * complete). Returns the number of bytes taken; 0 when idle or stalled. */
uint32 MockRtd_UartTransmitTake(uint8 instance, uint8 *out, uint32 max);
/* Receive error as raised by the RTD error IRQ: the status is stored as the
 * receive status, the reception is completed (no longer armed) and the
 * Callback runs with ERROR. */
void MockRtd_UartRaiseError(uint8 instance, Lpuart_Uart_Ip_StatusType status);
#if (LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT == STD_ON)
/* Receive idle line as raised by the RTD error IRQ with the timeout
 * interrupt: the reception is completed (no longer armed), the receive status
 * is RX_IDLE_STATE and the Callback runs with IDLE_STATE. */
void MockRtd_UartRaiseIdle(uint8 instance);
#endif
/* Stalled line: TransmitTake moves nothing (no peer reading, flow stuck). */
void MockRtd_UartSetTxStall(uint8 instance, bool stall);
/* AsyncReceive refuses (ERROR) while set: models a failed re-arm. */
void MockRtd_UartSetReceiveRefused(uint8 instance, bool refused);
/* AbortSendingData returns ERROR while set and the active transfer goes on
 * (the driver still reads its buffer, transmit status BUSY). */
void MockRtd_UartSetAbortRefused(uint8 instance, bool refused);
/* The next AsyncSend finds an END_TRANSFER interrupt pending (a reception
 * that ended): it runs at once when interrupts are not masked
 * (net_board_irq_save), otherwise it waits for MockRtd_RunPendingIrq. */
void MockRtd_UartEndTransferAtSend(uint8 instance);
/* The next count AsyncSend calls on an idle line return ERROR (a driver
 * refusal; nothing starts), then AsyncSend works again. */
void MockRtd_UartSetSendErrors(uint8 instance, uint32 count);
/* Runs the interrupts raised while masked, once nothing is masked. */
void MockRtd_RunPendingIrq(void);
bool MockRtd_UartRxArmed(uint8 instance);
uint32 MockRtd_UartTxRemaining(uint8 instance);  /* bytes of the active transfer not yet taken */
uint32 MockRtd_UartSends(uint8 instance);        /* AsyncSend calls that started a transfer */
uint32 MockRtd_UartAborts(uint8 instance);       /* AbortSendingData calls that stopped a transfer */
uint32 MockRtd_UartRxLost(uint8 instance);       /* injected bytes with no armed reception */
uint32 MockRtd_UartInits(uint8 instance);

/* DLC helpers shared by the simulation. */
uint8 MockRtd_DlcForLength(uint8 length);

#endif
