/* Public headers compile standalone in C99 and carry the geometry and timing
 * constants the rest of the firmware relies on. */

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

/* One half is 1024 stereo frames of 16-bit samples. */
STATIC_CHECK(half_is_1024_frames, FRAMES_PER_HALF == 1024UL);
STATIC_CHECK(half_bytes_fit_dma_ring, (HAL_AUDIO_HALF_WORDS * 2UL * 2UL) == 8192UL);
STATIC_CHECK(prime_is_one_fifo_fill, HAL_AUDIO_PRIME_WORDS == 8UL);
STATIC_CHECK(tick_is_800_hz, HAL_TIMER_TICK_HZ == 800UL);

int main(void)
{
    AudioTxEvent ev;
    AudioControlStatus status;
    HalI2cStatus i2c = HAL_I2C_OK;

    /* Half-buffer deadline: 1024 frames at 48 kHz. */
    CHECK(HALF_DURATION_US > 21300UL && HALF_DURATION_US < 21400UL);

    /* The periodic flow evaluation must fit well inside one half-buffer. */
    CHECK((unsigned long)VSS_CFG_FLOW_PERIOD_MS * 1000UL < HALF_DURATION_US);

    /* Tick period in microseconds is 1.25 ms. */
    CHECK_EQ_U(1000000UL / HAL_TIMER_TICK_HZ, 1250UL);

    CHECK(VSS_CFG_VOLUME_PERCENT >= VSS_CFG_VOLUME_PERCENT_MIN);
    CHECK(VSS_CFG_VOLUME_PERCENT <= VSS_CFG_VOLUME_PERCENT_MAX);
    CHECK(VSS_CFG_STORE_RECLAIM_MS > 2000);
    CHECK_EQ_U(VSS_CFG_RX_GAP_BUDGET_MS, 5UL);

    /* Zero values are the "nothing happened" states. */
    CHECK_EQ_U(AUDIO_TX_EVENT_NONE, 0UL);
    CHECK_EQ_U(AUDIO_TX_OK, 0UL);
    CHECK_EQ_U(i2c, (unsigned long)HAL_I2C_OK);
    CHECK(HAL_I2C_ERROR != HAL_I2C_OK);

    ev.kind = AUDIO_TX_EVENT_HALF_FREE;
    ev.freeHalf = 1U;
    ev.generation = 7UL;
    ev.boundaryTick = 1UL;
    ev.faultMask = 0UL;
    CHECK(ev.kind == AUDIO_TX_EVENT_HALF_FREE && ev.freeHalf == 1U);

    status.clockLocked = false;
    status.codecConfigured = false;
    status.unmuted = false;
    status.muteLatched = false;
    status.i2cErrors = 0UL;
    CHECK(!status.clockLocked && !status.muteLatched);

    TEST_FINISH();
}
