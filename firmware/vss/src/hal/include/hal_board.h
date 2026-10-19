#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* One-time board bring-up: system clocks (PLL lock checked and distributed),
 * pin multiplexing, interrupt controller table and the I2C master.
 * Returns false when the PLL did not lock or a peripheral init failed. */
bool HalBoard_Init(void);

#endif /* HAL_BOARD_H */
