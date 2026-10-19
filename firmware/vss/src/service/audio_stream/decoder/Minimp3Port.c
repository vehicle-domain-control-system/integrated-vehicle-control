/*
 * minimp3 implementation translation unit.
 *
 * minimp3.h is the pinned upstream file with one change: the decoder scratch
 * object inside mp3dec_decode_frame is
 *
 *     static mp3dec_scratch_t scratch;
 *
 * instead of a local. The scratch object is much larger than the 4 KB the
 * board linker reserves for the interrupt/main stack, so it lives in BSS.
 *
 * This makes this minimp3 instance non-reentrant. There is one decoder call
 * path (the audio stream service), so that is intentional.
 */

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION

#include "minimp3.h"
