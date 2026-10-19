#ifndef VSS_TEST_INPUT_H
#define VSS_TEST_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/* Built-in input sequence for bench tests without CAN (VSS_CFG_TEST_INPUT=1).
 * Every debounced SW2 press moves one step on and sends the step's message
 * through the same Network adapter the product callbacks use, with the TEST
 * origin. Warning frames are sent every VSS_CFG_TEST_WARNING_PERIOD_MS and at
 * once when a step changes a warning.
 *
 *   step  input                                   expected
 *    0    boot, every warning CLEAR               nothing plays
 *    1    Event WELCOME (new occurrence)          WELCOME once
 *    2    step 1 retransmitted: same occurrence,  nothing plays (the use
 *         next TX, new receive time, age grown    limit has passed: refused)
 *    3    Event LOCK_COMPLETE                     LOCK
 *    4    Event UNLOCK_COMPLETE                   UNLOCK
 *    5    Event LOCK_ERROR                        LOCK_ERROR
 *    6    Event GOODBYE                           GOODBYE
 *    7    rear obstacle CAUTION                   REAR_CAUTION, repeated
 *    8    rear EMERGENCY                          REAR_EMERGENCY preempts
 *    9    anti-pinch ACTIVE                       ANTI_PINCH preempts (highest)
 *   10    anti-pinch CLEAR                        REAR_EMERGENCY again
 *   11    occupant hazard ACTIVE                  no change (below rear emergency)
 *   12    rear CLEAR                              OCCUPANT
 *   13    occupant CLEAR                          stops
 *   14    rear quality INVALID (value CLEAR)      no change (INVALID is not CLEAR)
 *   15    rear quality OK, EMERGENCY              REAR_EMERGENCY
 *   16    rear CLEAR                              stops
 *   17    Event WELCOME (new occurrence)          WELCOME
 *   18    end of input                            later presses ignored, no
 *                                                 more frames, silence
 *
 * Messages: boot_id 1, Event and Warning TX counted separately from 1,
 * received_at_ms = VssTime_NowMs32. Events: age 0, use limit 500 ms,
 * quality OK, available. Warnings: activation metadata format 1 with the
 * activation ENABLED, every lane from source boot 1 with a sequence that
 * grows each frame (each frame is a new evaluation), age 0. While the rear
 * obstacle quality is INVALID its frames keep the sequence of the last valid
 * evaluation and carry the time since it as age (a lost observation keeps
 * its original number and age; it is not a new evaluation).
 *
 * Runs in the super-loop owner context. */

#define VSS_TEST_INPUT_LAST_STEP 18u

typedef struct
{
    uint8_t step;              /* 0..18 */
    uint32_t presses;          /* every debounced press, ignored ones included */
    uint32_t accepted;         /* Events INPUT took as a new occurrence */
    uint32_t duplicates;       /* Events INPUT found unchanged, or STORE took as a
                                  duplicate of a known occurrence (step 2 pressed
                                  inside the use limit of step 1) */
    uint32_t rejected;         /* Events INPUT refused */
    uint32_t warningFrames;
    bool fault;                /* the adapter dropped a message (latched) */
} VssTestInputObservation;

/* Step 0, no message sent yet. Called once at boot after Sw2Button_Init. */
void VssTestInput_Init(void);

/* Samples SW2 at nowTick (HalTimer ticks), applies a new step and sends the
 * due warning frame. */
void VssTestInput_Service(uint32_t nowTick);

bool VssTestInput_Read(VssTestInputObservation *out);

#endif /* VSS_TEST_INPUT_H */
