#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>
#include <stdint.h>

/* Raw level of user switch SW2 (active high, no debouncing). */
bool HalGpio_ReadSw2(void);

#endif /* HAL_GPIO_H */
