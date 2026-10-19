#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Upper bound on PLL lock status reads in HalBoard_Init. Each read already
 * waits for lock up to the vendor clock driver's own timeout. */
#define HAL_BOARD_PLL_STATUS_READS 4u

/* One-time board bring-up: system clocks (PLL lock polled, then distributed),
 * pin multiplexing, interrupt controller table and the I2C master.
 * Returns false when the PLL did not lock or a peripheral init failed. */
bool HalBoard_Init(void);

#endif /* HAL_BOARD_H */
