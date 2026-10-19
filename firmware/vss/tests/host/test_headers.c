/* Public headers compile standalone in C99 and carry the geometry and timing
 * constants the rest of the firmware relies on. Everything that is a
 * constant is checked at compile time; a failed STATIC_CHECK stops the
 * build. Range limits of the build switches are enforced by vss_config.h
 * itself and by the configuration guards in host_test.py. */

#include "test_util.h"

#include "../../src/infra/vss_config.h"
#include "../../src/hal/include/hal_board.h"
#include "../../src/hal/include/hal_timer.h"
#include "../../src/hal/include/hal_i2c.h"
#include "../../src/hal/include/hal_audio_tx.h"
#include "../../src/hal/include/hal_gpio.h"
#include "../../src/driver/audio_tx/audio_tx.h"
#include "../../src/driver/audio_control/audio_control.h"

#define SAMPLE_RATE_HZ        48000UL
#define CHANNELS              2UL
#define FRAMES_PER_HALF       (HAL_AUDIO_HALF_WORDS / CHANNELS)
#define HALF_DURATION_US      (FRAMES_PER_HALF * 1000000UL / SAMPLE_RATE_HZ)
#define TICK_US               (1000000UL / HAL_TIMER_TICK_HZ)

#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

/* One half is 1024 stereo frames of 16-bit samples. */
STATIC_CHECK(half_is_1024_frames, FRAMES_PER_HALF == 1024UL);
STATIC_CHECK(ring_halves_are_8192_bytes, (HAL_AUDIO_HALF_WORDS * 2UL * 2UL) == 8192UL);
STATIC_CHECK(prime_is_8_words, HAL_AUDIO_PRIME_WORDS == 8UL);
STATIC_CHECK(tick_is_800_hz, HAL_TIMER_TICK_HZ == 800UL);
STATIC_CHECK(tick_is_1250_us, TICK_US == 1250UL);

/* Half-buffer deadline: 1024 frames at 48 kHz is 21.33 ms. */
STATIC_CHECK(half_lasts_21_3_ms, HALF_DURATION_US > 21300UL && HALF_DURATION_US < 21400UL);

/* The periodic flow evaluation must fit inside one half-buffer. */
STATIC_CHECK(flow_period_fits_a_half, (unsigned long)VSS_CFG_FLOW_PERIOD_MS * 1000UL < HALF_DURATION_US);

/* The missing-boundary limit lies beyond the longest regular gap (the prime
 * words plus one half after Start) and below four halves. */
STATIC_CHECK(stall_limit_above_one_half,
             (unsigned long)AUDIO_TX_STALL_TICKS * TICK_US > 2UL * HALF_DURATION_US);
STATIC_CHECK(stall_limit_below_four_halves,
             (unsigned long)AUDIO_TX_STALL_TICKS * TICK_US < 4UL * HALF_DURATION_US);

/* Fault bits are distinct single bits. */
STATIC_CHECK(fault_bits_distinct,
             (AUDIO_TX_FAULT_LATE | AUDIO_TX_FAULT_OVERLAP | AUDIO_TX_FAULT_SAI | AUDIO_TX_FAULT_STALL) == 0xFUL);

/* Zero values are the "nothing happened" states. */
STATIC_CHECK(event_none_is_zero, AUDIO_TX_EVENT_NONE == 0);
STATIC_CHECK(tx_ok_is_zero, AUDIO_TX_OK == 0);
STATIC_CHECK(abort_none_is_zero, AUDIO_TX_ABORT_NONE == 0);
STATIC_CHECK(hal_abort_none_is_zero, HAL_AUDIO_TX_ABORT_NONE == 0);
STATIC_CHECK(i2c_ok_differs_from_error, HAL_I2C_OK != HAL_I2C_ERROR);

/* Fields the event and status consumers read, with their widths. */
STATIC_CHECK(event_generation_is_32_bit, MEMBER_SIZE(AudioTxEvent, generation) == 4U);
STATIC_CHECK(event_boundary_tick_is_32_bit, MEMBER_SIZE(AudioTxEvent, boundaryTick) == 4U);
STATIC_CHECK(event_fault_mask_is_32_bit, MEMBER_SIZE(AudioTxEvent, faultMask) == 4U);
STATIC_CHECK(event_free_half_is_8_bit, MEMBER_SIZE(AudioTxEvent, freeHalf) == 1U);
STATIC_CHECK(status_mute_latched_is_bool, MEMBER_SIZE(AudioControlStatus, muteLatched) == sizeof(bool));
STATIC_CHECK(status_mute_accepted_is_bool, MEMBER_SIZE(AudioControlStatus, muteAccepted) == sizeof(bool));
STATIC_CHECK(status_i2c_errors_is_32_bit, MEMBER_SIZE(AudioControlStatus, i2cErrors) == 4U);

int main(void)
{
    /* Read through volatile so the conversion happens at run time: the
     * target's plain char is unsigned and the host build must match. */
    volatile int all_ones = 0xFF;

    CHECK((char)all_ones > 0);

    /* Default receive-path allowance (a default, not a range limit). */
    CHECK_EQ_U(VSS_CFG_RX_GAP_BUDGET_MS, 5UL);

    TEST_FINISH();
}
