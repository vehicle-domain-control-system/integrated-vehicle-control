/* L03/M04 Message Service: latest-value publish slots, Network-owned transmit
 * identity and periodic repetition. Contract and basis: net_upper_publish.h. */
#include "network/net_upper_publish.h"
#include "net_upper_private.h"
#include <string.h>

#define INC(x) net_upper_observe_increment(&(x))

/* Repetition age policy. A repetition sends the original content with the
 * original stamp again, so the service adds the whole time since that stamp
 * and the wire AGE saturates at 65535 (net_upper_can_add_age /
 * net_upper_uart_add_age). A new TX_SEQUENCE never renews the original time
 * and the current AGE keeps increasing; whether a value is still usable is
 * decided by the receiver against its own original-AGE limit
 * (interface/04_COMMON/10_DATA_AND_ID_RULES.md, sequence table and receive
 * timeout). A bounded maximum here would stop the schedule about 66 s after
 * the last submission (65535 + margin), and the receiver would then report a
 * lost link instead of a saturated AGE. So repetitions carry no Network-side
 * expiry. Limits the service enforces on its own (the 500 ms original Power
 * lifetime of DOMAIN_PERMISSION and P_PERMISSION) still apply to every
 * repetition. */
static const NetAgePolicy repeat_age = { true, UINT64_MAX, false };

static bool ready(const NetUpperPublisher *p)
{
    return p != NULL && p->initialized && p->service != NULL && p->service->initialized;
}
static size_t uart_index(NetWireLink link) { return (size_t)link - 1u; }
static bool link_down(const NetUpper *s, NetWireLink link)
{
    return s->link_failed[link] || s->quiescing[link];
}
static size_t slot_index(const NetUpperPublisher *p, NetWireLink link, uint16_t id)
{
    size_t i;
    for (i = 0u; i < p->count; ++i)
        if (p->slot[i].entry.link == link && p->slot[i].entry.id == id) return i;
    return NET_UPPER_PUBLISH_CAPACITY;
}
/* The service wraps a UART LINK_SEQUENCE modulo 65536 (20_UART_M.md shared
 * framing), so this guard only catches a counter outside u16. */
static bool uart_exhausted(const NetUpper *s, NetWireLink link)
{
    return s->next_uart_sequence[uart_index(link)] > UINT16_MAX;
}

NetStatus net_upper_publish_init(NetUpperPublisher *p, NetUpper *s, const NetUpperPublishConfig *c)
{
    size_t i, j;
    bool has_can = false;
    if (p == NULL || s == NULL || c == NULL || !s->initialized ||
        (c->entry_count != 0u && c->entries == NULL) || c->entry_count > NET_UPPER_PUBLISH_CAPACITY ||
        net_wire_ranges_overlap(p, sizeof(*p), s, sizeof(*s)) ||
        net_wire_ranges_overlap(p, sizeof(*p), c, sizeof(*c)) ||
        (c->entry_count != 0u &&
         net_wire_ranges_overlap(p, sizeof(*p), c->entries, c->entry_count * sizeof(*c->entries))))
        return NET_INVALID_ARGUMENT;
    if (s->in_call) return NET_BUSY;
    for (i = 0u; i < c->entry_count; ++i) {
        const NetUpperPublishEntry *e = &c->entries[i];
        const NetWireRegistryEntry *r;
        if (e->link < NET_WIRE_CAN || e->link > NET_WIRE_UART_C || !e->age.bound) return NET_INVALID_ARGUMENT;
        for (j = 0u; j < i; ++j)
            if (c->entries[j].link == e->link && c->entries[j].id == e->id) return NET_INVALID_ARGUMENT;
        r = net_wire_registry_find(e->link, e->id);
        if (r == NULL) return NET_UNSUPPORTED;
        if (!net_upper_profile_link(e->link)) return NET_NOT_CONFIGURED;
        if (r->sender != s->config.node) return NET_REJECTED;
        if (e->link == NET_WIRE_CAN) has_can = true;
        else if (!s->config.uart_sequence_bound[uart_index(e->link)]) return NET_NOT_CONFIGURED;
    }
    if (has_can && c->can_boot_id == 0u) return NET_INVALID_ARGUMENT; /* BOOT_ID 0 is reserved */
    if (s->config.retain_failure_notices) return NET_HOLD;
    memset(p, 0, sizeof(*p));
    p->service = s;
    p->can_boot_id = c->can_boot_id;
    p->count = c->entry_count;
    for (i = 0u; i < c->entry_count; ++i) {
        p->slot[i].entry = c->entries[i];
        p->slot[i].next_sequence = 1u; /* first TX_SEQUENCE of each message */
    }
    p->initialized = true;
    return NET_OK;
}

/* ------------------------------------------------------------ submission - */
static NetStatus clock_now(NetUpper *s, uint64_t *now, bool *in_step)
{
    if (s->in_call) {
        /* Called back from inside this service's step (an RX callback). The
         * step has read the clock and that reading is not later than this
         * call: the wait counted from it errs towards an older AGE, never a
         * younger one. */
        if (!s->clock_seen) return NET_HOLD;
        *now = s->last_now_ms;
        *in_step = true;
        return NET_OK;
    }
    *in_step = false;
    return net_upper_now(s, now);
}

static NetStatus stamp(NetUpperPublisher *p, const NetUpperPublishSlot *slot, const NetSourceStamp *source,
    uint16_t content_age, NetSourceStamp *out, bool *in_step)
{
    NetSourceStamp value;
    uint64_t now, age;
    NetStatus st = clock_now(p->service, &now, in_step);
    if (st != NET_OK) return st;
    if (source != NULL) {
        /* Relay: keep the original receipt; its AGE basis is the content's. */
        if (source->observation_id == 0u || source->original_age_ms != content_age) return NET_HOLD;
        value = *source;
    } else {
        if (p->next_observation == UINT64_MAX) return NET_HOLD;
        value.observation_id = p->next_observation + 1u;
        value.observed_at_ms = now;
        value.original_age_ms = content_age;
    }
    st = net_source_age(&value, now, &age);
    if (st != NET_OK) return st;
    if (age > slot->entry.age.maximum_age_ms ||
        (age == slot->entry.age.maximum_age_ms && slot->entry.age.expires_at_equality)) return NET_EXPIRED;
    *out = value;
    return NET_OK;
}

static void commit(NetUpperPublisher *p, NetUpperPublishSlot *slot, bool local,
    const NetSourceStamp *value, bool in_step, NetUpperPublishNote *note)
{
    bool replaced = slot->fresh;
    if (local) p->next_observation = value->observation_id;
    slot->source = *value;
    slot->has_source = true; /* also the repetition source from now on */
    slot->fresh = true;
    slot->fresh_counted = false;
    INC(slot->counters.submitted);
    if (replaced) INC(slot->counters.replaced);
    if (note != NULL) { note->replaced = replaced; note->in_step = in_step; }
}

NetStatus net_upper_publish_can(NetUpperPublisher *p, const NetUpperCanMessage *m,
    const NetSourceStamp *source, NetUpperPublishNote *note)
{
    NetUpperPublishSlot *slot;
    NetUpperCanMessage local;
    NetWireCanEnvelope probe;
    NetSourceStamp value;
    NetStatus st;
    size_t i;
    bool in_step = false;
    if (!ready(p) || m == NULL || net_wire_ranges_overlap(p, sizeof(*p), m, sizeof(*m)) ||
        (source != NULL && net_wire_ranges_overlap(p, sizeof(*p), source, sizeof(*source))) ||
        (note != NULL && net_wire_ranges_overlap(p, sizeof(*p), note, sizeof(*note))))
        return NET_INVALID_ARGUMENT;
    i = slot_index(p, NET_WIRE_CAN, m->id);
    if (i >= p->count) return NET_UNSUPPORTED;
    slot = &p->slot[i];
    if (link_down(p->service, NET_WIRE_CAN)) return NET_HOLD;
    if (slot->sequence_held) { INC(slot->counters.sequence_hold); return NET_HOLD; }
    local = *m;
    /* Content check with the identity the next frame would carry. */
    if (net_upper_can_set_identity(&local, p->can_boot_id, slot->next_sequence) != NET_WIRE_OK) return NET_UNSUPPORTED;
    st = net_upper_wire_status(net_upper_can_encode(p->service->config.profile, &local, &probe));
    if (st != NET_OK) return st;
    st = stamp(p, slot, source, net_upper_can_max_age(&local), &value, &in_step);
    if (st != NET_OK) return st;
    (void)net_upper_can_set_identity(&local, 0u, 0u); /* assigned per frame */
    slot->content.can = local;
    commit(p, slot, source == NULL, &value, in_step, note);
    return NET_OK;
}

NetStatus net_upper_publish_uart(NetUpperPublisher *p, const NetUpperUartMessage *m,
    const NetSourceStamp *source, NetUpperPublishNote *note)
{
    NetUpperPublishSlot *slot;
    NetUpperUartMessage local;
    NetWireUartFrame probe;
    NetSourceStamp value;
    NetStatus st;
    size_t i;
    bool in_step = false;
    if (!ready(p) || m == NULL || net_wire_ranges_overlap(p, sizeof(*p), m, sizeof(*m)) ||
        (source != NULL && net_wire_ranges_overlap(p, sizeof(*p), source, sizeof(*source))) ||
        (note != NULL && net_wire_ranges_overlap(p, sizeof(*p), note, sizeof(*note))) ||
        (m->link != NET_WIRE_UART_M && m->link != NET_WIRE_UART_C))
        return NET_INVALID_ARGUMENT;
    i = slot_index(p, m->link, m->type);
    if (i >= p->count) return NET_UNSUPPORTED;
    slot = &p->slot[i];
    if (link_down(p->service, m->link)) return NET_HOLD;
    if (uart_exhausted(p->service, m->link)) { INC(slot->counters.sequence_hold); return NET_HOLD; }
    local = *m;
    local.sequence = (uint16_t)p->service->next_uart_sequence[uart_index(m->link)];
    st = net_upper_wire_status(net_upper_uart_encode(p->service->config.profile, &local, &probe));
    if (st != NET_OK) return st;
    st = stamp(p, slot, source, net_upper_uart_max_age(&local), &value, &in_step);
    if (st != NET_OK) return st;
    local.sequence = 0u; /* assigned per frame by the service */
    slot->content.uart = local;
    commit(p, slot, source == NULL, &value, in_step, note);
    return NET_OK;
}

/* --------------------------------------------------------------- service - */
static void restart(NetUpperPublishSlot *slot, uint64_t now)
{
    uint64_t period = slot->entry.period_ms;
    slot->next_due_ms = (period != 0u && now <= UINT64_MAX - period) ? now + period : UINT64_MAX;
}
static void advance(NetUpperPublishSlot *slot, uint64_t now)
{
    uint64_t period = slot->entry.period_ms;
    if (period == 0u) return;
    slot->next_due_ms = slot->next_due_ms <= UINT64_MAX - period ? slot->next_due_ms + period : UINT64_MAX;
    if (slot->next_due_ms <= now) restart(slot, now); /* late by a whole period: re-anchor, never burst */
}
static bool transient(NetStatus st) { return st == NET_NO_CAPACITY || st == NET_BUSY; }

/* Settle the message's unsettled frame: retire it once its facts are final. */
static void settle(NetUpperPublisher *p, NetUpperPublishSlot *slot)
{
    NetUpperTxSnapshot t;
    NetStatus st;
    bool cancelled = false;
    if (!slot->in_flight) return;
    st = net_upper_tx_snapshot(p->service, slot->key, &t);
    if (st == NET_STALE) { slot->in_flight = false; return; } /* fate unknown: its number stays used */
    if (st != NET_OK) return;
    if (t.queued) {
        /* Offered but not dispatched: a newer submission replaces a frame no
         * device has taken (architecture §6 A). */
        if (!slot->fresh || net_upper_cancel(p->service, slot->key) != NET_OK ||
            net_upper_tx_snapshot(p->service, slot->key, &t) != NET_OK) return;
        cancelled = true;
    }
    st = net_upper_retire(p->service, slot->key);
    if (st != NET_OK && st != NET_STALE) return; /* a device still owns it, or facts are pending */
    slot->in_flight = false;
    if (t.submitted) {
        /* A device took the frame: its number is used whatever the outcome. */
        if (t.outcome == NET_TX_FAILED || (t.admission_known && !t.accepted)) INC(slot->counters.tx_failed);
        return;
    }
    /* No device took the frame. Its TX_SEQUENCE is given back only when no
     * device can have seen it: cancelled before dispatch, or no room (no frame
     * storage, every buffer still owned by the driver; a non-OK Port submit
     * promises no borrow). Any other refusal keeps the number used: a number
     * offered to a device is not reused even when the device clearly refused,
     * a gap is allowed, a reuse is not (CIS_REAR_TX_DISPATCH.md, NO_CAPACITY
     * keeps the pending observation, DRIVER_REJECTED keeps the number used).
     * The UART LINK_SEQUENCE is the service's per-direction counter. */
    if (slot->entry.link == NET_WIRE_CAN && (cancelled || transient(t.local_status))) {
        slot->next_sequence = slot->in_flight_sequence;
        slot->sequence_held = false;
    }
    if (cancelled) { INC(slot->counters.replaced); return; }
    if (slot->in_flight_fresh && !slot->fresh && transient(t.local_status)) {
        slot->fresh = true; /* the device had no room: keep the submission waiting */
        INC(slot->counters.queue_full);
        return;
    }
    INC(slot->counters.not_sent);
    if (slot->in_flight_fresh && !slot->fresh && t.local_status == NET_EXPIRED)
        slot->has_source = false; /* never repeat what may not be sent */
}

static void offer(NetUpperPublisher *p, NetUpperPublishSlot *slot, uint64_t now, bool fresh)
{
    const NetAgePolicy *policy = fresh ? &slot->entry.age : &repeat_age;
    NetTxKey key;
    NetStatus st;
    uint32_t sequence = 0u;
    bool exhausted = slot->entry.link == NET_WIRE_CAN ? slot->sequence_held : uart_exhausted(p->service, slot->entry.link);
    if (exhausted) {
        /* CAN TX_SEQUENCE wrap is HOLD: never restart at 0/1 and never send a wrong number. */
        INC(slot->counters.sequence_hold);
        if (fresh) slot->fresh = false;
        advance(slot, now);
        return;
    }
    if (slot->entry.link == NET_WIRE_CAN) {
        NetUpperCanMessage m = slot->content.can;
        sequence = slot->next_sequence;
        (void)net_upper_can_set_identity(&m, p->can_boot_id, sequence);
        st = net_upper_submit_can(p->service, &m, &slot->source, policy, &key);
    } else {
        st = net_upper_submit_uart_auto(p->service, &slot->content.uart, &slot->source, policy, &key);
    }
    if (st == NET_OK) {
        slot->in_flight = true;
        slot->in_flight_fresh = fresh;
        slot->key = key;
        if (slot->entry.link == NET_WIRE_CAN) {
            slot->in_flight_sequence = sequence;
            if (sequence == UINT32_MAX) slot->sequence_held = true;
            else slot->next_sequence = sequence + 1u;
        }
        if (fresh) {
            uint64_t waited = now - slot->source.observed_at_ms; /* age check passed: now >= stamp */
            slot->fresh = false;
            if (!slot->fresh_counted) INC(slot->counters.sent); /* a retry after "no room" is the same submission */
            slot->fresh_counted = true;
            if (waited > slot->counters.pending_age_added_ms_max)
                slot->counters.pending_age_added_ms_max = waited > UINT32_MAX ? UINT32_MAX : (uint32_t)waited;
            restart(slot, now); /* a submission resets the repetition phase */
        } else {
            INC(slot->counters.periodic_sent);
            advance(slot, now);
        }
        return;
    }
    if (transient(st)) {
        /* Service queue full: a submission waits for the next pass, a missed
         * repetition is not caught up. */
        INC(slot->counters.queue_full);
        if (!fresh) advance(slot, now);
        return;
    }
    INC(slot->counters.not_sent);
    if (fresh) {
        slot->fresh = false;
        if (st == NET_EXPIRED) slot->has_source = false; /* never repeat what may not be sent */
        restart(slot, now);
    } else {
        advance(slot, now); /* a failed repetition never stops the schedule */
    }
}

NetStatus net_upper_publish_service(NetUpperPublisher *p)
{
    size_t i;
    uint64_t now;
    NetStatus st;
    if (!ready(p)) return NET_INVALID_ARGUMENT;
    if (p->service->in_call) return NET_BUSY;
    st = net_upper_now(p->service, &now);
    if (st != NET_OK) return st;
    for (i = 0u; i < p->count; ++i) {
        NetUpperPublishSlot *slot = &p->slot[i];
        settle(p, slot);
        if (slot->fresh) {
            /* Sent at once unless this message's previous frame is unsettled;
             * then it waits and its AGE keeps growing from the original stamp. */
            if (!slot->in_flight) offer(p, slot, now, true);
        } else if (slot->entry.period_ms != 0u && slot->has_source && now >= slot->next_due_ms) {
            if (slot->in_flight) {
                INC(slot->counters.periodic_skipped_pending);
                advance(slot, now);
            } else {
                offer(p, slot, now, false);
            }
        }
    }
    return NET_OK;
}

NetStatus net_upper_publish_view(const NetUpperPublisher *p, NetWireLink link, uint16_t id,
    NetUpperPublishView *out)
{
    const NetUpperPublishSlot *slot;
    NetUpperPublishView v;
    size_t i;
    if (!ready(p) || out == NULL || net_wire_ranges_overlap(p, sizeof(*p), out, sizeof(*out)))
        return NET_INVALID_ARGUMENT;
    i = slot_index(p, link, id);
    if (i >= p->count) return NET_UNSUPPORTED;
    slot = &p->slot[i];
    memset(&v, 0, sizeof(v));
    v.has_source = slot->has_source;
    v.fresh = slot->fresh;
    v.in_flight = slot->in_flight;
    v.period_ms = slot->entry.period_ms;
    v.counters = slot->counters;
    if (link == NET_WIRE_CAN) {
        v.boot_id = p->can_boot_id;
        v.next_sequence = slot->next_sequence;
        v.sequence_held = slot->sequence_held;
    } else {
        v.next_sequence = p->service->next_uart_sequence[uart_index(link)];
        v.sequence_held = uart_exhausted(p->service, link);
    }
    *out = v;
    return NET_OK;
}
