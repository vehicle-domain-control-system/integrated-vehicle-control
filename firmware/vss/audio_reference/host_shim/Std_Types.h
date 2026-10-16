#ifndef VSS_AUDIO_REFERENCE_HOST_STD_TYPES_H
#define VSS_AUDIO_REFERENCE_HOST_STD_TYPES_H

/* Portable reference build only. This is not an S32K/RTD SDK replacement. */
#ifndef VSS_AUDIO_REFERENCE_HOST
#error Define VSS_AUDIO_REFERENCE_HOST only for the isolated host reference build
#endif
#include <stdint.h>
#include <stddef.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int16_t sint16;
typedef uint8_t boolean;
#define TRUE ((boolean)1U)
#define FALSE ((boolean)0U)
#define NULL_PTR ((void *)0)

#endif
