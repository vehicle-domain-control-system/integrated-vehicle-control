#include "network/net_upper_query.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define PORT_EVENTS 512u
#define PORT_ATTEMPTS 64u

/* The only fakes are physical Port/Clock and the external Domain Owner. M02,
 * M04, codecs, queues and TX outcome accounting are linked production code. */
typedef struct {
    NetTxKey key;
    const NetUpperPacket *borrow;
    NetUpperPacket saved;
} Attempt;
typedef struct {
    uint64_t now;
    NetUpperEvent events[PORT_EVENTS];
    size_t head, count, attempts;
    Attempt sent[PORT_ATTEMPTS];
    bool corrupt;
} FakePort;
typedef struct {
    NetUpperQueryPlan plan;
    NetStatus passive_status, plan_status, getter_status;
    size_t retained, recent_capacity, getter_calls, window_calls;
    bool overflow_count, empty, reenter, reentry_checked;
    NetUpperQuery *query;
    NetUpper *service;
    NetUpperRear committed_rear;
    size_t rear_commits;
    NetUpperCanMessage committed_can[9];
    bool can_committed[9];
} FakeDomain;
typedef struct {
    FakePort port;
    NetUpper service;
    NetUpperQuery query;
    FakeDomain owner;
    uint16_t sequence;
} Env;

static NetStatus clock_now(void *context, uint64_t *out)
{
    FakePort *p = (FakePort *)context;
    if (p == NULL || out == NULL) return NET_INVALID_ARGUMENT;
    *out = p->now;
    return NET_OK;
}
static NetStatus port_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    FakePort *p = (FakePort *)context;
    Attempt *attempt;
    if (p->attempts == PORT_ATTEMPTS) return NET_NO_CAPACITY;
    attempt = &p->sent[p->attempts++];
    attempt->key = key;
    attempt->borrow = packet;
    attempt->saved = *packet;
    printf("{\"event\":\"port_borrow\",\"attempt\":%u,\"type\":%u,\"epoch\":%llu,\"token\":%llu,\"wire\":\"",
        (unsigned)(p->attempts - 1u), (unsigned)packet->uart.bytes[3],
        (unsigned long long)key.epoch, (unsigned long long)key.token);
    {
        size_t i;
        for (i = 0u; i < packet->uart.length; ++i) printf("%02x", packet->uart.bytes[i]);
    }
    printf("\"}\n");
    return NET_OK;
}
static NetStatus port_poll(void *context, NetUpperEvent *out)
{
    FakePort *p = (FakePort *)context;
    NetUpperEvent event;
    size_t i;
    if (p->count == 0u) return NET_NO_WORK;
    event = p->events[p->head];
    p->head = (p->head + 1u) % PORT_EVENTS;
    --p->count;
    if (event.kind == NET_UPPER_TX_EVENT && event.tx.release_observed) {
        for (i = 0u; i < p->attempts; ++i) {
            Attempt *a = &p->sent[i];
            if (a->key.epoch == event.tx.key.epoch && a->key.token == event.tx.key.token && a->borrow != NULL) {
                if (memcmp(a->borrow, &a->saved, sizeof(a->saved)) != 0) p->corrupt = true;
                a->borrow = NULL;
            }
        }
    }
    *out = event;
    return NET_OK;
}
static const NetUpperPortOps port_ops = {clock_now, port_submit, port_poll};
static const uint16_t projection_ids[9] = {
    0x200u, 0x210u, 0x220u, 0x230u, 0x300u, 0x310u, 0x0a0u, 0x320u, 0x340u
};

static int push(FakePort *p, const NetUpperEvent *event)
{
    CHECK(p->count < PORT_EVENTS);
    p->events[(p->head + p->count) % PORT_EVENTS] = *event;
    ++p->count;
    return 0;
}
static int tx_event(Env *e, size_t index, bool admission, bool accepted,
                    NetTxOutcome outcome, bool release)
{
    NetUpperEvent event;
    CHECK(index < e->port.attempts);
    memset(&event, 0, sizeof(event));
    event.kind = NET_UPPER_TX_EVENT;
    event.link = NET_WIRE_UART_M;
    event.capture.port_epoch = 1u;
    event.capture.timestamp_known = true;
    event.capture.captured_at_ms = e->port.now;
    event.tx.key = e->port.sent[index].key;
    event.tx.admission_observed = admission;
    event.tx.accepted = accepted;
    event.tx.outcome = outcome;
    event.tx.release_observed = release;
    return push(&e->port, &event);
}
static NetStatus passive(void *context, uint32_t device, uint64_t session)
{
    FakeDomain *d = (FakeDomain *)context;
    if (device != 17u || session != UINT64_C(0xbb)) return NET_REJECTED;
    return d->passive_status;
}
static NetStatus select_plan(void *context, const NetWireMQueryRaw *request,
                              NetUpperQueryPlan *out)
{
    FakeDomain *d = (FakeDomain *)context;
    (void)request;
    if (d->plan_status != NET_OK) {
        memset(out, 0xa5, sizeof(*out));
        return d->plan_status;
    }
    *out = d->plan;
    return NET_OK;
}

static NetStatus get_common(FakeDomain *d, NetUpperQueryGetter getter,
    const NetWireMQueryRaw *request, uint8_t selector, uint64_t now,
    NetUpperQueryOutput *out, size_t capacity, size_t *count)
{
    size_t i, n = getter == NET_QUERY_RECENT_RESULTS ?
        (d->retained < capacity ? d->retained : capacity) : 1u;
    ++d->getter_calls;
    if (getter == NET_QUERY_RECENT_RESULTS) d->recent_capacity = capacity;
    if (getter == NET_QUERY_WINDOW_STATE || getter == NET_QUERY_WINDOW_FAULT) ++d->window_calls;
    if (d->reenter) {
        NetUpperQueryRun snapshot;
        d->reentry_checked = net_upper_query_step(d->query) == NET_BUSY &&
            net_upper_query_invalidate(d->query) == NET_BUSY &&
            net_upper_query_snapshot(d->query, &snapshot) == NET_BUSY;
    }
    if (d->getter_status != NET_OK) {
        memset(out, 0xa5, capacity * sizeof(*out));
        *count = SIZE_MAX;
        return d->getter_status;
    }
    if (d->empty) { *count = 0u; return NET_OK; }
    for (i = 0u; i < n; ++i) {
        NetUpperQueryOutput *o = &out[i];
        memset(o, 0, sizeof(*o));
        o->message.link = NET_WIRE_UART_M;
        o->message.type = net_upper_query_schema(getter);
        o->source.observation_id = 100u + i;
        o->source.observed_at_ms = now;
        o->source.original_age_ms = 7u;
        o->age_policy.bound = true;
        o->age_policy.maximum_age_ms = 10000u;
        switch (getter) {
        case NET_QUERY_REQUEST_RESULT:
        case NET_QUERY_RECENT_RESULTS:
            o->message.data.m_result.request_session = getter == NET_QUERY_REQUEST_RESULT ?
                request->request_session : UINT64_C(0xaa);
            o->message.data.m_result.request_id = getter == NET_QUERY_REQUEST_RESULT ?
                request->request_id : (uint32_t)(d->retained - i);
            o->message.data.m_result.result = UINT8_MAX;
            o->message.data.m_result.result_age_ms = 7u;
            break;
        case NET_QUERY_WARNING:
            o->message.data.m_warning.warning_type = selector;
            o->message.data.m_warning.occurrence_id = 1000u + selector;
            o->message.data.m_warning.warning_state = 1u;
            o->message.data.m_warning.source_boot = 3u;
            o->message.data.m_warning.source_sequence = selector;
            o->message.data.m_warning.source_age_ms = 7u;
            break;
        case NET_QUERY_DIGITAL_STATE:
            o->message.data.m_digital_status.update_sequence = 4u;
            o->message.data.m_digital_status.source_age_ms = 7u;
            break;
        case NET_QUERY_USER_SETTINGS:
            o->message.data.m_user_settings.target_temperature_centi_c = 2150;
            o->message.data.m_user_settings.source_age_ms = 7u;
            break;
        case NET_QUERY_AVAILABILITY:
            {
                size_t j;
                for (j = 0u; j < 9u; ++j)
                    o->message.data.m_availability.items[j].function_id = (uint8_t)(j + 1u);
                o->message.data.m_availability.source_age_ms = 7u;
            }
            break;
        case NET_QUERY_DIGITAL_RESULT:
            o->message.data.m_digital_result.origin = 2u;
            o->message.data.m_digital_result.result = UINT8_MAX;
            o->message.data.m_digital_result.result_age_ms = 7u;
            break;
        case NET_QUERY_BCM_DOOR: case NET_QUERY_BCM_CLIMATE:
        case NET_QUERY_BCM_LIGHT: case NET_QUERY_BCM_STATUS:
        case NET_QUERY_CIS_ENVIRONMENT: case NET_QUERY_CIS_OCCUPANT:
        case NET_QUERY_CIS_REAR: case NET_QUERY_CIS_STATUS: case NET_QUERY_VSS_STATUS:
            {
                NetUpperCanMessage can;
                NetUpperDelivery delivery;
                NetStatus s = net_upper_received_can(d->service,
                    projection_ids[(unsigned)getter], &can, &delivery, &o->source);
                if (s != NET_OK) return NET_HOLD;
                if (getter == NET_QUERY_CIS_REAR) {
                    if (d->rear_commits != 1u || d->committed_rear.rear_distance_mm != 123u)
                        return NET_HOLD;
                } else {
                    NetWireCanEnvelope committed, accepted;
                    if (!d->can_committed[(unsigned)getter]) return NET_HOLD;
                    if (net_upper_can_encode(PROFILE, &d->committed_can[(unsigned)getter], &committed) != NET_WIRE_OK ||
                        net_upper_can_encode(PROFILE, &can, &accepted) != NET_WIRE_OK ||
                        committed.length != accepted.length ||
                        memcmp(committed.bytes, accepted.bytes, accepted.length) != 0) return NET_HOLD;
                }
                if (net_upper_uart_project_can(PROFILE, &can, request->device_context_id,
                    request->session_id, 9u, request->query_id, 0u, &o->message) != NET_WIRE_OK)
                    return NET_HOLD;
            }
            break;
        default: return NET_HOLD;
        }
    }
    *count = d->overflow_count ? capacity + 1u : n;
    return NET_OK;
}

#define GETTER(name_, id_) \
static NetStatus name_(void *c, const NetWireMQueryRaw *r, uint8_t s, uint64_t n, \
                      NetUpperQueryOutput *o, size_t cap, size_t *count) \
{ return get_common((FakeDomain *)c, id_, r, s, n, o, cap, count); }
GETTER(get_recent, NET_QUERY_RECENT_RESULTS)
GETTER(get_single, NET_QUERY_REQUEST_RESULT)
GETTER(get_warning, NET_QUERY_WARNING)
GETTER(get_digital, NET_QUERY_DIGITAL_STATE)
GETTER(get_settings, NET_QUERY_USER_SETTINGS)
GETTER(get_availability, NET_QUERY_AVAILABILITY)
GETTER(get_digital_result, NET_QUERY_DIGITAL_RESULT)
GETTER(get_window, NET_QUERY_WINDOW_STATE)
GETTER(get_window_fault, NET_QUERY_WINDOW_FAULT)
GETTER(get_rear, NET_QUERY_CIS_REAR)
GETTER(get_door, NET_QUERY_BCM_DOOR)
GETTER(get_climate, NET_QUERY_BCM_CLIMATE)
GETTER(get_light, NET_QUERY_BCM_LIGHT)
GETTER(get_bcm_status, NET_QUERY_BCM_STATUS)
GETTER(get_environment, NET_QUERY_CIS_ENVIRONMENT)
GETTER(get_occupant, NET_QUERY_CIS_OCCUPANT)
GETTER(get_cis_status, NET_QUERY_CIS_STATUS)
GETTER(get_vss_status, NET_QUERY_VSS_STATUS)
#undef GETTER

static NetStatus fake_rear(void *context, const NetUpperRear *rear,
                           const NetUpperDelivery *delivery)
{
    NetUpperQuery *query = (NetUpperQuery *)context;
    FakeDomain *d = (FakeDomain *)query->owner.context;
    if (!delivery->freshness_known || rear->rear_distance_mm != 123u) return NET_REJECTED;
    d->committed_rear = *rear;
    ++d->rear_commits;
    return NET_OK;
}
static NetStatus fake_can(void *context, const NetUpperCanMessage *message,
                         const NetUpperDelivery *delivery)
{
    NetUpperQuery *query = (NetUpperQuery *)context;
    FakeDomain *d = (FakeDomain *)query->owner.context;
    size_t i;
    if (!delivery->freshness_known) return NET_REJECTED;
    for (i = 0u; i < 9u; ++i) if (projection_ids[i] == message->id) {
        d->committed_can[i] = *message;
        d->can_committed[i] = true;
        return NET_OK;
    }
    return NET_UNSUPPORTED;
}
static int setup(Env *e)
{
    NetUpperConfig config;
    NetUpperQueryOwner owner;
    size_t i;
    memset(e, 0, sizeof(*e));
    memset(&config, 0, sizeof(config));
    memset(&owner, 0, sizeof(owner));
    e->port.now = 1000u;
    e->sequence = 1u;
    e->owner.retained = 8u;
    e->owner.query = &e->query;
    e->owner.service = &e->service;
    config.node = NET_WIRE_NODE_DOMAIN;
    config.local_epoch = 1u;
    config.profile = PROFILE;
    config.uart_sequence_bound[0] = true;
    config.uart_initial_sequence[0] = 100u;
    config.port.ops = &port_ops;
    config.port.context = &e->port;
    config.owner.context = &e->query;
    config.owner.uart = net_upper_query_receive;
    config.owner.rear = fake_rear;
    config.owner.can = fake_can;
    for (i = 0u; i < 3u; ++i) config.port_epoch[i] = 1u;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        config.rx_policy[i].bound = true;
        config.rx_policy[i].maximum_age_ms = 10000u;
    }
    for (i = 0u; i < 2u; ++i) {
        config.uart_policy[i].candidate_time_policy_selected = true;
        config.uart_policy[i].assembly_limit_ms = 1000u;
    }
    CHECK(net_upper_init(&e->service, &config) == NET_OK);
    owner.context = &e->owner;
    owner.validate_passive = passive;
    owner.select_plan = select_plan;
    owner.getters[NET_QUERY_RECENT_RESULTS] = get_recent;
    owner.getters[NET_QUERY_REQUEST_RESULT] = get_single;
    owner.getters[NET_QUERY_WARNING] = get_warning;
    owner.getters[NET_QUERY_DIGITAL_STATE] = get_digital;
    owner.getters[NET_QUERY_USER_SETTINGS] = get_settings;
    owner.getters[NET_QUERY_AVAILABILITY] = get_availability;
    owner.getters[NET_QUERY_DIGITAL_RESULT] = get_digital_result;
    owner.getters[NET_QUERY_WINDOW_STATE] = get_window;
    owner.getters[NET_QUERY_WINDOW_FAULT] = get_window_fault;
    owner.getters[NET_QUERY_CIS_REAR] = get_rear;
    owner.getters[NET_QUERY_BCM_DOOR] = get_door;
    owner.getters[NET_QUERY_BCM_CLIMATE] = get_climate;
    owner.getters[NET_QUERY_BCM_LIGHT] = get_light;
    owner.getters[NET_QUERY_BCM_STATUS] = get_bcm_status;
    owner.getters[NET_QUERY_CIS_ENVIRONMENT] = get_environment;
    owner.getters[NET_QUERY_CIS_OCCUPANT] = get_occupant;
    owner.getters[NET_QUERY_CIS_STATUS] = get_cis_status;
    owner.getters[NET_QUERY_VSS_STATUS] = get_vss_status;
    CHECK(net_upper_query_init(&e->query, &e->service, 9u, &owner) == NET_OK);
    return 0;
}
static void add_plan(Env *e, NetUpperQueryGetter getter, uint8_t selector, uint8_t capacity)
{
    NetUpperQuerySelection *s = &e->owner.plan.selected[e->owner.plan.count++];
    s->getter = getter; s->selector = selector; s->capacity = capacity;
}
static int queue_frame(Env *e, const NetWireUartFrame *frame, size_t first, size_t limit)
{
    size_t i;
    for (i = first; i < limit; ++i) {
        NetUpperEvent event;
        memset(&event, 0, sizeof(event));
        event.kind = NET_UPPER_UART_BYTE;
        event.link = NET_WIRE_UART_M;
        event.byte = frame->bytes[i];
        event.capture.port_epoch = 1u;
        event.capture.timestamp_known = true;
        event.capture.captured_at_ms = ++e->port.now;
        CHECK(push(&e->port, &event) == 0);
    }
    return 0;
}
static int service_drain(Env *e)
{
    size_t i;
    for (i = 0u; i < 40u; ++i) {
        NetStatus s = net_upper_step(&e->service, NET_UPPER_RX_CAPACITY);
        CHECK(s == NET_OK || s == NET_NO_WORK);
        if (e->port.count == 0u && e->service.rx_count == 0u) break;
    }
    CHECK(i < 40u && !e->port.corrupt);
    return 0;
}
static int send_context(Env *e, uint64_t session)
{
    NetUpperUartMessage m;
    NetWireUartFrame frame;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_M; m.type = 1u; m.sequence = e->sequence++;
    m.data.m_context.device_context_id = 17u;
    m.data.m_context.session_id = session;
    m.data.m_context.proximity_validity = 1u;
    CHECK(net_upper_uart_encode(PROFILE, &m, &frame) == NET_WIRE_OK);
    CHECK(queue_frame(e, &frame, 0u, frame.length) == 0);
    CHECK(service_drain(e) == 0);
    return 0;
}
static int send_query(Env *e, uint8_t scope, bool specific)
{
    NetUpperUartMessage m;
    NetWireUartFrame frame;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_M; m.type = 0x11u; m.sequence = e->sequence++;
    m.data.m_query.device_context_id = 17u;
    m.data.m_query.session_id = UINT64_C(0xbb);
    m.data.m_query.query_id = 19u;
    m.data.m_query.query_scope = scope;
    if (specific) { m.data.m_query.request_session = UINT64_C(0xaa); m.data.m_query.request_id = 71u; }
    CHECK(net_upper_uart_encode(PROFILE, &m, &frame) == NET_WIRE_OK);
    CHECK(queue_frame(e, &frame, 0u, frame.length) == 0);
    CHECK(service_drain(e) == 0);
    CHECK(e->query.run.active);
    return 0;
}
static int run_steps(Env *e, size_t count)
{
    size_t i;
    for (i = 0u; i < count; ++i) {
        NetStatus s = net_upper_query_step(&e->query);
        CHECK(s == NET_OK || s == NET_BUSY || s == NET_NO_CAPACITY ||
              s == NET_NO_WORK || s == NET_HOLD || s == NET_REJECTED ||
              s == NET_NOT_CONFIGURED || s == NET_INVALID_ARGUMENT);
        CHECK(service_drain(e) == 0);
    }
    return 0;
}
static int finish(Env *e, size_t index, NetTxOutcome outcome)
{
    CHECK(tx_event(e, index, true, true, outcome, true) == 0);
    CHECK(service_drain(e) == 0);
    CHECK(run_steps(e, 3u) == 0);
    return 0;
}
static int decode_attempt(const Env *e, size_t index, NetUpperUartMessage *out)
{
    CHECK(index < e->port.attempts);
    CHECK(net_upper_uart_decode(PROFILE, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        &e->port.sent[index].saved.uart, out) == NET_WIRE_OK);
    return 0;
}
static void trace(const char *name, const Env *e)
{
    const NetUpperQueryRun *r = &e->query.run;
    printf("{\"test\":\"%s\",\"product\":\"net_upper_query_receive/step\",\"selected\":%u,\"submitted\":%u,\"final_success\":%u,\"failed\":%u,\"getter_failed\":%u,\"end_submitted\":%s,\"end_status\":%u,\"end_count\":%u,\"end_final_success\":%s,\"cancelled\":%s,\"held\":%s,\"owner\":\"FakeDomain\",\"external_gates\":\"HOLD\"}\n",
        name, (unsigned)r->selected_response_count, (unsigned)r->response_count,
        (unsigned)r->final_success_count, (unsigned)r->failed_response_count,
        (unsigned)r->failed_getter_count, r->end_submitted ? "true" : "false",
        (unsigned)r->end.query_status, (unsigned)r->end.item_count,
        r->end_succeeded ? "true" : "false", r->cancelled ? "true" : "false",
        r->held ? "true" : "false");
}

static int recent_count(size_t capacity)
{
    Env e;
    NetUpperUartMessage m;
    size_t i;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_RECENT_RESULTS, 0u, (uint8_t)capacity);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0);
    CHECK(send_query(&e, 1u, false) == 0);
    CHECK(run_steps(&e, 12u) == 0);
    CHECK(e.port.attempts == capacity && e.owner.recent_capacity == capacity);
    CHECK(e.owner.retained == 8u && e.owner.getter_calls == 1u);
    for (i = 0u; i < capacity; ++i) {
        CHECK(decode_attempt(&e, i, &m) == 0);
        CHECK(m.type == 0x20u && m.data.m_result.request_id == 8u - i);
        CHECK(m.data.m_result.request_session == UINT64_C(0xaa));
        CHECK(m.data.m_result.session_id == UINT64_C(0xbb));
        CHECK(m.data.m_result.query_id == 19u && m.data.m_result.result == UINT8_MAX);
        CHECK(tx_event(&e, i, true, true, NET_TX_UNKNOWN, true) == 0);
    }
    CHECK(service_drain(&e) == 0 && run_steps(&e, 3u) == 0);
    CHECK(e.query.run.final_success_count == 0u && !e.query.run.end_submitted);
    for (i = 0u; i < capacity; ++i)
        CHECK(tx_event(&e, i, false, false, NET_TX_SUCCEEDED, false) == 0);
    CHECK(service_drain(&e) == 0 && run_steps(&e, 4u) == 0);
    CHECK(e.port.attempts == capacity + 1u);
    CHECK(decode_attempt(&e, capacity, &m) == 0);
    CHECK(m.type == 0x40u && m.data.m_query_end.item_count == capacity && m.data.m_query_end.query_status == 0u);
    CHECK(finish(&e, capacity, NET_TX_SUCCEEDED) == 0);
    CHECK(e.query.run.finished && e.query.run.end_succeeded);
    CHECK(e.query.run.final_success_count == capacity);
    CHECK(tx_event(&e, 0u, false, false, NET_TX_SUCCEEDED, true) == 0);
    CHECK(net_upper_step(&e.service, NET_UPPER_RX_CAPACITY) == NET_STALE);
    CHECK(run_steps(&e, 1u) == 0);
    CHECK(e.query.run.final_success_count == capacity);
    trace(capacity == 5u ? "recent8_select5" : "recent_actual_capacity3", &e);
    return 0;
}
static int recent_five(void) { return recent_count(5u); }
static int recent_three(void) { return recent_count(3u); }
static int poisoned_failure(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_RECENT_RESULTS, 0u, 5u);
    e.owner.getter_status = NET_REJECTED;
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 1u, false) == 0);
    CHECK(run_steps(&e, 4u) == 0);
    CHECK(e.query.run.failed_getter_count == 1u && e.query.run.response_count == 0u);
    CHECK(e.query.run.end_submitted && e.query.run.end.query_status == 2u && e.query.run.end.item_count == 0u);
    CHECK(e.port.attempts == 1u);
    trace("failed_getter_poisoned_out_ignored", &e);
    return 0;
}
static int overflow_count(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_RECENT_RESULTS, 0u, 5u);
    e.owner.overflow_count = true;
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 1u, false) == 0);
    CHECK(run_steps(&e, 4u) == 0);
    CHECK(e.query.run.getter_status[0] == NET_NO_CAPACITY && e.query.run.response_count == 0u);
    CHECK(e.query.run.end.query_status == 2u && e.query.run.end.item_count == 0u);
    trace("getter_count6_capacity5_rejected", &e);
    return 0;
}
static int ten_eight_final(void)
{
    Env e;
    NetUpperUartMessage m;
    size_t i;
    CHECK(setup(&e) == 0);
    for (i = 1u; i <= 8u; ++i) add_plan(&e, NET_QUERY_WARNING, (uint8_t)i, 1u);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    add_plan(&e, NET_QUERY_USER_SETTINGS, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 3u, false) == 0);
    CHECK(run_steps(&e, 16u) == 0 && e.port.attempts == 10u);
    for (i = 0u; i < 10u; ++i)
        CHECK(tx_event(&e, i, true, true, i < 8u ? NET_TX_SUCCEEDED : NET_TX_UNKNOWN, true) == 0);
    CHECK(service_drain(&e) == 0 && run_steps(&e, 3u) == 0);
    CHECK(e.query.run.final_success_count == 8u && !e.query.run.end_submitted);
    trace("ten_admitted_eight_final_two_unresolved_no_end", &e);
    CHECK(tx_event(&e, 8u, false, false, NET_TX_FAILED, false) == 0);
    CHECK(tx_event(&e, 9u, false, false, NET_TX_FAILED, false) == 0);
    CHECK(service_drain(&e) == 0 && run_steps(&e, 3u) == 0);
    CHECK(e.port.attempts == 11u && decode_attempt(&e, 10u, &m) == 0);
    CHECK(m.type == 0x40u && m.data.m_query_end.query_status == 2u && m.data.m_query_end.item_count == 8u);
    CHECK(finish(&e, 10u, NET_TX_FAILED) == 0);
    CHECK(e.query.run.end_terminal && !e.query.run.end_succeeded && e.query.run.final_success_count == 8u);
    trace("two_final_failures_partial8_end_final_failed", &e);
    return 0;
}
static int final_before_release(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 3u) == 0 && e.port.attempts == 1u);
    CHECK(tx_event(&e, 0u, true, true, NET_TX_SUCCEEDED, false) == 0);
    CHECK(service_drain(&e) == 0 && run_steps(&e, 3u) == 0);
    CHECK(e.query.run.final_success_count == 1u && !e.query.run.responses[0].retired);
    CHECK(e.port.sent[0].borrow != NULL && e.port.attempts == 2u);
    CHECK(finish(&e, 1u, NET_TX_SUCCEEDED) == 0 && !e.query.run.finished);
    CHECK(tx_event(&e, 0u, false, false, NET_TX_UNKNOWN, true) == 0);
    CHECK(service_drain(&e) == 0 && run_steps(&e, 2u) == 0);
    CHECK(e.query.run.finished && !e.port.corrupt);
    trace("final_before_release_stable_frame", &e);
    return 0;
}
static int empty_not_found(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 2u) == 0);
    CHECK(e.query.run.end_submitted && e.query.run.end.query_status == 0u && e.query.run.end.item_count == 0u);
    trace("confirmed_empty_done0", &e);
    CHECK(finish(&e, 0u, NET_TX_SUCCEEDED) == 0);
    /* A fresh fixture is not a live service reset. Its lifetime is separate. */
    {
        Env other;
        CHECK(setup(&other) == 0);
        add_plan(&other, NET_QUERY_REQUEST_RESULT, 0u, 1u);
        other.owner.empty = true;
        CHECK(send_context(&other, UINT64_C(0xbb)) == 0 && send_query(&other, 1u, true) == 0);
        CHECK(run_steps(&other, 3u) == 0);
        CHECK(other.query.run.end.query_status == 1u && other.query.run.end.item_count == 0u);
        trace("specific_absence_not_found0", &other);
    }
    return 0;
}
static int reentrant_getter(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    e.owner.reenter = true;
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 3u) == 0);
    CHECK(e.owner.reentry_checked && !e.query.run.cancelled && e.query.run.response_count == 1u);
    CHECK(finish(&e, 0u, NET_TX_SUCCEEDED) == 0);
    CHECK(e.query.run.end.item_count == 1u);
    trace("owner_reentry_blocked", &e);
    return 0;
}
static int window_local_hold(void)
{
    Env e;
    size_t i, j, unique = 0u;
    CHECK(setup(&e) == 0);
    for (i = 0u; i < NET_UPPER_QUERY_GETTERS; ++i) {
        bool seen = false;
        CHECK(net_upper_query_schema((NetUpperQueryGetter)i) != 0u);
        for (j = 0u; j < i; ++j)
            if (net_upper_query_schema((NetUpperQueryGetter)i) == net_upper_query_schema((NetUpperQueryGetter)j)) seen = true;
        if (!seen) ++unique;
    }
    CHECK(unique == 17u && net_upper_query_schema((NetUpperQueryGetter)18u) == 0u);
    add_plan(&e, NET_QUERY_WINDOW_STATE, 0u, 1u);
    add_plan(&e, NET_QUERY_WINDOW_FAULT, 0u, 1u);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 5u) == 0);
    CHECK(e.owner.window_calls == 0u && e.query.run.failed_getter_count == 2u && e.port.attempts == 1u);
    CHECK(finish(&e, 0u, NET_TX_SUCCEEDED) == 0);
    CHECK(e.query.run.end.query_status == 2u && e.query.run.end.item_count == 1u);
    trace("window2_hold_independent_getter_runs_18_to17_map", &e);
    return 0;
}
static int plan_hold(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    e.owner.plan_status = NET_HOLD;
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 3u, false) == 0);
    CHECK(run_steps(&e, 3u) == 0);
    CHECK(e.query.run.held && !e.query.run.planned && e.owner.getter_calls == 0u && e.port.attempts == 0u);
    trace("owner_selection_contract_hold_poison_plan_ignored", &e);
    return 0;
}

static int session_change(void)
{
    Env e;
    NetUpperUartMessage m;
    NetWireUartFrame context;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 3u) == 0 && e.port.attempts == 1u);
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_M; m.type = 1u; m.sequence = e.sequence++;
    m.data.m_context.device_context_id = 17u; m.data.m_context.session_id = UINT64_C(0xcc);
    m.data.m_context.proximity_validity = 1u;
    CHECK(net_upper_uart_encode(PROFILE, &m, &context) == NET_WIRE_OK);
    CHECK(queue_frame(&e, &context, 0u, context.length - 1u) == 0);
    CHECK(service_drain(&e) == 0);
    CHECK(tx_event(&e, 0u, true, true, NET_TX_SUCCEEDED, true) == 0);
    CHECK(service_drain(&e) == 0);
    CHECK(net_upper_query_step(&e.query) == NET_OK);
    CHECK(e.query.run.end_submitted && e.port.attempts == 1u);
    /* New context completes before the already queued End's dispatch. The
     * actual service must reject that old recipient before Port borrowing. */
    CHECK(queue_frame(&e, &context, context.length - 1u, context.length) == 0);
    CHECK(net_upper_step(&e.service, NET_UPPER_RX_CAPACITY) == NET_STALE);
    CHECK(run_steps(&e, 3u) == 0);
    CHECK(e.query.run.cancelled && !e.query.run.end_succeeded && e.port.attempts == 1u);
    CHECK(e.query.run.final_success_count == 1u);
    trace("session_change_blocks_queued_old_end", &e);
    return 0;
}

static int passive_revocation(void)
{
    Env e;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(net_upper_query_step(&e.query) == NET_OK);
    CHECK(e.query.run.response_count == 1u && e.port.attempts == 0u);
    e.owner.passive_status = NET_REJECTED;
    CHECK(net_upper_query_step(&e.query) == NET_REJECTED);
    CHECK(service_drain(&e) == 0 && run_steps(&e, 2u) == 0);
    CHECK(e.query.run.cancelled && e.query.run.finished && e.port.attempts == 0u);
    CHECK(!e.query.run.end_submitted && e.query.run.final_success_count == 0u);
    trace("passive_revocation_cancels_queued_before_dispatch", &e);
    return 0;
}

static int reset_hold(void)
{
    Env e;
    NetUpperEvent reset;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_DIGITAL_STATE, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(net_upper_query_step(&e.query) == NET_OK);
    memset(&reset, 0, sizeof(reset));
    reset.kind = NET_UPPER_LINK_RESET; reset.link = NET_WIRE_UART_M;
    reset.capture.port_epoch = 2u; reset.capture.timestamp_known = true;
    reset.capture.captured_at_ms = e.port.now;
    CHECK(push(&e.port, &reset) == 0 && service_drain(&e) == 0);
    CHECK(run_steps(&e, 2u) == 0);
    CHECK(e.query.run.cancelled && e.port.attempts == 0u && !e.query.run.end_submitted);
    CHECK(!e.service.context_seen && e.service.config.port_epoch[NET_WIRE_UART_M] == 2u);
    trace("link_reset_without_new_context_blocks_old_query", &e);
    return 0;
}

static int queue_pressure(void)
{
    Env e;
    NetUpperUartMessage spontaneous, actual;
    NetSourceStamp source = {99u, 0u, 0u};
    NetAgePolicy policy = {true, UINT64_MAX, false};
    NetTxKey keys[NET_UPPER_TX_CAPACITY];
    size_t i;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_RECENT_RESULTS, 0u, 5u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 1u, false) == 0);
    memset(&spontaneous, 0, sizeof(spontaneous));
    spontaneous.link = NET_WIRE_UART_M; spontaneous.type = 0x20u;
    spontaneous.data.m_result.device_context_id = 17u;
    spontaneous.data.m_result.session_id = UINT64_C(0xbb);
    spontaneous.data.m_result.domain_boot_id = 9u;
    spontaneous.data.m_result.result = UINT8_MAX;
    source.observed_at_ms = e.port.now;
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i) {
        spontaneous.data.m_result.request_id = (uint32_t)(100u + i);
        CHECK(net_upper_submit_uart_auto(&e.service, &spontaneous, &source, &policy, &keys[i]) == NET_OK);
    }
    CHECK(net_upper_query_step(&e.query) == NET_NO_CAPACITY);
    CHECK(e.owner.getter_calls == 1u && e.query.run.output_count == 5u && e.query.run.response_count == 0u);
    e.owner.retained = 99u; /* Accepted Getter copy must remain8..4. */
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i) {
        CHECK(net_upper_cancel(&e.service, keys[i]) == NET_OK);
        CHECK(net_upper_retire(&e.service, keys[i]) == NET_OK);
    }
    CHECK(run_steps(&e, 8u) == 0 && e.port.attempts == 5u);
    CHECK(e.owner.getter_calls == 1u && e.query.run.response_count == 5u);
    CHECK(decode_attempt(&e, 0u, &actual) == 0 && actual.data.m_result.request_id == 8u);
    CHECK(e.query.run.final_success_count == 0u && !e.query.run.end_submitted);
    trace("queue_full_holds_stable_outputs_without_getter_retry", &e);
    return 0;
}

static int consecutive_end_sequence(void)
{
    Env e;
    NetUpperUartMessage first, second, request;
    NetWireUartFrame frame;
    CHECK(setup(&e) == 0);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 2u) == 0 && finish(&e, 0u, NET_TX_SUCCEEDED) == 0);
    CHECK(decode_attempt(&e, 0u, &first) == 0 && first.sequence == 100u);
    memset(&request, 0, sizeof(request));
    request.link = NET_WIRE_UART_M; request.type = 0x11u; request.sequence = e.sequence++;
    request.data.m_query = e.query.run.request;
    request.data.m_query.query_id = 20u;
    CHECK(net_upper_uart_encode(PROFILE, &request, &frame) == NET_WIRE_OK);
    CHECK(queue_frame(&e, &frame, 0u, frame.length) == 0 && service_drain(&e) == 0);
    CHECK(run_steps(&e, 2u) == 0 && e.port.attempts == 2u);
    CHECK(decode_attempt(&e, 1u, &second) == 0);
    CHECK(second.type == 0x40u && second.sequence == 101u && second.data.m_query_end.query_id == 20u);
    CHECK(first.data.m_query_end.query_id == 19u && second.data.m_query_end.item_count == 0u);
    CHECK(finish(&e, 1u, NET_TX_SUCCEEDED) == 0);
    trace("consecutive_query_ends_unique_real_local_link_sequence", &e);
    return 0;
}

static int rear_projection(void)
{
    Env e;
    NetUpperEvent event;
    NetUpperCanMessage can;
    NetUpperUartMessage output;
    CHECK(setup(&e) == 0);
    memset(&event, 0, sizeof(event));
    memset(&can, 0, sizeof(can));
    can.id = 0x0a0u;
    can.data.cis_rear.boot_id = 5u;
    can.data.cis_rear.tx_sequence = 1u;
    can.data.cis_rear.update_sequence = 7u;
    can.data.cis_rear.rear_distance_deci_cm = 123u;
    can.data.cis_rear.validity = 1u;
    CHECK(net_upper_can_encode(PROFILE, &can, &event.can) == NET_WIRE_OK);
    event.kind = NET_UPPER_CAN_RX;
    event.link = NET_WIRE_CAN;
    event.capture.port_epoch = 1u;
    event.capture.timestamp_known = true;
    event.capture.captured_at_ms = e.port.now;
    CHECK(push(&e.port, &event) == 0 && service_drain(&e) == 0);
    CHECK(e.owner.rear_commits == 1u && e.owner.committed_rear.rear_distance_mm == 123u);
    add_plan(&e, NET_QUERY_CIS_REAR, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 0u, false) == 0);
    CHECK(run_steps(&e, 3u) == 0 && e.port.attempts == 1u);
    CHECK(decode_attempt(&e, 0u, &output) == 0);
    CHECK(output.type == 0x36u && output.data.m_cis_rear.rear_distance_deci_cm == 123u);
    CHECK(output.data.m_cis_rear.original_can_boot_id == 5u && output.data.m_cis_rear.original_can_tx_sequence == 1u);
    CHECK(output.data.m_cis_rear.source_age_ms >= 90u);
    CHECK(finish(&e, 0u, NET_TX_SUCCEEDED) == 0 && e.query.run.end.item_count == 1u);
    trace("actual_can_owner_commit_getter_projection123mm", &e);
    return 0;
}

static int all_selected_getters(void)
{
    Env e;
    NetUpperCanMessage can;
    NetUpperEvent event;
    size_t i, acknowledged = 0u, turns, unique = 0u;
    bool schema_seen[256] = {false};
    CHECK(setup(&e) == 0);
    for (i = 0u; i < 9u; ++i) {
        memset(&can, 0, sizeof(can));
        memset(&event, 0, sizeof(event));
        can.id = projection_ids[i];
#define BASE(member_) do { can.data.member_.boot_id = 5u; can.data.member_.tx_sequence = 1u; } while (0)
        switch (i) {
        case 0u: BASE(bcm_door_state); break;
        case 1u: BASE(bcm_climate_state); break;
        case 2u: BASE(bcm_light_state); break;
        case 3u: BASE(bcm_status); break;
        case 4u:
            BASE(cis_environment);
            can.data.cis_environment.temperature_validity = 1u;
            can.data.cis_environment.humidity_validity = 1u;
            can.data.cis_environment.illuminance_validity = 1u;
            break;
        case 5u:
            BASE(cis_occupant);
            can.data.cis_occupant.pi_boot_id = 8u;
            can.data.cis_occupant.vision_sequence = 2u;
            can.data.cis_occupant.presence_validity = 1u;
            can.data.cis_occupant.count_validity = 1u;
            break;
        case 6u:
            BASE(cis_rear);
            can.data.cis_rear.rear_distance_deci_cm = 123u;
            can.data.cis_rear.validity = 1u;
            break;
        case 7u: BASE(cis_status); break;
        case 8u: BASE(vss_status); can.data.vss_status.last_fault_quality = 3u; break;
        default: CHECK(false);
        }
#undef BASE
        CHECK(net_upper_can_encode(PROFILE, &can, &event.can) == NET_WIRE_OK);
        event.kind = NET_UPPER_CAN_RX; event.link = NET_WIRE_CAN;
        event.capture.port_epoch = 1u; event.capture.timestamp_known = true;
        event.capture.captured_at_ms = e.port.now;
        CHECK(push(&e.port, &event) == 0 && service_drain(&e) == 0);
    }
    for (i = 0u; i <= (unsigned)NET_QUERY_DIGITAL_STATE; ++i)
        add_plan(&e, (NetUpperQueryGetter)i, 0u, 1u);
    for (i = 1u; i <= 8u; ++i) add_plan(&e, NET_QUERY_WARNING, (uint8_t)i, 1u);
    add_plan(&e, NET_QUERY_RECENT_RESULTS, 0u, 5u);
    add_plan(&e, NET_QUERY_DIGITAL_RESULT, 0u, 1u);
    CHECK(e.owner.plan.count == 24u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 3u, false) == 0);
    for (turns = 0u; turns < 80u && !e.query.run.finished; ++turns) {
        CHECK(run_steps(&e, 1u) == 0);
        while (acknowledged < e.port.attempts) {
            CHECK(tx_event(&e, acknowledged, true, true, NET_TX_SUCCEEDED, true) == 0);
            ++acknowledged;
        }
        CHECK(service_drain(&e) == 0);
    }
    CHECK(e.query.run.finished && e.query.run.end_succeeded);
    CHECK(e.query.run.failed_getter_count == 2u && e.owner.window_calls == 0u);
    CHECK(e.query.run.final_success_count == 26u && e.query.run.end.query_status == 2u);
    CHECK(e.query.run.end.item_count == 26u && e.port.attempts == 27u);
    for (i = 0u; i < 26u; ++i) {
        NetUpperUartMessage response;
        CHECK(decode_attempt(&e, i, &response) == 0);
        CHECK(response.sequence == 100u + i);
        if (!schema_seen[response.type]) { schema_seen[response.type] = true; ++unique; }
    }
    CHECK(unique == 15u && !schema_seen[0x38u] && !schema_seen[0x39u]);
    CHECK(e.owner.recent_capacity == 5u && e.owner.retained == 8u);
    trace("scope_all_15_active_schemas_window2_hold_actual_can_getters", &e);
    return 0;
}

static int specific_result(void)
{
    Env e;
    NetUpperUartMessage response;
    CHECK(setup(&e) == 0);
    add_plan(&e, NET_QUERY_REQUEST_RESULT, 0u, 1u);
    CHECK(send_context(&e, UINT64_C(0xbb)) == 0 && send_query(&e, 1u, true) == 0);
    CHECK(run_steps(&e, 3u) == 0 && e.port.attempts == 1u);
    CHECK(decode_attempt(&e, 0u, &response) == 0);
    CHECK(response.type == 0x20u && response.data.m_result.request_session == UINT64_C(0xaa));
    CHECK(response.data.m_result.request_id == 71u && response.data.m_result.session_id == UINT64_C(0xbb));
    CHECK(response.data.m_result.result == UINT8_MAX && response.data.m_result.result_confirmation == 0u);
    CHECK(finish(&e, 0u, NET_TX_SUCCEEDED) == 0);
    CHECK(e.query.run.end.query_status == 0u && e.query.run.end.item_count == 1u);
    trace("specific_old_request_new_recipient_unconfirmed_result", &e);
    return 0;
}

typedef int (*TestFn)(void);
static const struct { const char *name; TestFn fn; } tests[] = {
    {"recent5", recent_five}, {"capacity3", recent_three},
    {"getter_failure", poisoned_failure}, {"getter_overflow", overflow_count},
    {"final_count", ten_eight_final}, {"final_release", final_before_release},
    {"empty_absence", empty_not_found}, {"reentry", reentrant_getter},
    {"window_hold", window_local_hold}, {"plan_hold", plan_hold},
    {"session_change", session_change}, {"rear_projection", rear_projection},
    {"passive_revocation", passive_revocation}, {"reset_hold", reset_hold},
    {"queue_pressure", queue_pressure}, {"end_sequence", consecutive_end_sequence},
    {"all_getters", all_selected_getters}, {"specific_result", specific_result}
};
int main(int argc, char **argv)
{
    size_t i;
    if (argc != 2) return 2;
    for (i = 0u; i < sizeof(tests) / sizeof(tests[0]); ++i)
        if (strcmp(argv[1], tests[i].name) == 0) return tests[i].fn();
    return 2;
}
