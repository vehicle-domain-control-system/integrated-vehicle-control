/*
 * Occupant presence and count from the Raspberry Pi camera judgement. No hardware access.
 *
 * Rules (CIS-SYS-FUN-003..007, 025):
 *  - presence and count come from the same judgement: they share one SEQUENCE and
 *    cannot contradict each other (presence is derived from the count)
 *  - an unusable picture or an unknown count is never turned into "nobody there"
 *  - a repeated frame of the same judgement does not become a new observation and
 *    does not extend the validity; the value ages from the time of the judgement
 *  - a Pi restart (different SESSION) discards the old judgement
 *  - no frame for VISION_RX_TIMEOUT_MS -> the values become INVALID (NO_DATA)
 *  - after a vision fault, values are valid again only after a new good judgement
 */
#ifndef VISION_LOGIC_H
#define VISION_LOGIC_H

#include "cis_value.h"
#include "pi_protocol.h"

#define VISION_MAX_COUNT            5u       /* CIS-SYS-FUN-004 */
#define VISION_UART_MARGIN_MS       50u      /* allowance for the UART transfer added to the age */
#define VISION_STALE_MS             1000u    /* a judgement older than this is not used */
#define VISION_RX_TIMEOUT_MS        300u     /* no frame for this long = link lost */

#define PRESENCE_ABSENT             0
#define PRESENCE_PRESENT            1
#define PRESENCE_UNKNOWN            255

typedef struct {
    cis_value_t presence;       /* PRESENCE_ABSENT / PRESENT / UNKNOWN */
    cis_value_t count;          /* 0..5, 255 = unknown */
    uint8_t     session;
    uint16_t    last_seq;
    bool        have_context;
    bool        link_up;
    uint32_t    last_rx_ms;
} vision_state_t;

void vision_init(vision_state_t *s);

/* A valid VISION frame arrived. */
void vision_on_frame(vision_state_t *s, const pi_vision_t *f, uint32_t now_ms);

/* Call regularly: detects a lost link and ages the values. */
void vision_check(vision_state_t *s, uint32_t now_ms);

#endif /* VISION_LOGIC_H */
