#ifndef SW2_BUTTON_H
#define SW2_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

/* Debounced user switch SW2. Ticks are HalTimer ticks (1.25 ms). */

/* Takes the current level as the settled state, so a switch held at power-up
 * does not produce a press. */
void Sw2Button_Init(uint32_t nowTick);

/* Samples the switch every 4 ticks (5 ms). A level is accepted after 4
 * equal samples in a row; a released-to-pressed change queues one press. */
void Sw2Button_Process(uint32_t nowTick);

/* Consumes one queued press. */
bool Sw2Button_TakePressEvent(void);

#endif /* SW2_BUTTON_H */
