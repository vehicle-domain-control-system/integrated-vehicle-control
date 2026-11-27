/*
 * Occupant presence and count from the Raspberry Pi camera (P_VISION_RESULT / P_VISION_STATUS).
 * No hardware access.
 *
 * The Pi is the producer of the original judgement: Pi boot number, VISION_SEQUENCE, the
 * values and the quality of each value are kept as they came (value.sequence of both
 * values is the Pi's VISION_SEQUENCE). The CIS only restricts a value further when it can
 * confirm a reason itself:
 *  - the judgement is older than VISION_STALE_MS            -> INVALID / STALE
 *  - no P_VISION_RESULT for VISION_RX_TIMEOUT_MS            -> INVALID / COMMUNICATION_FAULT
 *  - the Pi restarted (new boot number)                     -> old judgement INVALID / NO_DATA
 *  - the two values contradict each other (e.g. none + 2)   -> both INVALID / DATA_INVALID
 *
 * Rules (CIS-SYS-FUN-003..007, 025):
 *  - presence and count have independent quality: a bad count does not hide a valid presence
 *  - an unknown or unusable picture is never turned into "nobody there"
 *  - a repeated frame of the same judgement does not become a new observation and does not
 *    extend the validity: the value ages from the time of the judgement
 *  - P_VISION_STATUS READY / RECOVERING never renews a judgement
 *  - after a vision fault, values are valid again only after new good judgements
 */
#ifndef VISION_LOGIC_H
#define VISION_LOGIC_H

#include "cis_value.h"
#include "pi_protocol.h"

#define VISION_MAX_COUNT            5u       /* CIS-SYS-FUN-004 */
#define VISION_UART_MARGIN_MS       50u      /* UART hop allowance added once to the age (proposal) */
#define VISION_STALE_MS             1000u    /* a judgement older than this is not used (proposal) */
#define VISION_RX_TIMEOUT_MS        300u     /* no P_VISION_RESULT for this long = link lost (proposal) */
#define VISION_STATUS_TIMEOUT_MS    3000u    /* no P_VISION_STATUS for this long (proposal) */

/* Function states (same numbers as cis_func_status_t) as the Pi reports them */
#define PI_STATE_NOT_READY          0u
#define PI_STATE_READY              1u
#define PI_STATE_ACTIVE             2u
#define PI_STATE_FAULT              3u
#define PI_STATE_RECOVERING         4u

typedef struct {
    cis_value_t presence;       /* 0 none, 1 present, 255 unknown */
    cis_value_t count;          /* 0..5, 255 unknown */
    uint32_t    pi_boot_id;
    uint32_t    vision_sequence;        /* last accepted new judgement */
    bool        have_context;           /* a Pi boot number is known */
    bool        have_sequence;
    bool        link_up;                /* P_VISION_RESULT arrives */
    uint32_t    last_rx_ms;
    uint8_t     pi_vision_status;       /* last state reported by the Pi */
    uint16_t    pi_active_fault;
    uint16_t    pi_last_fault;
    uint32_t    status_sequence;
    bool        have_status;
    bool        status_link_up;         /* P_VISION_STATUS arrives */
    uint32_t    last_status_rx_ms;
    uint32_t    rejected_frames;        /* e.g. Pi boot number 0 */
} vision_state_t;

void vision_init(vision_state_t *s);

/* A valid P_VISION_RESULT arrived. Returns true if it was a new judgement. */
bool vision_on_result(vision_state_t *s, const pi_vision_result_t *f, uint32_t now_ms);

/* A valid P_VISION_STATUS arrived. */
void vision_on_status(vision_state_t *s, const pi_vision_status_t *f, uint32_t now_ms);

/* Call regularly: detects a lost link and ages the values. */
void vision_check(vision_state_t *s, uint32_t now_ms);

/* Function state of the vision function as the CIS reports it (for CIS_STATUS). */
cis_func_status_t vision_function_status(const vision_state_t *s);

#endif /* VISION_LOGIC_H */
