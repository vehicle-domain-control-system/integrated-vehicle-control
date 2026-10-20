#ifndef OSIF_H
#define OSIF_H
/* Host mock of the RTD OsIf counter API subset (same signatures as RTD).
 * Independently written; NOT the NXP file. */
#include "Std_Types.h"

typedef enum {
    OSIF_COUNTER_DUMMY,
    OSIF_COUNTER_SYSTEM,
    OSIF_COUNTER_CUSTOM
} OsIf_CounterType;

void OsIf_Init(const void* Config);
uint32 OsIf_GetCounter(OsIf_CounterType SelectedCounter);
uint32 OsIf_GetElapsed(uint32 * const CurrentRef, OsIf_CounterType SelectedCounter);
void OsIf_SetTimerFrequency(uint32 Freq, OsIf_CounterType SelectedCounter);
uint32 OsIf_MicrosToTicks(uint32 Micros, OsIf_CounterType SelectedCounter);

#endif
