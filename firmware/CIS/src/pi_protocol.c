#include "pi_protocol.h"
#include "crc16.h"

static uint16_t rd16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFFu); p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu); p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu); p[3] = (uint8_t)(v >> 24);
}

void pi_parser_init(pi_parser_t *p)
{
    p->n = 0;
    p->t_start_ms = 0;
    p->frames_ok = 0;
    p->bad_crc = 0;
    p->bad_format = 0;
    p->assembly_timeouts = 0;
    p->skipped_bytes = 0;
}

void pi_parser_push(pi_parser_t *p, uint8_t byte, uint32_t now_ms)
{
    if (p->n == 0u) {
        p->t_start_ms = now_ms;
    }
    if (p->n < PI_FRAME_MAX) {
        p->buf[p->n++] = byte;
    }
}

static void drop_first(pi_parser_t *p, uint32_t now_ms)
{
    uint8_t i;
    for (i = 1; i < p->n; i++) {
        p->buf[i - 1u] = p->buf[i];
    }
    if (p->n > 0) {
        p->n--;
    }
    p->t_start_ms = now_ms;            /* the new first byte starts a new assembly period */
}

/* Payload length the CIS accepts for a type that arrives from the Pi; 0 = not accepted. */
static uint16_t inbound_len(uint8_t type)
{
    if (type == PI_TYPE_VISION_RESULT) {
        return PI_VISION_RESULT_LEN;
    }
    if (type == PI_TYPE_VISION_STATUS) {
        return PI_VISION_STATUS_LEN;
    }
    return 0u;
}

void pi_parser_expire(pi_parser_t *p, uint32_t now_ms)
{
    if (p->n > 0u && (uint32_t)(now_ms - p->t_start_ms) > PI_ASSEMBLY_TIMEOUT_MS) {
        p->assembly_timeouts++;
        drop_first(p, now_ms);         /* resync after the first byte of the late frame */
    }
}

bool pi_parser_step(pi_parser_t *p, pi_frame_t *out)
{
    for (;;) {
        uint16_t len;
        uint16_t total;
        uint16_t crc_calc;
        uint16_t crc_rx;
        const uint8_t *pl;

        if (p->n >= 1u && p->buf[0] != PI_SOF0) {
            drop_first(p, p->t_start_ms);
            p->skipped_bytes++;
            continue;
        }
        if (p->n >= 2u && p->buf[1] != PI_SOF1) {
            drop_first(p, p->t_start_ms);
            p->skipped_bytes++;
            continue;
        }
        if (p->n >= 3u && p->buf[2] != PI_VERSION) {
            p->bad_format++;
            drop_first(p, p->t_start_ms);
            continue;
        }
        if (p->n >= 4u && inbound_len(p->buf[3]) == 0u) {
            p->bad_format++;
            drop_first(p, p->t_start_ms);
            continue;
        }
        if (p->n < 6u) {
            return false;                           /* header not complete yet */
        }

        len = rd16(&p->buf[4]);
        if (len != inbound_len(p->buf[3])) {
            p->bad_format++;
            drop_first(p, p->t_start_ms);
            continue;
        }

        total = (uint16_t)(PI_FRAME_OVERHEAD + len);
        if (p->n < total) {
            return false;                           /* frame not complete yet */
        }

        crc_calc = crc16_ccitt_false(&p->buf[2], (size_t)(6u + len));
        crc_rx = rd16(&p->buf[8u + len]);
        if (crc_calc != crc_rx) {
            p->bad_crc++;
            drop_first(p, p->t_start_ms);           /* a real frame may start inside it */
            continue;
        }

        out->type = p->buf[3];
        out->link_seq = rd16(&p->buf[6]);
        pl = &p->buf[8];
        if (out->type == PI_TYPE_VISION_RESULT) {
            out->result.pi_boot_id         = rd32(&pl[0]);
            out->result.vision_sequence    = rd32(&pl[4]);
            out->result.vision_age_ms      = rd16(&pl[8]);
            out->result.presence           = pl[10];
            out->result.presence_validity  = pl[11];
            out->result.presence_reason    = pl[12];
            out->result.count              = pl[13];
            out->result.count_validity     = pl[14];
            out->result.count_reason       = pl[15];
            out->result.vision_status      = pl[16];
            out->result.fault_code         = rd16(&pl[17]);
        } else {
            out->status.pi_boot_id         = rd32(&pl[0]);
            out->status.status_sequence    = rd32(&pl[4]);
            out->status.vision_status      = pl[8];
            out->status.active_fault       = rd16(&pl[9]);
            out->status.last_fault         = rd16(&pl[11]);
            out->status.status_age_ms      = rd16(&pl[13]);
            out->status.affected_function  = pl[15];
        }
        {
            uint16_t i;
            for (i = total; i < p->n; i++) {
                p->buf[i - total] = p->buf[i];
            }
            p->n = (uint8_t)(p->n - total);
        }
        p->frames_ok++;
        return true;
    }
}

static size_t finish_frame(uint8_t *out, uint8_t type, uint16_t link_seq, uint16_t len)
{
    uint16_t crc;

    out[0] = PI_SOF0;
    out[1] = PI_SOF1;
    out[2] = (uint8_t)PI_VERSION;
    out[3] = type;
    wr16(&out[4], len);
    wr16(&out[6], link_seq);
    crc = crc16_ccitt_false(&out[2], (size_t)(6u + len));
    wr16(&out[8u + len], crc);
    return (size_t)(PI_FRAME_OVERHEAD + len);
}

size_t pi_build_vision_result(uint8_t *out, uint16_t link_seq, const pi_vision_result_t *v)
{
    uint8_t *pl = &out[8];
    wr32(&pl[0], v->pi_boot_id);
    wr32(&pl[4], v->vision_sequence);
    wr16(&pl[8], v->vision_age_ms);
    pl[10] = v->presence;
    pl[11] = v->presence_validity;
    pl[12] = v->presence_reason;
    pl[13] = v->count;
    pl[14] = v->count_validity;
    pl[15] = v->count_reason;
    pl[16] = v->vision_status;
    wr16(&pl[17], v->fault_code);
    return finish_frame(out, PI_TYPE_VISION_RESULT, link_seq, PI_VISION_RESULT_LEN);
}

size_t pi_build_vision_status(uint8_t *out, uint16_t link_seq, const pi_vision_status_t *s)
{
    uint8_t *pl = &out[8];
    wr32(&pl[0], s->pi_boot_id);
    wr32(&pl[4], s->status_sequence);
    pl[8] = s->vision_status;
    wr16(&pl[9], s->active_fault);
    wr16(&pl[11], s->last_fault);
    wr16(&pl[13], s->status_age_ms);
    pl[15] = s->affected_function;
    return finish_frame(out, PI_TYPE_VISION_STATUS, link_seq, PI_VISION_STATUS_LEN);
}

size_t pi_build_permission(uint8_t *out, uint16_t link_seq, const pi_permission_t *p)
{
    uint8_t *pl = &out[8];
    wr32(&pl[0], p->cis_boot_id);
    wr32(&pl[4], p->update_sequence);
    pl[8] = p->permission;
    pl[9] = p->value_quality;
    pl[10] = p->quality_reason;
    wr16(&pl[11], p->use_limit_ms);
    wr16(&pl[13], p->source_age_ms);
    return finish_frame(out, PI_TYPE_PERMISSION, link_seq, PI_PERMISSION_LEN);
}
