#ifndef MOCK_RTD_H
#define MOCK_RTD_H
/* Test hooks of the host mock SDK. They play the role of the physical bus and
 * of the S32K144 debug node: inject frames into armed RX MBs (invoking the
 * generated-config callback exactly like the RTD IRQ handler), and pull
 * frames the node transmitted (invoking TX_COMPLETE). Not part of RTD.
 * Independently written; NOT the NXP file. */
#include <stdbool.h>
#include "FlexCAN_Ip.h"
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
 * With ACK missing, nothing completes (frames stay pending). */
int MockRtd_CanTransmitNext(uint8 instance, MockCanFrame *out);
void MockRtd_CanSetAckMissing(uint8 instance, bool missing);
void MockRtd_CanRaiseBusOff(uint8 instance);
uint32 MockRtd_CanAborts(uint8 instance);

/* DLC helpers shared by the simulation. */
uint8 MockRtd_DlcForLength(uint8 length);

#endif
