/*
 * Frame format of the UART link between the Raspberry Pi (camera) and the CIS.
 * This link is internal to the CIS module.
 *
 *   byte 0    0xA5                       start of frame (2 bytes)
 *   byte 1    0x5A
 *   byte 2    TYPE                       0x01 VISION (Pi -> CIS), 0x02 PERMISSION (CIS -> Pi)
 *   byte 3    LEN                        payload length (fixed for each TYPE)
 *   byte 4..  payload
 *   last 2    CRC16 (low byte first)     CRC-16/CCITT-FALSE over TYPE, LEN and payload
 *
 * All multi-byte numbers are little-endian. 115200 baud, 8N1, no flow control.
 *
 * VISION payload (8 bytes), sent by the Pi every 100 ms:
 *   0     SESSION   random number chosen when the Pi program starts; a different
 *                   value means the Pi restarted
 *   1..2  SEQ       +1 for every NEW camera judgement; repeated frames of the same
 *                   judgement keep the number
 *   3..4  AGE_MS    time since that judgement was made (saturates at 65535)
 *   5     COUNT     number of people 0..5, 255 = unknown
 *   6     QUALITY   0 = result is usable, otherwise a QUALITY_REASON code
 *                   (1 NOT_READY, 2 OUT_OF_RANGE, 3 SENSOR_FAULT, 4 VISION_FAULT, 6 NO_DATA)
 *   7     FAULT     0 none, 1 initialization, 2 camera, 3 detector, 4 data error
 *
 * PERMISSION payload (6 bytes), sent by the CIS every 500 ms:
 *   0     SESSION   changes when the CIS restarts
 *   1..2  SEQ       +1 for every frame sent
 *   3     PERMISSION  0 = denied, 1 = allowed, 255 = unknown
 *   4..5  SOURCE_AGE  age of the permission information in ms (65535 = unknown)
 *
 * The presence of occupants is not sent: the CIS derives it from COUNT, so the two
 * values always belong to the same judgement and can never contradict each other.
 */
#ifndef PI_PROTOCOL_H
#define PI_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PI_SOF0                 0xA5u
#define PI_SOF1                 0x5Au

#define PI_TYPE_VISION          0x01u
#define PI_TYPE_PERMISSION      0x02u

#define PI_VISION_PAYLOAD_LEN       8u
#define PI_PERMISSION_PAYLOAD_LEN   6u
#define PI_FRAME_OVERHEAD           6u      /* SOF(2) + TYPE + LEN + CRC(2) */
#define PI_FRAME_MAX                (PI_FRAME_OVERHEAD + PI_VISION_PAYLOAD_LEN)

#define PI_COUNT_UNKNOWN        255u
#define PI_PERMISSION_DENIED    0u
#define PI_PERMISSION_ALLOWED   1u
#define PI_PERMISSION_UNKNOWN   255u

typedef struct {
    uint8_t  session;
    uint16_t seq;
    uint16_t age_ms;
    uint8_t  count;
    uint8_t  quality;
    uint8_t  fault;
} pi_vision_t;

typedef struct {
    uint8_t  type;
    pi_vision_t vision;         /* valid when type == PI_TYPE_VISION */
} pi_frame_t;

typedef struct {
    uint8_t  buf[PI_FRAME_MAX];
    uint8_t  n;
    uint32_t frames_ok;
    uint32_t bad_crc;
    uint32_t bad_format;        /* unknown TYPE or wrong LEN */
    uint32_t skipped_bytes;     /* bytes dropped while looking for a frame start */
} pi_parser_t;

void pi_parser_init(pi_parser_t *p);

/* Adds one received byte. Call pi_parser_step() afterwards. */
void pi_parser_push(pi_parser_t *p, uint8_t byte);

/* Looks for a complete valid frame in the bytes collected so far.
 * Returns true and fills `out` when one was found; call again until it returns
 * false. A damaged frame is skipped without changing any state elsewhere. */
bool pi_parser_step(pi_parser_t *p, pi_frame_t *out);

/* Builders. Return the number of bytes written (buffer must hold PI_FRAME_MAX). */
size_t pi_build_vision(uint8_t *out, const pi_vision_t *v);
size_t pi_build_permission(uint8_t *out, uint8_t session, uint16_t seq,
                           uint8_t permission, uint16_t source_age_ms);

#endif /* PI_PROTOCOL_H */
