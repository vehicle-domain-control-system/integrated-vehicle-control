#include "vision_logic.h"

void vision_init(vision_state_t *s)
{
    cis_value_init(&s->presence);
    cis_value_init(&s->count);
    s->session = 0;
    s->last_seq = 0;
    s->have_context = false;
    s->link_up = false;
    s->last_rx_ms = 0;
}

static void both_invalidate(vision_state_t *s, cis_reason_t reason)
{
    cis_value_invalidate(&s->presence, reason);
    cis_value_invalidate(&s->count, reason);
}

static void both_fault(vision_state_t *s, cis_reason_t reason)
{
    cis_value_fault(&s->presence, reason);
    cis_value_fault(&s->count, reason);
}

/* The Pi reported a usable judgement (count 0..5, no fault). */
static void accept_judgement(vision_state_t *s, const pi_vision_t *f, uint32_t now_ms)
{
    uint32_t age = (uint32_t)f->age_ms + VISION_UART_MARGIN_MS;
    int32_t presence = (f->count > 0u) ? PRESENCE_PRESENT : PRESENCE_ABSENT;

    /* After a fault: this judgement only starts the recovery, the next one completes it. */
    if (s->count.status == CIS_FUNC_FAULT || s->presence.status == CIS_FUNC_FAULT) {
        cis_value_begin_recovery(&s->presence);
        cis_value_begin_recovery(&s->count);
        return;
    }

    cis_value_new_sample(&s->presence, presence, now_ms);
    cis_value_new_sample(&s->count, (int32_t)f->count, now_ms);
    /* The observation is as old as the judgement was when it was sent. */
    s->presence.sample_ms = now_ms - age;
    s->count.sample_ms = now_ms - age;
}

void vision_on_frame(vision_state_t *s, const pi_vision_t *f, uint32_t now_ms)
{
    uint16_t diff;

    s->last_rx_ms = now_ms;
    s->link_up = true;

    if (!s->have_context || f->session != s->session) {
        /* First frame, or the Pi restarted: forget the old judgement. */
        s->session = f->session;
        s->have_context = true;
        s->last_seq = (uint16_t)(f->seq - 1u);          /* so that this frame counts as new */
        if (s->presence.has_sample) {
            both_invalidate(s, CIS_REASON_NO_DATA);
        }
    }

    diff = (uint16_t)(f->seq - s->last_seq);
    if (diff == 0u || diff >= 0x8000u) {
        return;                                         /* repeat or older: no new observation */
    }
    s->last_seq = f->seq;

    if (f->fault != 0u) {
        /* The vision function itself has a fault. */
        both_fault(s, CIS_REASON_VISION_FAULT);
    } else if (f->quality != 0u) {
        /* The picture was not good enough: unknown, never "nobody". */
        both_invalidate(s, (cis_reason_t)f->quality);
    } else if (f->count == PI_COUNT_UNKNOWN) {
        both_invalidate(s, CIS_REASON_NO_DATA);
    } else if (f->count > VISION_MAX_COUNT) {
        both_invalidate(s, CIS_REASON_OUT_OF_RANGE);
    } else {
        accept_judgement(s, f, now_ms);
    }
}

void vision_check(vision_state_t *s, uint32_t now_ms)
{
    if (s->link_up && (uint32_t)(now_ms - s->last_rx_ms) > VISION_RX_TIMEOUT_MS) {
        s->link_up = false;
        both_invalidate(s, CIS_REASON_NO_DATA);          /* last judgement is not "current" any more */
    }
    cis_value_check_stale(&s->presence, now_ms, VISION_STALE_MS);
    cis_value_check_stale(&s->count, now_ms, VISION_STALE_MS);
}
