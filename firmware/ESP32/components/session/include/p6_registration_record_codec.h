#ifndef P6_REGISTRATION_RECORD_CODEC_H
#define P6_REGISTRATION_RECORD_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "p6_registration_store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define P6_REGISTRATION_RECORD_BLOB_SIZE 28U

bool p6_registration_record_encode(
    const p6_registration_record_t *record,
    uint8_t out_blob[P6_REGISTRATION_RECORD_BLOB_SIZE]);
bool p6_registration_record_decode(
    const uint8_t *blob,
    size_t blob_length,
    p6_registration_record_t *out_record);

#ifdef __cplusplus
}
#endif

#endif
