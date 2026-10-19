#ifndef MOCK_STD_TYPES_H
#define MOCK_STD_TYPES_H

/* Stand-in for the vendor driver headers. Only the declarations the board
 * adapter uses are reproduced, with the same names, signatures and values as
 * the vendor release it targets. Used to compile-check sources when the
 * vendor headers are not installed; never linked. */

#include <stdint.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint8_t  boolean;
typedef uint8_t  Std_ReturnType;

#ifndef TRUE
#define TRUE  1U
#endif
#ifndef FALSE
#define FALSE 0U
#endif
#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif
#define E_OK     0x00U
#define E_NOT_OK 0x01U

#endif /* MOCK_STD_TYPES_H */
