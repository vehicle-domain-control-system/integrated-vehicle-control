#include "network/net_upper_query.h"
#include <string.h>

/* F04/F05: selected external snapshots -> stable product Service submissions
 * -> observed local final-TX ledger -> ordered End. No vehicle state cache,
 * Owner algorithm, Session issuance, remote ACK or deadline is invented. */
static const uint8_t schemas[NET_UPPER_QUERY_GETTERS] = {
    0x30u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x3au,
    0x38u, 0x39u, 0x25u, 0x22u, 0x23u, 0x21u, 0x20u, 0x20u, 0x24u
};

uint8_t net_upper_query_schema(NetUpperQueryGetter getter)
{
    return (unsigned)getter < NET_UPPER_QUERY_GETTERS ? schemas[(unsigned)getter] : 0u;
}

static NetStatus delivery_decode(const NetUpperUartDelivery *delivery,
                                uint8_t type, NetUpperUartMessage *out)
{
    NetUpperUartMessage decoded;
    size_t i;
    if (delivery == NULL || out == NULL || delivery->port_epoch == 0u ||
        delivery->frame.length == 0u ||
        delivery->frame.length > NET_WIRE_UART_FRAME_MAX) return NET_INVALID_ARGUMENT;
    for (i = 1u; i < delivery->frame.length; ++i)
        if (delivery->byte_at_ms[i] < delivery->byte_at_ms[i - 1u]) return NET_HOLD;
    if (delivery->byte_at_ms[delivery->frame.length - 1u] !=
        delivery->frame.captured_at_ms) return NET_HOLD;
    if (net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M,
        NET_WIRE_UPLINK, &delivery->frame, &decoded) != NET_WIRE_OK)
        return NET_INVALID_ARGUMENT;
    if (decoded.type != type) return NET_INVALID_ARGUMENT;
    *out = decoded;
    return NET_OK;
}

NetStatus net_upper_query_init(NetUpperQuery *q, NetUpper *service,
    uint32_t boot, const NetUpperQueryOwner *owner)
{
    if (q == NULL || service == NULL || owner == NULL || boot == 0u)
        return NET_INVALID_ARGUMENT;
    memset(q, 0, sizeof(*q));
    q->service = service;
    q->owner = *owner;
    q->domain_boot = boot;
    q->initialized = true;
    return NET_OK;
}

NetStatus net_upper_query_on_context(NetUpperQuery *q,
                                   const NetUpperUartDelivery *delivery)
{
    NetUpperUartMessage decoded;
    NetStatus s;
    bool changed;
    if (q == NULL || !q->initialized) return NET_INVALID_ARGUMENT;
    if (q->in_call) return NET_BUSY;
    s = delivery_decode(delivery, 0x01u, &decoded);
    if (s != NET_OK) return s;
    if (q->context_known && delivery->port_epoch == q->port_epoch &&
        delivery->frame.captured_at_ms < q->context_received_at_ms) return NET_STALE;
    changed = !q->context_known || q->port_epoch != delivery->port_epoch ||
        q->context.device_context_id != decoded.data.m_context.device_context_id ||
        q->context.session_id != decoded.data.m_context.session_id;
    if (changed && q->context_generation == UINT64_MAX) return NET_HOLD;
    if (changed) {
        ++q->context_generation;
        if (q->run.active && !q->run.finished) q->run.cancelled = true;
    }
    q->context = decoded.data.m_context;
    q->port_epoch = delivery->port_epoch;
    q->context_received_at_ms = delivery->frame.captured_at_ms;
    q->context_known = true;
    return NET_OK;
}

static bool released_all(const NetUpperQueryRun *r)
{
    size_t i;
    for (i = 0u; i < r->response_count; ++i)
        if (!r->responses[i].retired) return false;
    return !r->end_submitted || r->end_retired;
}

static void cancel_pending(NetUpperQuery *q)
{
    size_t i;
    for (i = 0u; i < q->run.response_count; ++i)
        if (!q->run.responses[i].retired)
            (void)net_upper_cancel(q->service, q->run.responses[i].key);
    if (q->run.end_submitted && !q->run.end_retired)
        (void)net_upper_cancel(q->service, q->run.end_key);
}

NetStatus net_upper_query_begin(NetUpperQuery *q,
                              const NetUpperUartDelivery *delivery)
{
    NetUpperUartMessage decoded;
    NetStatus s;
    if (q == NULL || !q->initialized) return NET_INVALID_ARGUMENT;
    if (q->in_call) return NET_BUSY;
    if(delivery!=NULL && net_wire_ranges_overlap(q,sizeof(*q),delivery,sizeof(*delivery)))
        return NET_INVALID_ARGUMENT;
    s = delivery_decode(delivery, 0x11u, &decoded);
    if (s != NET_OK) return s;
    if (!q->context_known || q->port_epoch != delivery->port_epoch ||
        q->context.device_context_id != decoded.data.m_query.device_context_id ||
        q->context.session_id != decoded.data.m_query.session_id ||
        delivery->byte_at_ms[0] < q->context_received_at_ms) return NET_HOLD;
    if (q->run.active && (!q->run.finished || !released_all(&q->run))) return NET_BUSY;
    if (q->run.active && q->run.context_generation == q->context_generation &&
        q->run.domain_boot == q->domain_boot &&
        q->run.request.query_id == decoded.data.m_query.query_id) return NET_DUPLICATE;
    /* All failure guards and input alias checks precede this commit. */
    memset(&q->run, 0, sizeof(q->run));
    q->run.active = true;
    q->run.request = decoded.data.m_query;
    q->run.port_epoch = delivery->port_epoch;
    q->run.received_at_ms = delivery->frame.captured_at_ms;
    q->run.context_generation = q->context_generation;
    q->run.domain_boot = q->domain_boot;
    q->run.link_sequence = decoded.sequence;
    q->run.received_length = delivery->frame.length;
    memcpy(q->run.byte_at_ms, delivery->byte_at_ms,
           delivery->frame.length * sizeof(delivery->byte_at_ms[0]));
    return NET_OK;
}

NetStatus net_upper_query_invalidate(NetUpperQuery *q)
{
    if (q == NULL || !q->initialized) return NET_INVALID_ARGUMENT;
    if (q->in_call) return NET_BUSY;
    q->context_known = false;
    if (q->run.active && !q->run.finished) {
        q->run.cancelled = true;
        cancel_pending(q);
    }
    return NET_OK;
}

NetStatus net_upper_query_receive(void *context,
    const NetUpperUartDelivery *delivery, const NetUpperDelivery *metadata)
{
    NetUpperQuery *q = (NetUpperQuery *)context;
    if (q == NULL || delivery == NULL || metadata == NULL) return NET_INVALID_ARGUMENT;
    if (!metadata->capture.timestamp_known ||
        metadata->capture.port_epoch != delivery->port_epoch) return NET_HOLD;
    if (delivery->message.link != NET_WIRE_UART_M) return NET_UNSUPPORTED;
    if (delivery->message.type == 0x01u) return net_upper_query_on_context(q, delivery);
    if (delivery->message.type == 0x11u) return net_upper_query_begin(q, delivery);
    return NET_UNSUPPORTED;
}

static bool current(const NetUpperQuery *q)
{
    const NetUpperQueryRun *r = &q->run;
    return q->context_known && q->service->context_seen &&
        q->service->config.port_epoch[NET_WIRE_UART_M] == r->port_epoch &&
        r->context_generation == q->context_generation &&
        r->domain_boot == q->domain_boot && r->port_epoch == q->port_epoch &&
        r->request.device_context_id == q->context.device_context_id &&
        r->request.session_id == q->context.session_id;
}

static NetStatus validate_plan(const NetWireMQueryRaw *request,
                               const NetUpperQueryPlan *plan)
{
    size_t i, j, maximum = 0u;
    if (plan->count > NET_UPPER_QUERY_PLAN_CAPACITY) return NET_NO_CAPACITY;
    for (i = 0u; i < plan->count; ++i) {
        const NetUpperQuerySelection *s = &plan->selected[i];
        if ((unsigned)s->getter >= NET_UPPER_QUERY_GETTERS) return NET_INVALID_ARGUMENT;
        if (s->getter <= NET_QUERY_DIGITAL_STATE) {
            if (request->query_scope != 0u && request->query_scope != 3u)
                return NET_INVALID_ARGUMENT;
        } else if (s->getter == NET_QUERY_WARNING) {
            if ((request->query_scope != 2u && request->query_scope != 3u) ||
                s->selector < 1u || s->selector > 8u) return NET_INVALID_ARGUMENT;
        } else {
            if (request->query_scope != 1u && request->query_scope != 3u)
                return NET_INVALID_ARGUMENT;
            if (s->getter == NET_QUERY_REQUEST_RESULT &&
                (request->request_session == 0u || request->request_id == 0u)) return NET_HOLD;
            if (s->getter == NET_QUERY_RECENT_RESULTS &&
                (request->request_session != 0u || request->request_id != 0u)) return NET_HOLD;
        }
        if (s->getter != NET_QUERY_WARNING && s->selector != 0u) return NET_INVALID_ARGUMENT;
        if (s->capacity == 0u || s->capacity > NET_UPPER_QUERY_RECENT_CAPACITY ||
            (s->getter != NET_QUERY_RECENT_RESULTS && s->capacity != 1u)) return NET_INVALID_ARGUMENT;
        maximum += s->capacity;
        for (j = 0u; j < i; ++j)
            if (s->getter == plan->selected[j].getter &&
                s->selector == plan->selected[j].selector) return NET_DUPLICATE;
    }
    return maximum > NET_UPPER_QUERY_RESPONSE_CAPACITY ? NET_NO_CAPACITY : NET_OK;
}

static NetStatus observe_one(NetUpperQuery *q, NetUpperQueryResponse *r)
{
    NetUpperTxSnapshot snapshot;
    NetStatus s;
    if (r->retired) return NET_OK;
    if (q->run.cancelled) (void)net_upper_cancel(q->service, r->key);
    s = net_upper_tx_snapshot(q->service, r->key, &snapshot);
    if (s != NET_OK) return s;
    if (snapshot.conflict) return NET_HOLD;
    if (!r->terminal && snapshot.admission_known &&
        (!snapshot.accepted || snapshot.outcome != NET_TX_UNKNOWN)) {
        r->terminal = true;
        r->succeeded = snapshot.accepted && snapshot.outcome == NET_TX_SUCCEEDED;
        r->local_status = snapshot.local_status;
        if (r->succeeded) ++q->run.final_success_count;
        else { ++q->run.failed_response_count; q->run.partial = true; }
    }
    if (r->terminal && snapshot.reference_released) {
        s = net_upper_retire(q->service, r->key);
        if (s != NET_OK) return s;
        r->retired = true;
    }
    return NET_OK;
}

static NetStatus observe(NetUpperQuery *q)
{
    NetUpperQueryRun *r = &q->run;
    NetUpperTxSnapshot snapshot;
    NetStatus s;
    size_t i;
    for (i = 0u; i < r->response_count; ++i) {
        s = observe_one(q, &r->responses[i]);
        if (s != NET_OK) return s;
    }
    if (r->end_submitted && !r->end_retired) {
        if (r->cancelled) (void)net_upper_cancel(q->service, r->end_key);
        s = net_upper_tx_snapshot(q->service, r->end_key, &snapshot);
        if (s != NET_OK) return s;
        if (snapshot.conflict) return NET_HOLD;
        if (snapshot.admission_known &&
            (!snapshot.accepted || snapshot.outcome != NET_TX_UNKNOWN)) {
            r->end_terminal = true;
            r->end_succeeded = snapshot.accepted && snapshot.outcome == NET_TX_SUCCEEDED;
        }
        if (r->end_terminal && snapshot.reference_released) {
            s = net_upper_retire(q->service, r->end_key);
            if (s != NET_OK) return s;
            r->end_retired = true;
        }
    }
    if ((r->cancelled || r->end_terminal) && released_all(r)) r->finished = true;
    return NET_OK;
}

static NetStatus prepare_plan(NetUpperQuery *q)
{
    NetUpperQueryPlan plan;
    NetStatus s;
    if (q->owner.validate_passive == NULL || q->owner.select_plan == NULL)
        return NET_NOT_CONFIGURED;
    s = q->owner.validate_passive(q->owner.context, q->run.request.device_context_id,
                                q->run.request.session_id);
    if (s != NET_OK) return s;
    /* Failure plan OUT is deliberately unread. */
    s = q->owner.select_plan(q->owner.context, &q->run.request, &plan);
    if (s != NET_OK) return s;
    s = validate_plan(&q->run.request, &plan);
    if (s != NET_OK) return s;
    q->run.plan = plan;
    q->run.planned = true;
    return NET_OK;
}

static void getter_failed(NetUpperQueryRun *r, NetStatus status)
{
    r->getter_status[r->plan_index] = status;
    r->partial = true;
    ++r->failed_getter_count;
    ++r->plan_index;
}

static NetStatus produce_one(NetUpperQuery *q, uint64_t now)
{
    NetUpperQueryRun *r = &q->run;
    NetUpperQueryOutput outputs[NET_UPPER_QUERY_RECENT_CAPACITY];
    NetUpperQuerySelection selection;
    NetStatus s;
    size_t count, i;
    if (r->output_index < r->output_count) return NET_OK;
    r->output_count = r->output_index = 0u;
    if (r->plan_index == r->plan.count) { r->producing_done = true; return NET_OK; }
    selection = r->plan.selected[r->plan_index];
    if (selection.getter == NET_QUERY_WINDOW_STATE ||
        selection.getter == NET_QUERY_WINDOW_FAULT) {
        getter_failed(r, NET_HOLD);
        return NET_OK;
    }
    if (q->owner.getters[(unsigned)selection.getter] == NULL) {
        getter_failed(r, NET_NOT_CONFIGURED);
        return NET_OK;
    }
    s = q->owner.getters[(unsigned)selection.getter](q->owner.context,
        &r->request, selection.selector, now, outputs, selection.capacity, &count);
    if (s != NET_OK) { getter_failed(r, s); return NET_OK; }
    if (count > selection.capacity) { getter_failed(r, NET_NO_CAPACITY); return NET_OK; }
    /* Bounds are checked before any element/count-driven access. A successful
     * single-record absence is retained separately from a failed Getter. */
    r->getter_status[r->plan_index] = NET_OK;
    r->getter_count[r->plan_index] = count;
    if (count == 0u && selection.getter == NET_QUERY_REQUEST_RESULT)
        r->specific_not_found = true;
    for (i = 0u; i < count; ++i) {
        if (outputs[i].message.link != NET_WIRE_UART_M ||
            outputs[i].message.type != schemas[(unsigned)selection.getter] ||
            outputs[i].source.observation_id == 0u ||
            outputs[i].source.observed_at_ms > now || !outputs[i].age_policy.bound) {
            getter_failed(r, NET_INVALID_ARGUMENT);
            return NET_OK;
        }
        if (selection.getter == NET_QUERY_REQUEST_RESULT &&
            (outputs[i].message.data.m_result.request_session != r->request.request_session ||
             outputs[i].message.data.m_result.request_id != r->request.request_id)) {
            getter_failed(r, NET_HOLD);
            return NET_OK;
        }
        if (selection.getter == NET_QUERY_WARNING &&
            outputs[i].message.data.m_warning.warning_type != selection.selector) {
            getter_failed(r, NET_HOLD);
            return NET_OK;
        }
        if (net_upper_uart_set_query_context(&outputs[i].message,
            r->request.device_context_id, r->request.session_id, r->domain_boot,
            r->request.query_id) != NET_WIRE_OK) {
            getter_failed(r, NET_INVALID_ARGUMENT);
            return NET_OK;
        }
    }
    /* All selected Getter outputs are validated before copying or submitting
     * any of this Getter batch. Queue wait does not rerun the Getter. */
    memcpy(r->outputs, outputs, count * sizeof(outputs[0]));
    r->output_count = count;
    r->selected_response_count += count;
    ++r->plan_index;
    return NET_OK;
}

static NetStatus submit_one(NetUpperQuery *q)
{
    NetUpperQueryRun *r = &q->run;
    NetUpperQueryOutput *output;
    NetTxKey key;
    NetStatus s;
    NetUpperQueryResponse record;
    if (r->output_index == r->output_count) return NET_OK;
    if (r->response_count == NET_UPPER_QUERY_RESPONSE_CAPACITY) return NET_HOLD;
    output = &r->outputs[r->output_index];
    s = net_upper_submit_uart_auto(q->service, &output->message, &output->source,
                              &output->age_policy, &key);
    if (s == NET_BUSY || s == NET_NO_CAPACITY) return s;
    if (s == NET_OK) {
        memset(&record, 0, sizeof(record));
        record.key = key;
        record.type = output->message.type;
        r->responses[r->response_count++] = record;
    } else { ++r->failed_response_count; r->partial = true; }
    ++r->output_index;
    return NET_OK;
}

static bool terminal_all(const NetUpperQueryRun *r)
{
    size_t i;
    for (i = 0u; i < r->response_count; ++i)
        if (!r->responses[i].terminal) return false;
    return true;
}

static NetStatus submit_end(NetUpperQuery *q, uint64_t now)
{
    NetUpperQueryRun *r = &q->run;
    NetUpperUartMessage message;
    NetSourceStamp source;
    NetAgePolicy policy;
    NetTxKey key;
    NetStatus s;
    if (r->end_submitted || !r->producing_done ||
        r->output_index != r->output_count || !terminal_all(r)) return NET_OK;
    if (r->final_success_count > UINT16_MAX) return NET_HOLD;
    memset(&message, 0, sizeof(message));
    message.link = NET_WIRE_UART_M;
    message.type = 0x40u;
    message.data.m_query_end.device_context_id = r->request.device_context_id;
    message.data.m_query_end.session_id = r->request.session_id;
    message.data.m_query_end.domain_boot_id = r->domain_boot;
    message.data.m_query_end.query_id = r->request.query_id;
    message.data.m_query_end.query_scope = r->request.query_scope;
    message.data.m_query_end.query_status = r->partial ? 2u :
        (r->specific_not_found && r->final_success_count == 0u ? 1u : 0u);
    message.data.m_query_end.item_count = (uint16_t)r->final_success_count;
    /* This End is a newly-created local control record, not a sensor sample.
     * UINT64_MAX is a private retention bound, not a remote validity TTL. */
    source.observation_id = r->request.query_id;
    source.observed_at_ms = now;
    source.original_age_ms = 0u;
    policy.bound = true;
    policy.maximum_age_ms = UINT64_MAX;
    policy.expires_at_equality = false;
    s = net_upper_submit_uart_auto(q->service, &message, &source, &policy, &key);
    if (s == NET_OK) {
        r->end = message.data.m_query_end;
        r->end_key = key;
        r->end_submitted = true;
    }
    return s;
}

NetStatus net_upper_query_step(NetUpperQuery *q)
{
    NetStatus s;
    uint64_t now;
    if (q == NULL || !q->initialized) return NET_INVALID_ARGUMENT;
    if (q->in_call) return NET_BUSY;
    if (!q->run.active || q->run.finished) return NET_NO_WORK;
    q->in_call = true;
    if (!current(q)) q->run.cancelled = true;
    s = observe(q);
    if (s != NET_OK) goto hold;
    if (q->run.cancelled) { s = NET_HOLD; goto done; }
    if (q->run.held) { s = q->run.hold_reason; goto done; }
    s = net_upper_now(q->service, &now);
    if (s != NET_OK) goto hold;
    if (now < q->run.received_at_ms) { s = NET_HOLD; goto hold; }
    if (!q->run.planned) {
        s = prepare_plan(q);
        if (s != NET_OK) goto hold;
    } else {
        s = q->owner.validate_passive(q->owner.context,
            q->run.request.device_context_id, q->run.request.session_id);
        if (s != NET_OK) {
            q->run.cancelled = true;
            cancel_pending(q);
            goto done;
        }
    }
    if (!q->run.end_submitted) {
        s = produce_one(q, now);
        if (s != NET_OK) goto hold;
        s = submit_one(q);
        if (s == NET_BUSY || s == NET_NO_CAPACITY) goto done;
        if (s != NET_OK) goto hold;
        s = submit_end(q, now);
        if (s == NET_BUSY || s == NET_NO_CAPACITY) goto done;
        if (s != NET_OK) goto hold;
    }
    s = NET_OK;
    goto done;
hold:
    q->run.held = true;
    q->run.hold_reason = s;
done:
    q->in_call = false;
    return s;
}

NetStatus net_upper_query_snapshot(const NetUpperQuery *q, NetUpperQueryRun *out)
{
    if (q == NULL || out == NULL || !q->initialized) return NET_INVALID_ARGUMENT;
    if (q->in_call) return NET_BUSY;
    *out = q->run;
    return NET_OK;
}
