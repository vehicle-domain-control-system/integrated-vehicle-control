#include "power_gate.h"

void power_gate_init(power_gate_t *g)
{
    g->have_rx = false;
    g->permission = POWER_PERM_UNKNOWN;
    g->quality_ok = false;
    g->rx_ms = 0;
    g->origin_age_ms = 0;
    g->was_open = false;
    g->wait_new_sample = true;
    g->baseline_sequence = 0;
}

void power_gate_on_permission(power_gate_t *g, uint8_t permission, bool quality_ok,
                              uint32_t source_age_ms, uint32_t now_ms)
{
    g->have_rx = true;
    g->permission = permission;
    g->quality_ok = quality_ok;
    g->origin_age_ms = source_age_ms;
    g->rx_ms = now_ms;
}

power_gate_state_t power_gate_state(const power_gate_t *g, uint32_t now_ms)
{
    uint32_t gap;
    uint32_t origin_age;

    if (!g->have_rx) {
        return POWER_GATE_UNKNOWN;
    }
    gap = now_ms - g->rx_ms;
    if (gap > POWER_RX_TIMEOUT_MS) {
        return POWER_GATE_LINK_LOST;
    }
    origin_age = g->origin_age_ms + gap;           /* the age keeps counting from the original */
    if (origin_age >= POWER_ORIGIN_LIMIT_MS) {
        return POWER_GATE_EXPIRED;
    }
    if (!g->quality_ok || (g->permission != POWER_PERM_DENY && g->permission != POWER_PERM_ALLOW)) {
        return POWER_GATE_UNKNOWN;
    }
    return (g->permission == POWER_PERM_ALLOW) ? POWER_GATE_OPEN : POWER_GATE_DENIED;
}

static void closed_output(rear_output_t *out, power_gate_state_t st, const rear_state_t *rear)
{
    out->status = CIS_PROX_INACTIVE;
    out->valid = false;
    out->distance_mm = REAR_NO_VALUE;
    out->sequence = rear->value.sequence;
    switch (st) {
    case POWER_GATE_DENIED:
        out->valid = true;                          /* a confirmed "not allowed" */
        out->reason = CIS_REASON_NONE;
        break;
    case POWER_GATE_EXPIRED:
        out->reason = CIS_REASON_STALE;
        break;
    case POWER_GATE_LINK_LOST:
        out->reason = CIS_REASON_COMMUNICATION_FAULT;
        break;
    default:
        out->reason = CIS_REASON_NO_DATA;
        break;
    }
}

void power_gate_apply(power_gate_t *g, const rear_state_t *rear, uint32_t now_ms,
                      rear_output_t *out)
{
    power_gate_state_t st = power_gate_state(g, now_ms);

    if (st != POWER_GATE_OPEN) {
        g->was_open = false;
        closed_output(out, st, rear);
        return;
    }
    if (!g->was_open) {
        /* Just opened: only a measurement made from now on counts. */
        g->was_open = true;
        g->wait_new_sample = true;
        g->baseline_sequence = rear->value.sequence;
    }
    if (g->wait_new_sample && rear->value.sequence != g->baseline_sequence) {
        g->wait_new_sample = false;
    }
    if (g->wait_new_sample) {
        out->status = CIS_PROX_UNAVAILABLE;
        out->valid = false;
        out->reason = CIS_REASON_NO_DATA;
        out->distance_mm = REAR_NO_VALUE;
        out->sequence = rear->value.sequence;
        return;
    }
    out->status = rear->status;
    out->valid = rear->value.valid;
    out->reason = rear->value.reason;
    out->distance_mm = (uint32_t)rear->value.value;
    out->sequence = rear->value.sequence;
}
