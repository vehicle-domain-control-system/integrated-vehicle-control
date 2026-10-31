/* Host diagnostics for two WIRE_PROPOSED frames; no public Node API. */
#ifndef U3_RAW_DIAG_H
#define U3_RAW_DIAG_H
#include "i1/i1_host_codec.h"

typedef enum {
    U3_OK = I1_OK,
    U3_ERR_ARGUMENT = I1_ERR_ARGUMENT,
    U3_ERR_CAPACITY = I1_ERR_CAPACITY,
    U3_ERR_LENGTH = I1_ERR_LENGTH,
    U3_ERR_ID = I1_ERR_ID,
    U3_ERR_DLC = I1_ERR_DLC,
    U3_ERR_PADDING = I1_ERR_PADDING,
    U3_ERR_VALUE = I1_ERR_VALUE,
    U3_ERR_RESERVED = I1_ERR_RESERVED,
    U3_ERR_PROFILE = -20,
    U3_ERR_UNSUPPORTED = -21
} u3_status;

typedef struct {
    uint32_t can_id;
    uint8_t ide, fdf, brs, rtr, dlc;
    const uint8_t *payload;
    size_t length;
    uint32_t received_at_ms; /* caller's local RX time; never a source birth time */
} u3_raw_frame;

typedef struct {
    uint32_t can_id, received_at_ms;
    uint8_t ide, fdf, brs, rtr, dlc;
    uint8_t raw[I1_CAN_BYTES];
    union { i1_cis_rear rear; i1_domain_permission permission; } fields;
} u3_diagnostic;

/* Pure calls: no replay cache, liveness/freshness clock, policy or allocation.
 * Decode owns a bounded copy. On failure *out remains byte-for-byte unchanged.
 * Buffers/records must not overlap; caller supplies truthful readable lengths.
 */
u3_status u3_raw_decode(const u3_raw_frame *frame, u3_diagnostic *out);
/* Re-encode diagnostic fields through I1; produces bytes only, never submits TX.
 * Output untouched on failure, *written=0 (when supplied). Metadata is checked.
 * Permission bytes are diagnostic fixtures, not outgoing permission synthesis.
 */
u3_status u3_raw_encode(const u3_diagnostic *record, uint8_t *out,
    size_t capacity, size_t *written);
/* Deterministic bounded text, includes raw flags/bytes, identities, AGE and quality.
 * capacity includes the terminator. On failure no partial output, *written=0.
 * Caller must pass a record successfully decoded by u3_raw_decode, unmodified.
 */
u3_status u3_raw_format(const u3_diagnostic *record, char *out,
    size_t capacity, size_t *written);
#endif
