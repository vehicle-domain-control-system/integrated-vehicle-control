#include "pi_protocol.h"
#include "crc16.h"

void pi_parser_init(pi_parser_t *p)
{
    p->n = 0;
    p->frames_ok = 0;
    p->bad_crc = 0;
    p->bad_format = 0;
    p->skipped_bytes = 0;
}

void pi_parser_push(pi_parser_t *p, uint8_t byte)
{
    if (p->n < PI_FRAME_MAX) {
        p->buf[p->n++] = byte;
    }
}

static void drop_first(pi_parser_t *p)
{
    uint8_t i;
    for (i = 1; i < p->n; i++) {
        p->buf[i - 1u] = p->buf[i];
    }
    if (p->n > 0) {
        p->n--;
    }
}

static uint8_t payload_len_for(uint8_t type)
{
    if (type == PI_TYPE_VISION) {
        return PI_VISION_PAYLOAD_LEN;
    }
    if (type == PI_TYPE_PERMISSION) {
        return PI_PERMISSION_PAYLOAD_LEN;
    }
    return 0u;
}

bool pi_parser_step(pi_parser_t *p, pi_frame_t *out)
{
    for (;;) {
        uint8_t len;
        uint8_t total;
        uint16_t crc_calc;
        uint16_t crc_rx;
        const uint8_t *pl;

        /* Find the start of a frame. */
        if (p->n >= 1u && p->buf[0] != PI_SOF0) {
            drop_first(p);
            p->skipped_bytes++;
            continue;
        }
        if (p->n >= 2u && p->buf[1] != PI_SOF1) {
            drop_first(p);
            p->skipped_bytes++;
            continue;
        }
        if (p->n < 4u) {
            return false;                           /* header not complete yet */
        }

        len = payload_len_for(p->buf[2]);
        if (len == 0u || p->buf[3] != len) {
            p->bad_format++;
            drop_first(p);
            continue;
        }

        total = (uint8_t)(PI_FRAME_OVERHEAD + len);
        if (p->n < total) {
            return false;                           /* frame not complete yet */
        }

        crc_calc = crc16_ccitt_false(&p->buf[2], (size_t)(2u + len));
        crc_rx = (uint16_t)((uint16_t)p->buf[total - 2u] | ((uint16_t)p->buf[total - 1u] << 8));
        if (crc_calc != crc_rx) {
            p->bad_crc++;
            drop_first(p);                          /* a real frame may start inside it */
            continue;
        }

        out->type = p->buf[2];
        pl = &p->buf[4];
        if (out->type == PI_TYPE_VISION) {
            out->vision.session = pl[0];
            out->vision.seq     = (uint16_t)((uint16_t)pl[1] | ((uint16_t)pl[2] << 8));
            out->vision.age_ms  = (uint16_t)((uint16_t)pl[3] | ((uint16_t)pl[4] << 8));
            out->vision.count   = pl[5];
            out->vision.quality = pl[6];
            out->vision.fault   = pl[7];
        }
        {
            uint8_t i;
            for (i = total; i < p->n; i++) {
                p->buf[i - total] = p->buf[i];
            }
            p->n = (uint8_t)(p->n - total);
        }
        p->frames_ok++;
        return true;
    }
}

static size_t finish_frame(uint8_t *out, uint8_t type, uint8_t len)
{
    uint16_t crc;

    out[0] = PI_SOF0;
    out[1] = PI_SOF1;
    out[2] = type;
    out[3] = len;
    crc = crc16_ccitt_false(&out[2], (size_t)(2u + len));
    out[4u + len] = (uint8_t)(crc & 0xFFu);
    out[5u + len] = (uint8_t)(crc >> 8);
    return (size_t)(PI_FRAME_OVERHEAD + len);
}

size_t pi_build_vision(uint8_t *out, const pi_vision_t *v)
{
    out[4] = v->session;
    out[5] = (uint8_t)(v->seq & 0xFFu);
    out[6] = (uint8_t)(v->seq >> 8);
    out[7] = (uint8_t)(v->age_ms & 0xFFu);
    out[8] = (uint8_t)(v->age_ms >> 8);
    out[9] = v->count;
    out[10] = v->quality;
    out[11] = v->fault;
    return finish_frame(out, PI_TYPE_VISION, PI_VISION_PAYLOAD_LEN);
}

size_t pi_build_permission(uint8_t *out, uint8_t session, uint16_t seq,
                           uint8_t permission, uint16_t source_age_ms)
{
    out[4] = session;
    out[5] = (uint8_t)(seq & 0xFFu);
    out[6] = (uint8_t)(seq >> 8);
    out[7] = permission;
    out[8] = (uint8_t)(source_age_ms & 0xFFu);
    out[9] = (uint8_t)(source_age_ms >> 8);
    return finish_frame(out, PI_TYPE_PERMISSION, PI_PERMISSION_PAYLOAD_LEN);
}
