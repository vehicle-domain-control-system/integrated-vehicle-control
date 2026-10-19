#include "sw2_button.h"
#include "../../hal/include/hal_gpio.h"

#define SW2_SAMPLE_PERIOD_TICKS 4U
#define SW2_DEBOUNCE_SAMPLES    4U

static uint8_t  s_candidate;
static uint8_t  s_candidateCount;
static uint8_t  s_debounced;
static uint32_t s_lastSampleTick;
static uint32_t s_pendingPresses;

/* Counters for the debugger. */
static volatile uint32_t s_pressCount;
static volatile uint32_t s_releaseCount;

static uint8_t Sw2Button_ReadRaw(void)
{
    return HalGpio_ReadSw2() ? 1U : 0U;
}

static void Sw2Button_Sample(void)
{
    uint8_t raw = Sw2Button_ReadRaw();
    uint8_t previous;

    if (raw != s_candidate)
    {
        s_candidate = raw;
        s_candidateCount = 1U;
    }
    else if (s_candidateCount < SW2_DEBOUNCE_SAMPLES)
    {
        s_candidateCount++;
    }
    else
    {
        /* stable */
    }

    if ((s_candidateCount >= SW2_DEBOUNCE_SAMPLES) && (s_debounced != s_candidate))
    {
        previous = s_debounced;
        s_debounced = s_candidate;
        if ((previous == 0U) && (s_debounced == 1U))
        {
            s_pressCount++;
            s_pendingPresses++;
        }
        else
        {
            s_releaseCount++;
        }
    }
}

void Sw2Button_Init(uint32_t nowTick)
{
    uint8_t raw = Sw2Button_ReadRaw();

    s_debounced = raw;
    s_candidate = raw;
    s_candidateCount = SW2_DEBOUNCE_SAMPLES;
    s_lastSampleTick = nowTick;
    s_pendingPresses = 0U;
    s_pressCount = 0U;
    s_releaseCount = 0U;
}

void Sw2Button_Process(uint32_t nowTick)
{
    if ((nowTick - s_lastSampleTick) >= SW2_SAMPLE_PERIOD_TICKS)
    {
        s_lastSampleTick = nowTick;
        Sw2Button_Sample();
    }
}

bool Sw2Button_TakePressEvent(void)
{
    if (s_pendingPresses == 0U)
    {
        return false;
    }
    s_pendingPresses--;
    return true;
}
