/*
 * Stage 13 — minimp3 implementation translation unit.
 *
 * IMPORTANT:
 * Use the pinned upstream minimp3.h described in MINIMP3_SETUP.md.
 * Before building on S32K344, patch the decoder scratch object from:
 *
 *     mp3dec_scratch_t scratch;
 *
 * to:
 *
 *     static mp3dec_scratch_t scratch;
 *
 * The current project linker reserves only 4 KB for the interrupt/main stack.
 * minimp3's decoder scratch object is much larger than that, so this bring-up
 * deliberately moves the single decoder scratch object into BSS instead of
 * changing the linker stack.
 *
 * This makes this exact minimp3 instance non-reentrant. Stage 13 uses only
 * one decoder call path, so that is intentional.
 */

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION

#include "minimp3.h"
