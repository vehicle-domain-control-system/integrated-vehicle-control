#ifndef STD_TYPES_H
#define STD_TYPES_H
/* Host mock of the AUTOSAR/RTD platform types used by the Network RTD HAL
 * (minimal subset).
 * Independently written; NOT the NXP file. */
#include <stdint.h>
#include <stddef.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;
typedef unsigned char boolean;
typedef uint8 Std_ReturnType;

#ifndef TRUE
#define TRUE  ((boolean)1)
#endif
#ifndef FALSE
#define FALSE ((boolean)0)
#endif
#define E_OK     ((Std_ReturnType)0x00U)
#define E_NOT_OK ((Std_ReturnType)0x01U)
#define STD_ON   0x01U
#define STD_OFF  0x00U
#define NULL_PTR ((void *)0)

#endif
