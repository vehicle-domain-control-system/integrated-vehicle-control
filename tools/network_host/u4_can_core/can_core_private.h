#ifndef CAN_CORE_PRIVATE_H
#define CAN_CORE_PRIVATE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../u3_raw_diag/i1/i1_host_codec.h"
/* Private Host API: caller owns truthful, nonoverlapping static backing.
 * All state APIs share one serialized owner. Actual ISR atomics are a binding duty. */
#ifdef CAN_CORE_TRACE
void CanTest_BranchHit(const char *id);
#define CAN_BRANCH(id) CanTest_BranchHit(id)
#else
#define CAN_BRANCH(id) ((void)0)
#endif
typedef struct CanLinkPort CanLinkPort;
typedef struct CanTransport CanTransport;
typedef struct CanService CanService;
typedef enum { CAN_REAR, CAN_PERMISSION } CanMsgKind;
typedef enum { CAN_OK, CAN_ACCEPTED, CAN_EMPTY, CAN_INVALID_ARG,
    CAN_NO_CAPACITY, CAN_NOT_READY, CAN_BUSY, CAN_UNSUPPORTED,
    CAN_PROFILE_REJECT, CAN_CODEC_REJECT, CAN_CLOCK_UNKNOWN,
    CAN_ORIGIN_CONFLICT, CAN_ORDER_REJECT, CAN_REENTRANT } CanCoreStatus;
typedef enum { CAN_TX_COMPLETE, CAN_TX_CONFIRMED_FAIL, CAN_TX_UNKNOWN } CanTxResult;
typedef enum { CAN_AGE_LOWER_BOUND, CAN_AGE_SATURATED, CAN_AGE_UNCERTAIN } CanAgeState;
typedef struct { uint32_t epoch; uint32_t ms; uint8_t valid; } CanClockStamp;
typedef struct {
    uint32_t can_id;
    uint8_t ide, fdf, brs, rtr, dlc, length;
    uint8_t bytes[20];
} CanFrame;
typedef struct {
    CanMsgKind kind;
    union { i1_cis_rear rear; i1_domain_permission permission; } fields;
} CanRawMessage;
typedef struct { uint64_t backend_generation; uint16_t slot; uint64_t serial; } CanTxToken;
typedef struct { CanCoreStatus status; i1_status i1; uint8_t i1_called; } CanCodecResult;
typedef struct {
    CanFrame frame;
    CanClockStamp captured_at;
    uint64_t backend_generation;
    uint32_t controller_error;
} CanRxRecord;
typedef struct {
    CanTxToken token;
    CanMsgKind kind;
    uint32_t source_update;
    CanTxResult result;
    CanCoreStatus cause;
    CanClockStamp observed_at;
} CanTxOutcome;
typedef struct { uint64_t generation; uint8_t ready; uint8_t bus_off;
    uint8_t quiescent; uint8_t critical_loss; uint32_t rx_dropped; } CanPortHealth;
typedef enum { CAN_PORT_TX_TERMINAL, CAN_PORT_RX_FRAME, CAN_PORT_HEALTH } CanPortEventKind;
typedef struct {
    CanPortEventKind kind;
    union {
        struct { CanTxToken token; CanTxResult result;
                 CanCoreStatus cause; CanClockStamp observed_at; } terminal;
        CanRxRecord rx;
        CanPortHealth health;
    } data;
} CanPortEvent;
typedef enum { CAN_RESET_REQUESTED, CAN_RESET_BUS_OFF, CAN_RESET_EVENT_LOSS } CanResetReason;
typedef struct CanHostBackend CanHostBackend;
typedef enum {
    CAN_PORT_FREE, CAN_PORT_RESERVED, CAN_PORT_ACCEPTED,
    CAN_PORT_TERMINAL_READY, CAN_PORT_UNKNOWN_PINNED
} CanPortSlotState;
typedef enum { CAN_PORT_HOST_PROFILE_ONLY, CAN_PORT_TARGET_UNBOUND } CanPortBindingKind;
typedef struct {
    CanCoreStatus status;
    uint8_t no_start_proven;
} CanPortAdmissionObservation;
typedef struct {
    uint16_t tx_slots;
    uint16_t rx_capacity;
    uint32_t max_clock_gap_ms;
    size_t memory_budget_bytes;
    CanPortBindingKind binding_kind;
    uint8_t horizon_proven;
    uint8_t fd_metadata_available;
    uint8_t initial_quiescent;
    uint8_t initial_ready;
    const CanHostBackend *backend;
} CanPortConfig;
typedef struct {
    CanPortSlotState state;
    CanFrame frame;
    CanTxToken token;
    uint64_t highest_serial;
    CanClockStamp accepted_at;
    CanTxResult terminal_result;
    CanCoreStatus terminal_cause;
    CanClockStamp terminal_at;
    uint8_t terminal_valid;
    uint8_t release_safe;
} CanPortSlot;
typedef struct {
    CanPortSlot *tx;
    CanRxRecord *rx;
    uint16_t tx_slots;
    uint16_t rx_capacity;
} CanPortStorage;
struct CanLinkPort {
    CanPortConfig config;
    CanPortStorage storage;
    CanPortHealth health;
    CanClockStamp last_progress;
    CanResetReason reset_reason;
    uint16_t rx_head, rx_tail, rx_count, terminal_cursor;
    uint32_t stale_events, duplicate_events, bad_events;
    uint8_t initialized, owner_active, health_pending, health_transition_pending;
    uint8_t reset_pending, fence_applied, recovery_ready;
};
typedef enum { CAN_T_FREE, CAN_T_RESERVED, CAN_T_INFLIGHT, CAN_T_OUTCOME_READY } CanTransportState;
typedef struct {
    CanTransportState state;
    uint64_t serial;
    CanTxToken token;
    CanFrame frame;
    CanMsgKind kind;
    uint32_t source_update;
    CanClockStamp accepted_at;
    uint8_t deadline_fenced;
    CanTxOutcome outcome;
} CanTransportSlot;
typedef struct {
    size_t tx_capacity, rx_capacity, memory_budget_bytes;
    uint32_t watchdog_ms, max_observation_gap_ms;
    uint8_t horizon_proven, generation_fresh_for_pair;
} CanTransportConfig;
typedef struct {
    CanTransportSlot *tx_slots; size_t tx_count;
    CanRxRecord *rx_records; size_t rx_count;
} CanTransportStorage;
struct CanTransport {
    CanTransportConfig config;
    CanTransportStorage storage;
    CanLinkPort *port;
    CanPortHealth health;
    size_t rx_head, rx_tail, rx_used, outcome_cursor;
    uint32_t rx_dropped, late_event_count;
    CanRxRecord last_rejected_rx;
    CanCodecResult last_codec_result;
    uint8_t rejected_rx_valid, codec_result_valid, initialized, owner_guard;
};
typedef struct {
    CanRawMessage original;
    CanClockStamp received_at;
    CanClockStamp age_anchor;
    uint16_t effective_age_ms;
    CanAgeState age_state;
    uint8_t new_observation;
    uint64_t owner_context;
} CanSvcRxView;

typedef void (*CanSvcDiagSink)(void *context, const CanSvcRxView *view);

typedef struct {
    uint32_t local_boot;
    uint8_t pending_capacity;
    uint8_t tx_mask;
    uint8_t rx_mask;
    uint32_t max_observation_gap_ms;
    uint32_t source_horizon_ms[2];
    uint8_t external_horizon_proven;
    size_t memory_budget_bytes;
    CanSvcDiagSink diagnostic_sink;
    void *sink_context;
} CanSvcConfig;

typedef struct {
    uint8_t occupied;
    CanRawMessage original;
    CanClockStamp age_anchor;
    uint16_t age_at_anchor;
    CanAgeState age_state;
} CanSvcPending;

typedef struct {
    uint8_t has_observation;
    CanRawMessage original;
    CanClockStamp last_observed_at;
    CanClockStamp age_anchor;
    uint16_t age_at_anchor;
    CanAgeState age_state;
} CanSvcTxOrigin;

typedef struct {
    uint8_t bound;
    uint32_t admitted_boot;
    uint64_t owner_context;
    uint8_t has_record;
    CanSvcRxView last;
    CanClockStamp last_valid_frame;
    CanClockStamp last_new_update;
} CanSvcRxContext;

typedef enum {
    CAN_SVC_IDLE, CAN_SVC_PUMP, CAN_SVC_RECEIVE, CAN_SVC_CALLBACK
} CanSvcGuard;

typedef struct {
    uint32_t rejected, full, replaced, busy, not_ready;
    uint32_t origin_conflicts, order_rejects, delivered;
    uint32_t complete, confirmed_fail, unknown;
} CanSvcCounters;

struct CanService {
    uint8_t initialized;
    CanSvcConfig config;
    CanTransport *transport;
    CanClockStamp last_owner_now;
    CanSvcPending pending[2];
    CanSvcTxOrigin tx_origin[2];
    CanSvcRxContext rx_origin[2];
    uint32_t tx_next[2];
    uint8_t fairness_cursor;
    CanSvcGuard guard;
    uint8_t dispatch_permit;
    uint8_t has_outcome[2];
    CanTxOutcome last_outcome[2];
    CanCoreStatus last_diagnostic;
    uint8_t has_codec_reject;
    CanCodecResult last_codec_result;
    CanRxRecord last_rejected_rx;
    CanSvcCounters counters;
};
/* Only Host binding observations, never vendor or Node interfaces. */
struct CanHostBackend {
    void *context;
    CanPortAdmissionObservation (*admit)(void *, CanLinkPort *, const CanFrame *, CanTxToken, CanClockStamp);
    uint8_t (*flush)(void *, CanLinkPort *, CanResetReason, uint64_t, CanClockStamp);
};
CanCoreStatus CanTransport_Init(CanTransport *self, const CanTransportConfig *config, CanTransportStorage *storage, CanLinkPort *port);
CanCoreStatus CanTransport_TryTx(CanTransport *self, const CanRawMessage *input, CanClockStamp now, CanTxToken *accepted);
CanCoreStatus CanTransport_Pump(CanTransport *self, CanClockStamp now, uint16_t budget);
CanCoreStatus CanTransport_TakeOutcome(CanTransport *self, CanTxOutcome *out);
CanCoreStatus CanTransport_TakeRx(CanTransport *self, CanRxRecord *out);
CanCoreStatus CanLinkPort_Init(CanLinkPort *self, const CanPortConfig *config, CanPortStorage *storage, CanClockStamp now);
CanCoreStatus CanLinkPort_TryTx(CanLinkPort *self, const CanFrame *frame, CanTxToken token, CanClockStamp now);
CanCoreStatus CanLinkPort_PollEvent(CanLinkPort *self, CanPortEvent *out, CanClockStamp now);
CanCoreStatus CanLinkPort_Reset(CanLinkPort *self, CanResetReason reason, CanClockStamp now);
CanCoreStatus CanSvc_Init(CanService *self, const CanSvcConfig *config, CanTransport *transport);
CanCoreStatus CanSvc_Submit(CanService *self, const CanRawMessage *input, CanClockStamp age_accounted_at, CanClockStamp now);
CanCoreStatus CanSvc_Pump(CanService *self, CanClockStamp now, uint16_t budget);
CanCoreStatus CanSvc_OnDecoded(CanService *self, const CanRawMessage *decoded, const CanRxRecord *rx, CanClockStamp now);
CanCoreStatus CanSvc_BindSource(CanService *self, CanMsgKind kind, uint32_t admitted_boot, uint64_t owner_context);
CanCodecResult CanCodec_Encode(const CanRawMessage *input, CanFrame *out);
CanCodecResult CanCodec_Decode(const CanFrame *input, CanRawMessage *out);
static inline uint32_t can_sat_inc(uint32_t x) { return x==UINT32_MAX ? x : x+1; }
static inline int can_token_equal(CanTxToken a, CanTxToken b) {
    return a.backend_generation==b.backend_generation && a.slot==b.slot && a.serial==b.serial;
}
/* Integer ranges avoid relational comparison of unrelated C pointers. */
static inline int can_overlap(const void *a, size_t na, const void *b, size_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if (!na || !nb) return 0;
    if (na>UINTPTR_MAX-x || nb>UINTPTR_MAX-y) return 1;
    return x<y+nb && y<x+na;
}
static inline int can_size_add(size_t *total,size_t count,size_t width) {
    if (width && count>SIZE_MAX/width) return 0;
    if (*total>SIZE_MAX-count*width) return 0;
    *total+=count*width;return 1;
}
static inline int can_clock_delta(CanClockStamp a,CanClockStamp b,uint32_t *delta) {
    if (a.valid!=1 || b.valid!=1 || a.epoch!=b.epoch) return 0;
    *delta=(uint32_t)(b.ms-a.ms);return *delta<UINT32_C(0x80000000);
}
static inline int can_port_alias(const CanLinkPort *p,const void *v,size_t n) {
    return can_overlap(p,sizeof(*p),v,n) ||
        can_overlap(p->storage.tx,(size_t)p->config.tx_slots*sizeof(CanPortSlot),v,n) ||
        can_overlap(p->storage.rx,(size_t)p->config.rx_capacity*sizeof(CanRxRecord),v,n);
}
static inline int can_transport_alias(const CanTransport *p,const void *v,size_t n) {
    return can_overlap(p,sizeof(*p),v,n) ||
        can_overlap(p->storage.tx_slots,p->config.tx_capacity*sizeof(CanTransportSlot),v,n) ||
        can_overlap(p->storage.rx_records,p->config.rx_capacity*sizeof(CanRxRecord),v,n);
}
/* Capture hooks are binding-only, serialized by the deterministic Host Mock.
 * No decode/Service/App calls occur here. Real ISR publication is not proved. */
static inline void can_host_fault(CanLinkPort *p,uint8_t bus_off,uint8_t critical) {
    p->health.ready=0;p->health.bus_off|=bus_off;p->health.critical_loss|=critical;
    if (!p->reset_pending) {p->reset_pending=1;p->fence_applied=0;}
    p->health_pending=p->health_transition_pending=1;
}
static inline CanCoreStatus can_host_terminal(CanLinkPort *p,CanTxToken token,CanTxResult result,CanCoreStatus cause,CanClockStamp now) {
    CanPortSlot *slot;
    if (!p || !p->initialized) return CAN_INVALID_ARG;
    if (token.slot>=p->config.tx_slots || (result!=CAN_TX_COMPLETE && result!=CAN_TX_CONFIRMED_FAIL && result!=CAN_TX_UNKNOWN)) {
        p->bad_events=can_sat_inc(p->bad_events);can_host_fault(p,0,1);return CAN_INVALID_ARG;
    }
    slot=&p->storage.tx[token.slot];
    if (!can_token_equal(slot->token,token) || slot->state==CAN_PORT_FREE) {
        p->stale_events=can_sat_inc(p->stale_events);return CAN_ORDER_REJECT;
    }
    if (slot->terminal_valid) {p->duplicate_events=can_sat_inc(p->duplicate_events);return CAN_EMPTY;}
    slot->terminal_result=result;slot->terminal_cause=cause;slot->terminal_at=now;
    slot->release_safe=(uint8_t)(result!=CAN_TX_UNKNOWN);slot->terminal_valid=1;
    if (slot->state!=CAN_PORT_RESERVED) slot->state=CAN_PORT_TERMINAL_READY;
    if (result==CAN_TX_UNKNOWN) can_host_fault(p,0,0);
    return CAN_OK;
}
static inline CanCoreStatus can_host_rx(CanLinkPort *p,const CanFrame *frame,CanClockStamp at,uint64_t generation,uint32_t error) {
    CanRxRecord *r;
    if (!p || !frame || !p->initialized || at.valid>1 || can_port_alias(p,frame,sizeof(*frame))) return CAN_INVALID_ARG;
    if (p->rx_count==p->config.rx_capacity) {p->health.rx_dropped=can_sat_inc(p->health.rx_dropped);p->health_pending=1;return CAN_NO_CAPACITY;}
    r=&p->storage.rx[p->rx_tail];r->frame=*frame;r->captured_at=at;r->backend_generation=generation;r->controller_error=error;
    p->rx_tail=(uint16_t)((p->rx_tail+1u)%p->config.rx_capacity);p->rx_count++;return CAN_ACCEPTED;
}
#endif
