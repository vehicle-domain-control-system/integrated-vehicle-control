/* R4_PROPOSED_HOST. Package-local Host API, not an adopted Network API.
 * No packed structs, SDK/HAL, permission policy, producer, or session allocator.
 * All multi-byte wire fields are written explicitly, never via sizeof(struct).
 */
#ifndef I1_HOST_CODEC_H
#define I1_HOST_CODEC_H
#include <stddef.h>
#include <stdint.h>

enum {
    I1_UART_C = 1, I1_UART_M = 2,
    I1_UART_PAYLOAD_MAX = 128, I1_UART_OVERHEAD = 10, I1_UART_FRAME_MAX = 138,
    I1_CAN_USED = 19, I1_CAN_BYTES = 20, I1_CAN_DLC = 11,
    I1_CIS_REAR_ID = 0x0A0, I1_DOMAIN_PERMISSION_ID = 0x080
};
typedef enum {
    I1_OK = 0, I1_NEED_MORE = 1,
    I1_ERR_ARGUMENT = -1, I1_ERR_CAPACITY = -2, I1_ERR_SYNC = -3,
    I1_ERR_VERSION = -4, I1_ERR_TYPE = -5, I1_ERR_LENGTH = -6,
    I1_ERR_CRC = -7, I1_ERR_ID = -8, I1_ERR_DLC = -9,
    I1_ERR_PADDING = -10, I1_ERR_VALUE = -11, I1_ERR_RESERVED = -12
} i1_status;

typedef struct {
    uint8_t version, type;
    uint16_t link_sequence, payload_length;
    uint8_t payload[I1_UART_PAYLOAD_MAX];
} i1_uart_frame;

/* Streaming state is Host-testable; the caller drains next() after every push.
 * Separate contexts are required for the two links/directions. No global runtime.
 */
typedef struct {
    uint8_t bytes[I1_UART_FRAME_MAX];
    size_t used;
    uint8_t expected_version;
    uint32_t rejected, skipped, frames;
} i1_uart_stream;

typedef struct {
    uint32_t boot_id, tx_sequence, update_sequence;
    uint16_t source_age_ms, rear_distance_deci_cm;
    uint8_t validity, quality_reason, proximity_status;
} i1_cis_rear;
typedef struct {
    uint32_t boot_id, tx_sequence;
    uint8_t vehicle_power_permission, power_permission_quality;
    uint8_t power_operation_state, operation_allowed, operation_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} i1_domain_permission;

/* CRC(NULL,0) is 0xFFFF; data must be non-NULL for nonzero length. */
uint16_t i1_crc16(const uint8_t *data, size_t length);

/* Envelope-only operations: TYPE/payload semantics are intentionally opaque.
 * I1_OK here means a valid bounded envelope, NOT a supported/usable message.
 * Used to test the common grammar at P=0 and P=128 without inventing Wire types.
 */
i1_status i1_uart_envelope_encode(uint8_t version, uint8_t type, uint16_t sequence,
    const uint8_t *payload, size_t length, uint8_t *out, size_t capacity, size_t *written);
i1_status i1_uart_envelope_decode(uint8_t expected_version,
    const uint8_t *data, size_t length, i1_uart_frame *out);

/* Supported candidate schemas: C 60/19,61/16,62/15; M 10/24 only.
 * UART-C payload remains opaque; these are NOT final Pi message codecs.
 * UART-M M_REQUEST payload must additionally pass its typed decoder.
 */
i1_status i1_uart_encode(uint8_t version, uint8_t type, uint16_t sequence,
    const uint8_t *payload, size_t length, uint8_t *out, size_t capacity, size_t *written);
i1_status i1_uart_decode(uint8_t expected_version,
    const uint8_t *data, size_t length, i1_uart_frame *out);
void i1_uart_stream_init(i1_uart_stream *stream, uint8_t expected_version);
i1_status i1_uart_stream_push(i1_uart_stream *stream, uint8_t byte);
i1_status i1_uart_stream_next(i1_uart_stream *stream, i1_uart_frame *out);
/* EOF/deadline owner can abandon the first partial candidate byte and then drain.
 * No wall clock or adopted 50 ms deadline is imposed by this Host codec.
 */
void i1_uart_stream_abandon(i1_uart_stream *stream);

/* Encode writes exactly 20 B, including zero padding; *written is zero on failure.
 * Decode checks ID, DLC CODE 11, actual BYTE count 20, padding, reserved Boot=0,
 * and documented raw codes. It preserves stale/invalid/unknown data unchanged.
 * No coherence, freshness, physical sensor range, ALLOW, or admission policy.
 * Output is unchanged on failure. Input and output buffers must not overlap.
 */
i1_status i1_cis_rear_encode(const i1_cis_rear *fields, uint8_t *out,
    size_t capacity, size_t *written);
i1_status i1_cis_rear_decode(uint32_t id, uint8_t dlc, const uint8_t *data,
    size_t length, i1_cis_rear *out);
i1_status i1_domain_permission_encode(const i1_domain_permission *fields,
    uint8_t *out, size_t capacity, size_t *written);
i1_status i1_domain_permission_decode(uint32_t id, uint8_t dlc,
    const uint8_t *data, size_t length, i1_domain_permission *out);
#endif
