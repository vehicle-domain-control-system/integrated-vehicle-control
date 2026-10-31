/*
 * UART-C v1: frames between the Raspberry Pi (camera) and the CIS.
 * Defined in docs/network/interface/03_COMMUNICATION/30_UART_C.md (pull request #68,
 * status PROPOSED). All multi-byte numbers are little-endian.
 *
 *   byte 0..1  SYNC            0xA5 0x5A
 *   byte 2     VERSION         1
 *   byte 3     MESSAGE_TYPE    0x60 P_VISION_RESULT (Pi -> CIS), 0x61 P_VISION_STATUS (Pi -> CIS),
 *                              0x62 P_PERMISSION (CIS -> Pi)
 *   byte 4..5  PAYLOAD_LENGTH  u16, exact for each type (19, 16, 15)
 *   byte 6..7  LINK_SEQUENCE   u16, +1 for every frame sent in one direction (not a new-observation proof)
 *   byte 8..   PAYLOAD
 *   last 2     CRC16           CRC-16/CCITT-FALSE over VERSION .. end of payload (SYNC excluded)
 *
 * A frame has to be complete within PI_ASSEMBLY_TIMEOUT_MS of its first byte; a damaged or
 * late frame is dropped and the search for the next SYNC continues after its first byte.
 */
#ifndef PI_PROTOCOL_H
#define PI_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PI_SOF0                 0xA5u
#define PI_SOF1                 0x5Au
#define PI_VERSION              1u

#define PI_TYPE_VISION_RESULT   0x60u
#define PI_TYPE_VISION_STATUS   0x61u
#define PI_TYPE_PERMISSION      0x62u
#define PI_TYPE_BENCH_SENSORS   0x70u   /* NOT UART-C v1: bench-only report CIS -> Pi (CIS_BENCH_REPORT) */

#define PI_VISION_RESULT_LEN    19u
#define PI_VISION_STATUS_LEN    16u
#define PI_PERMISSION_LEN       15u
#define PI_BENCH_SENSORS_LEN    52u
#define PI_BENCH_FRAME_LEN      (PI_FRAME_OVERHEAD + PI_BENCH_SENSORS_LEN)

#define PI_FRAME_OVERHEAD       10u     /* SYNC(2) VERSION TYPE LEN(2) LINK_SEQ(2) CRC(2) */
#define PI_FRAME_MAX            (PI_FRAME_OVERHEAD + PI_VISION_RESULT_LEN)
#define PI_ASSEMBLY_TIMEOUT_MS  50u

/* Code values used inside the payloads */
#define PI_TRI_ABSENT           0u      /* OCCUPANT_PRESENCE: none */
#define PI_TRI_PRESENT          1u      /* OCCUPANT_PRESENCE: present */
#define PI_TRI_UNKNOWN          255u    /* presence / count: not known */
#define PI_INVALID              0u      /* validity flags */
#define PI_VALID                1u

#define PI_PERMISSION_DENIED    0u
#define PI_PERMISSION_ALLOWED   1u
#define PI_PERMISSION_UNKNOWN   255u

/* NetValueQuality (P_PERMISSION) */
#define PI_QUALITY_OK           0u
#define PI_QUALITY_STALE        1u
#define PI_QUALITY_INVALID      2u
#define PI_QUALITY_NO_DATA      3u

/* Vision fault codes */
#define PI_FAULT_NONE           0u
#define PI_FAULT_INIT           1u
#define PI_FAULT_CAMERA         2u
#define PI_FAULT_DETECTOR       3u
#define PI_FAULT_DATA           4u

/* AFFECTED_FUNCTION bits */
#define PI_AFFECTS_PRESENCE     0x01u
#define PI_AFFECTS_COUNT        0x02u

typedef struct {                        /* P_VISION_RESULT, Pi -> CIS */
    uint32_t pi_boot_id;                /* Pi start number, 0 is reserved */
    uint32_t vision_sequence;           /* +1 for every NEW camera judgement */
    uint16_t vision_age_ms;             /* age of the judgement when it was sent */
    uint8_t  presence;                  /* 0 none, 1 present, 255 unknown */
    uint8_t  presence_validity;         /* 0 INVALID, 1 VALID */
    uint8_t  presence_reason;           /* original quality reason from the Pi */
    uint8_t  count;                     /* 0..5, 255 unknown */
    uint8_t  count_validity;
    uint8_t  count_reason;
    uint8_t  vision_status;             /* function state: 0 NOT_READY 1 READY 2 ACTIVE 3 FAULT 4 RECOVERING */
    uint16_t fault_code;                /* PI_FAULT_* */
} pi_vision_result_t;

typedef struct {                        /* P_VISION_STATUS, Pi -> CIS */
    uint32_t pi_boot_id;
    uint32_t status_sequence;           /* +1 for every new function state evaluation */
    uint8_t  vision_status;
    uint16_t active_fault;
    uint16_t last_fault;
    uint16_t status_age_ms;
    uint8_t  affected_function;         /* PI_AFFECTS_* bits */
} pi_vision_status_t;

typedef struct {                        /* P_PERMISSION, CIS -> Pi */
    uint32_t cis_boot_id;               /* CIS start number, 0 is reserved */
    uint32_t update_sequence;           /* changes when the permission evaluation changes */
    uint8_t  permission;                /* PI_PERMISSION_* */
    uint8_t  value_quality;             /* PI_QUALITY_* */
    uint8_t  quality_reason;
    uint16_t use_limit_ms;              /* 1500 (proposal) */
    uint16_t source_age_ms;             /* age of the Domain permission, 65535 = unknown */
} pi_permission_t;

typedef struct {                        /* one sensor value of P_BENCH_SENSORS */
    uint32_t value;                     /* number (temperature: int16 in two's complement) */
    uint8_t  valid;
    uint8_t  reason;
    uint32_t sequence;
    uint16_t age_ms;
} pi_bench_value_t;

typedef struct {                        /* P_BENCH_SENSORS, CIS -> Pi, bench only (layout: tools/pi_vision/pi_protocol.py) */
    uint8_t  reported_status;           /* PROXIMITY_STATUS after the power gate */
    uint8_t  reported_valid;
    uint8_t  reported_reason;
    uint8_t  sensor_status;             /* PROXIMITY_STATUS before the power gate */
    pi_bench_value_t distance;          /* mm */
    pi_bench_value_t temperature;       /* 0.01 degC */
    pi_bench_value_t humidity;          /* 0.01 %RH */
    pi_bench_value_t illuminance;       /* lx */
    uint8_t  pi_link_up;
    uint8_t  occupant_count;
    uint8_t  occupant_presence;
    uint32_t vision_sequence;
    uint8_t  cis_state;
} pi_bench_sensors_t;

typedef struct {
    uint8_t  type;                      /* PI_TYPE_VISION_RESULT or PI_TYPE_VISION_STATUS */
    uint16_t link_seq;
    pi_vision_result_t result;          /* valid for PI_TYPE_VISION_RESULT */
    pi_vision_status_t status;          /* valid for PI_TYPE_VISION_STATUS */
} pi_frame_t;

typedef struct {
    uint8_t  buf[PI_FRAME_MAX];
    uint8_t  n;
    uint32_t t_start_ms;                /* time the oldest byte in buf arrived */
    uint32_t frames_ok;
    uint32_t bad_crc;
    uint32_t bad_format;                /* wrong VERSION, unknown or wrong-direction TYPE, wrong length */
    uint32_t assembly_timeouts;
    uint32_t skipped_bytes;             /* bytes dropped while looking for a frame start */
} pi_parser_t;

void pi_parser_init(pi_parser_t *p);

/* Adds one received byte. Call pi_parser_step() afterwards. */
void pi_parser_push(pi_parser_t *p, uint8_t byte, uint32_t now_ms);

/* Drops a frame that is not complete 50 ms after its first byte. Call regularly. */
void pi_parser_expire(pi_parser_t *p, uint32_t now_ms);

/* Looks for a complete valid frame in the bytes collected so far. Returns true and fills
 * `out` when one was found; call again until it returns false. */
bool pi_parser_step(pi_parser_t *p, pi_frame_t *out);

/* Builders. Return the number of bytes written (the buffer must hold PI_FRAME_MAX). */
size_t pi_build_vision_result(uint8_t *out, uint16_t link_seq, const pi_vision_result_t *v);
size_t pi_build_vision_status(uint8_t *out, uint16_t link_seq, const pi_vision_status_t *s);
size_t pi_build_bench_sensors(uint8_t *out, uint16_t link_seq, const pi_bench_sensors_t *b);
size_t pi_build_permission(uint8_t *out, uint16_t link_seq, const pi_permission_t *p);

#endif /* PI_PROTOCOL_H */
