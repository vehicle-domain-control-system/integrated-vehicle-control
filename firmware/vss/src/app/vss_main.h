#ifndef VSS_MAIN_H
#define VSS_MAIN_H

#include <stdbool.h>
#include <stdint.h>

/* Boot sequence and super-loop of the VSS image.
 *
 * Boot, in this order:
 *   HalBoard_Init
 *   HalTimer_Init                 the tick starts at 0 here: VSS start time is
 *                                 clock 0. Nothing else runs if it fails.
 *   VssTime_Init(HalTimer_GetTicks)
 *   Health_Init                   board init failure -> INIT fault
 *   AudioControl_Init             clock synthesizer and codec; failure -> INIT
 *                                 fault, and the output is not started
 *   AudioControl_Unmute           failure -> OUTPUT fault
 *   Sw2Button_Init, AudioAsset_Init, AudioStream_Init, VssTestInput_Init
 *   Flow_Init, Net_Init
 *   AudioTx_Init, AudioTx_Start   failure -> INIT fault
 *   Health_BootComplete
 * Every blocking I2C transfer and wait is done before AudioTx_Start: from
 * then on a half-buffer must be refilled before the second boundary
 * (about 43 ms after the start), or the output fails late.
 *
 * One super-loop pass:
 *   1. AudioStream_Service        refill first
 *   2. input delivery             Net_MainFunction(VssTime_NowMs32), and the
 *                                 built-in test input when VSS_CFG_TEST_INPUT
 *   3. Flow_Process               when a message reached the adapter in step 2,
 *                                 when the last pass asked for a follow-up, or
 *                                 VSS_CFG_FLOW_PERIOD_MS after the last pass;
 *                                 after step 2 because the pass reads the time
 *   4. output failure             the codec is muted at once; while the mute
 *                                 is not accepted it is tried again every
 *                                 VSS_MAIN_MUTE_RETRY_MS
 * No call in a pass blocks except the mute retries after an output failure. */

#define VSS_MAIN_MUTE_RETRY_MS 50u

typedef enum
{
    VSS_MAIN_STAGE_RESET = 0,
    VSS_MAIN_STAGE_TIMER_FAILED,   /* stopped: no tick, no clock */
    VSS_MAIN_STAGE_AUDIO_CONTROL,
    VSS_MAIN_STAGE_SERVICES,
    VSS_MAIN_STAGE_OUTPUT,
    VSS_MAIN_STAGE_RUNNING
} VssMainStage;

/* Debugger view. Counters saturate. */
typedef struct
{
    uint8_t stage;                 /* VssMainStage */
    bool boardOk;
    bool timerOk;
    bool audioControlOk;           /* clock synthesizer locked, codec configured */
    bool unmuteOk;
    bool outputStarted;            /* AudioTx_Init and AudioTx_Start succeeded */
    uint32_t loops;
    uint32_t flowPasses;
    uint32_t inputPasses;          /* flow passes after a delivered message */
    uint32_t followUpPasses;       /* flow passes the previous pass asked for */
    uint32_t loopMaxTicks;         /* longest pass, in HalTimer ticks */
    bool outputFailed;
    uint32_t muteCalls;
    bool muteAccepted;
} VssMainObservation;

/* Runs the boot sequence. False when the tick could not be started; the
 * caller must not run the super-loop then. */
bool VssMain_Boot(void);

/* One super-loop pass. */
void VssMain_LoopOnce(void);

bool VssMain_Read(VssMainObservation *out);

#endif /* VSS_MAIN_H */
