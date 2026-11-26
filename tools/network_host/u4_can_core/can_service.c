#include "can_core_private.h"

CanCoreStatus CanSvc_Init(CanService *self, const CanSvcConfig *config, CanTransport *transport)
{
    if (self == NULL || config == NULL || transport == NULL || can_overlap(self,sizeof(*self),config,sizeof(*config)) || can_overlap(self,sizeof(*self),transport,sizeof(*transport))) { CAN_BRANCH("SI01"); /* */
        return CAN_INVALID_ARG;
    }
    if (self->guard != CAN_SVC_IDLE) return CAN_REENTRANT;
    if (transport->initialized && transport->port && (transport->owner_guard || transport->port->owner_active)) return CAN_REENTRANT;
    CAN_BRANCH("SI02"); /* */
    if (self->initialized) {
        return CAN_NOT_READY; /* self는 caller가 zero-init한 전체 객체라는 전제 */
    }
    if (config->local_boot == 0 || config->pending_capacity < 1 ||
        config->pending_capacity > 2 || (config->tx_mask & ~3u) != 0 ||
        (config->rx_mask & ~3u) != 0 || config->max_observation_gap_ms == 0 ||
        config->max_observation_gap_ms >= UINT32_C(0x80000000) ||
        config->source_horizon_ms[0] == 0 || config->source_horizon_ms[0] >= UINT32_C(0x80000000) ||
        config->source_horizon_ms[1] == 0 || config->source_horizon_ms[1] >= UINT32_C(0x80000000) ||
        config->external_horizon_proven != 1) { CAN_BRANCH("SI03"); /* */
        return CAN_INVALID_ARG;
    }
    size_t required = sizeof(CanService);
    size_t scratch_sizes[5] = { sizeof(CanService), sizeof(CanRawMessage), sizeof(CanRxRecord),
                                sizeof(CanTxOutcome), sizeof(CanSvcRxView) };
    for (size_t i = 0; i < 5; ++i) { CAN_BRANCH("SI04"); /* */
        if (SIZE_MAX - required < scratch_sizes[i]) return CAN_INVALID_ARG;
        required += scratch_sizes[i];
    }
    if (config->memory_budget_bytes < required) return CAN_INVALID_ARG;
    if (!transport->initialized || !transport->port || !transport->port->initialized) { CAN_BRANCH("SI05"); /* */
        return CAN_NOT_READY;
    }
    CanService candidate = {0};
    CAN_BRANCH("SI06"); /* */
    candidate.config = *config;
    candidate.transport = transport;
    candidate.tx_next[CAN_REAR] = 1;
    candidate.tx_next[CAN_PERMISSION] = 1;
    candidate.guard = CAN_SVC_IDLE;
    candidate.last_diagnostic = CAN_EMPTY;
    candidate.initialized = 1;
    *self = candidate; /* owner 직렬 문맥의 전체 commit; 실패했던 원 self는 건드리지 않음 */
    return CAN_OK;
}

CanCoreStatus CanSvc_Submit(CanService *self, const CanRawMessage *input,
                           CanClockStamp age_accounted_at, CanClockStamp now)
{
    if (self == NULL || input == NULL || now.valid > 1 || age_accounted_at.valid > 1 || can_overlap(self,sizeof(*self),input,sizeof(*input)))
        return CAN_INVALID_ARG;
    CAN_BRANCH("SS01"); /* */
    if (!self->initialized) return CAN_NOT_READY;
    if (can_transport_alias(self->transport,input,sizeof(*input)) || can_port_alias(self->transport->port,input,sizeof(*input))) return CAN_INVALID_ARG;
    if (self->guard != CAN_SVC_IDLE) return CAN_REENTRANT;
    if (input->kind != CAN_REAR && input->kind != CAN_PERMISSION) { CAN_BRANCH("SS02"); /* */
        return CAN_UNSUPPORTED; /* future discrete를 접수하지 않아 원 identity는 caller 소유 */
    }
    uint8_t k = (uint8_t)input->kind;
    uint32_t boot = k == CAN_REAR ? input->fields.rear.boot_id : input->fields.permission.boot_id;
    uint32_t update = k == CAN_REAR ? input->fields.rear.update_sequence : input->fields.permission.update_sequence;
    uint16_t raw_age = k == CAN_REAR ? input->fields.rear.source_age_ms : input->fields.permission.source_age_ms;
    if ((self->config.tx_mask & (1u << k)) == 0 || (boot != 0 && boot != self->config.local_boot)) { CAN_BRANCH("SS03"); /* */
        return CAN_NOT_READY;
    }
    if (k == CAN_REAR) { CAN_BRANCH("SS04"); /*: I1의 실제 raw code 검사만 */
        if (boot == 0 || input->fields.rear.validity > 1 ||
            input->fields.rear.quality_reason > 9 || input->fields.rear.proximity_status > 5)
            return CAN_CODEC_REJECT;
    } else {
        const i1_domain_permission *p = &input->fields.permission;
        if (boot == 0 || !(p->vehicle_power_permission == 0 || p->vehicle_power_permission == 1 || p->vehicle_power_permission == 255) ||
            !(p->power_operation_state == 0 || p->power_operation_state == 1 || p->power_operation_state == 255) ||
            p->power_permission_quality > 3 || p->operation_allowed > 1 || p->operation_quality > 3)
            return CAN_CODEC_REJECT;
    }
    uint32_t owner_gap = (uint32_t)(now.ms - self->last_owner_now.ms);
    uint8_t owner_clock_known = now.valid == 1 && self->config.external_horizon_proven == 1 &&
        (!self->last_owner_now.valid || (now.epoch == self->last_owner_now.epoch &&
         owner_gap < UINT32_C(0x80000000) && owner_gap <= self->config.max_observation_gap_ms));
    if (!owner_clock_known) {
        for (uint8_t c = 0; c < 2; ++c) {
            self->pending[c].age_anchor.valid = 0;
            self->pending[c].age_state = CAN_AGE_UNCERTAIN;
            self->tx_origin[c].age_anchor.valid = 0;
            self->tx_origin[c].age_state = CAN_AGE_UNCERTAIN;
            self->rx_origin[c].last.age_anchor.valid = 0;
            self->rx_origin[c].last.age_state = CAN_AGE_UNCERTAIN;
        }
    }
    self->last_owner_now = now;
    uint32_t elapsed = (uint32_t)(now.ms - age_accounted_at.ms);
    if (!owner_clock_known || age_accounted_at.valid != 1 || now.valid != 1 || age_accounted_at.epoch != now.epoch ||
        elapsed >= UINT32_C(0x80000000)) { CAN_BRANCH("SS05"); /* */
        return CAN_CLOCK_UNKNOWN;
    }
    uint64_t sum = (uint64_t)raw_age + elapsed;
    uint16_t age = sum >= UINT16_MAX ? UINT16_MAX : (uint16_t)sum;
    CanAgeState age_state = age == UINT16_MAX ? CAN_AGE_SATURATED : CAN_AGE_LOWER_BOUND;
    CanSvcTxOrigin *old = &self->tx_origin[k];
    uint8_t same = 0;
    if (old->has_observation) { CAN_BRANCH("SS06"); /* */
        uint32_t previous = k == CAN_REAR ? old->original.fields.rear.update_sequence : old->original.fields.permission.update_sequence;
        uint32_t d = (uint32_t)(update - previous);
        uint32_t gap = (uint32_t)(now.ms - old->last_observed_at.ms);
        if (old->last_observed_at.valid != 1 || now.valid != 1 || old->last_observed_at.epoch != now.epoch ||
            gap >= UINT32_C(0x80000000) || gap > self->config.source_horizon_ms[k] ||
            self->config.external_horizon_proven != 1) return CAN_CLOCK_UNKNOWN;
        if (d >= UINT32_C(0x80000000)) return CAN_ORDER_REJECT;
        same = d == 0;
    }
    if (same) { CAN_BRANCH("SS07"); /* */
        uint8_t equal;
        if (k == CAN_REAR) {
            const i1_cis_rear *a = &input->fields.rear, *b = &old->original.fields.rear;
            equal = a->rear_distance_deci_cm == b->rear_distance_deci_cm && a->validity == b->validity &&
                    a->quality_reason == b->quality_reason && a->proximity_status == b->proximity_status;
        } else {
            const i1_domain_permission *a = &input->fields.permission, *b = &old->original.fields.permission;
            equal = a->vehicle_power_permission == b->vehicle_power_permission &&
                    a->power_permission_quality == b->power_permission_quality &&
                    a->power_operation_state == b->power_operation_state &&
                    a->operation_allowed == b->operation_allowed && a->operation_quality == b->operation_quality;
        }
        if (!equal) {
            if (self->counters.origin_conflicts != UINT32_MAX) ++self->counters.origin_conflicts;
            return CAN_ORIGIN_CONFLICT;
        }
    }
    if (same) { CAN_BRANCH("SS08"); /* */
        if (old->age_anchor.valid != 1 || old->age_anchor.epoch != now.epoch ||
            old->age_state == CAN_AGE_UNCERTAIN ||
            (uint32_t)(now.ms - old->age_anchor.ms) >= UINT32_C(0x80000000)) return CAN_CLOCK_UNKNOWN;
        uint32_t gap = (uint32_t)(now.ms - old->age_anchor.ms);
        uint64_t retained_sum = (uint64_t)old->age_at_anchor + gap;
        uint16_t retained = retained_sum >= UINT16_MAX ? UINT16_MAX : (uint16_t)retained_sum;
        if (retained > age) age = retained;
        age_state = age == UINT16_MAX ? CAN_AGE_SATURATED : CAN_AGE_LOWER_BOUND;
    }
    int selected = -1, free_slot = -1;
    for (uint8_t i = 0; i < self->config.pending_capacity; ++i) { CAN_BRANCH("SS09"); /* */
        if (self->pending[i].occupied && self->pending[i].original.kind == input->kind) selected = i;
        if (!self->pending[i].occupied && free_slot < 0) free_slot = i;
    }
    if (selected < 0) selected = free_slot;
    if (selected < 0) {
        if (self->counters.full != UINT32_MAX) ++self->counters.full;
        return CAN_NO_CAPACITY;
    }
    CanSvcPending candidate = {0};
    CAN_BRANCH("SS10"); /* */
    candidate.original = *input; /* input TX는 보관된 원 사본의 값일 뿐 실제 Wire TX로 쓰지 않음 */
    candidate.age_anchor = now;
    candidate.age_at_anchor = age;
    candidate.age_state = age_state;
    candidate.occupied = 1;
    CanSvcTxOrigin watermark = {0};
    watermark.has_observation = 1;
    watermark.original = *input;
    watermark.last_observed_at = now;
    watermark.age_anchor = now;
    watermark.age_at_anchor = age;
    watermark.age_state = age_state;
    uint8_t replaced = self->pending[selected].occupied;
    self->pending[selected] = candidate;
    self->tx_origin[k] = watermark; /* 두 writes 사이 관측자가 없는 owner의 하나의 논리 commit */
    if (replaced && self->counters.replaced != UINT32_MAX) ++self->counters.replaced;
    self->last_diagnostic = CAN_ACCEPTED;
    return CAN_ACCEPTED; /* stable Service admission, Port accepted 이전 */
}

CanCoreStatus CanSvc_Pump(CanService *self, CanClockStamp now, uint16_t budget)
{
    if (self == NULL || budget < 3 || now.valid > 1) return CAN_INVALID_ARG;
    CAN_BRANCH("SP01"); /* */
    if (!self->initialized) return CAN_NOT_READY;
    if (self->guard != CAN_SVC_IDLE) return CAN_REENTRANT;
    self->guard = CAN_SVC_PUMP;
    CAN_BRANCH("SP02"); /* */
    self->dispatch_permit = 0;
    uint32_t owner_gap = (uint32_t)(now.ms - self->last_owner_now.ms);
    uint8_t owner_clock_known = now.valid == 1 && self->config.external_horizon_proven == 1 &&
        (!self->last_owner_now.valid || (now.epoch == self->last_owner_now.epoch &&
         owner_gap < UINT32_C(0x80000000) && owner_gap <= self->config.max_observation_gap_ms));
    if (!owner_clock_known) {
        for (uint8_t c = 0; c < 2; ++c) {
            self->pending[c].age_anchor.valid = 0;
            self->pending[c].age_state = CAN_AGE_UNCERTAIN;
            self->tx_origin[c].age_anchor.valid = 0;
            self->tx_origin[c].age_state = CAN_AGE_UNCERTAIN;
            self->rx_origin[c].last.age_anchor.valid = 0;
            self->rx_origin[c].last.age_state = CAN_AGE_UNCERTAIN;
        }
    }
    self->last_owner_now = now;
    uint16_t tx_quota = budget / 3 < 2 ? budget / 3 : 2;
    uint16_t outcome_quota = (budget - tx_quota) / 2;
    uint16_t rx_quota = budget - tx_quota - outcome_quota;
    uint8_t progress = 0, accepted_any = 0;
    CanCoreStatus last_error = CAN_EMPTY;
    for (uint16_t n = 0; n < outcome_quota; ++n) {
        CanTxOutcome outcome;
        CanCoreStatus result = CanTransport_TakeOutcome(self->transport, &outcome);
        if (result == CAN_OK) { CAN_BRANCH("SP03"); /* */
            uint8_t k = (uint8_t)outcome.kind; /* Transport는 검증된 두 kind outcome만 제공 */
            self->last_outcome[k] = outcome;
            self->has_outcome[k] = 1;
            if (outcome.result == CAN_TX_COMPLETE && self->counters.complete != UINT32_MAX) ++self->counters.complete;
            if (outcome.result == CAN_TX_CONFIRMED_FAIL && self->counters.confirmed_fail != UINT32_MAX) ++self->counters.confirmed_fail;
            if (outcome.result == CAN_TX_UNKNOWN && self->counters.unknown != UINT32_MAX) ++self->counters.unknown;
            progress = 1; /* 원 token/source_update 그대로. 현재 pending로 소급 적용 없음 */
        } else { CAN_BRANCH("SP04"); /* */
            if (result != CAN_EMPTY) last_error = result;
            break;
        }
    }
    for (uint16_t n = 0; n < rx_quota; ++n) {
        CanRxRecord rx;
        CanCoreStatus result = CanTransport_TakeRx(self->transport, &rx);
        if (result == CAN_OK) { CAN_BRANCH("SP05"); /* */
            CanRawMessage decoded;
            CanCodecResult codec;
            if (rx.controller_error != 0) {
                codec.status = CAN_PROFILE_REJECT;
                codec.i1 = I1_OK;
                codec.i1_called = 0;
            } else {
                codec = CanCodec_Decode(&rx.frame, &decoded);
            }
            progress = 1;
            if (codec.status != CAN_OK) { CAN_BRANCH("SP06"); /* */
                self->last_codec_result = codec;
                self->last_rejected_rx = rx;
                self->has_codec_reject = 1;
                if (self->counters.rejected != UINT32_MAX) ++self->counters.rejected;
                last_error = codec.status;
                continue; /* 원 수신의 정상 callback/liveness를 만들지 않음 */
            }
            self->dispatch_permit = 1;
    CAN_BRANCH("SP07"); /* */
            CanCoreStatus delivered = CanSvc_OnDecoded(self, &decoded, &rx, now);
            self->dispatch_permit = 0;
            if (delivered != CAN_OK) last_error = delivered;
        } else { CAN_BRANCH("SP08"); /* */
            if (result != CAN_EMPTY) last_error = result;
            break;
        }
    }
    if (!self->transport->health.ready || self->transport->health.bus_off ||
        self->transport->health.critical_loss) { CAN_BRANCH("SP09"); /* */
        last_error = CAN_NOT_READY;
        tx_quota = 0; /* pending/원 AGE anchor 보존; accepted 상태 처리는 Transport/Port */
    }
    uint16_t attempts = 0;
    for (uint8_t visited = 0; visited < 2 && attempts < tx_quota; ++visited) {
        uint8_t k = self->fairness_cursor;
        self->fairness_cursor = (uint8_t)((self->fairness_cursor + 1u) % 2u);
        int selected = -1;
        for (uint8_t i = 0; i < self->config.pending_capacity; ++i)
            if (self->pending[i].occupied && (uint8_t)self->pending[i].original.kind == k) selected = i;
        if (selected < 0) continue;
        ++attempts;
        CanSvcPending *pending = &self->pending[selected];
        uint32_t elapsed = (uint32_t)(now.ms - pending->age_anchor.ms);
        if (!owner_clock_known || now.valid != 1 || pending->age_anchor.valid != 1 ||
            pending->age_state == CAN_AGE_UNCERTAIN || now.epoch != pending->age_anchor.epoch ||
            elapsed >= UINT32_C(0x80000000)) { CAN_BRANCH("SP10"); /* */
            pending->age_state = CAN_AGE_UNCERTAIN;
            last_error = CAN_CLOCK_UNKNOWN;
            continue; /* source_age 값을 0/65535로 새로 만들지 않는다 */
        }
        uint64_t total = (uint64_t)pending->age_at_anchor + elapsed;
        uint16_t age = total >= UINT16_MAX ? UINT16_MAX : (uint16_t)total;
        CanRawMessage scratch = pending->original;
    CAN_BRANCH("SP11"); /* */
        if (k == CAN_REAR) {
            scratch.fields.rear.boot_id = self->config.local_boot;
            scratch.fields.rear.tx_sequence = self->tx_next[k];
            scratch.fields.rear.source_age_ms = age;
        } else {
            scratch.fields.permission.boot_id = self->config.local_boot;
            scratch.fields.permission.tx_sequence = self->tx_next[k];
            scratch.fields.permission.source_age_ms = age;
        }
        CanTxToken token; /* OUT은 CAN_ACCEPTED일 때만 초기화한다. */
        CanCoreStatus result = CanTransport_TryTx(self->transport, &scratch, now, &token);
        if (result == CAN_ACCEPTED) {
            pending->occupied = 0;
            self->tx_next[k] = (uint32_t)(self->tx_next[k] + UINT32_C(1));
            accepted_any = 1;
        } else {
            /* Transport 예약은 rollback했다. caller와 무관한 intent는 Service에 남는다. */
            pending->age_anchor = now;
            pending->age_at_anchor = age;
            pending->age_state = age == UINT16_MAX ? CAN_AGE_SATURATED : CAN_AGE_LOWER_BOUND;
            last_error = result;
            if (result == CAN_BUSY && self->counters.busy != UINT32_MAX) ++self->counters.busy;
            if (result == CAN_NO_CAPACITY && self->counters.full != UINT32_MAX) ++self->counters.full;
            if (result == CAN_NOT_READY && self->counters.not_ready != UINT32_MAX) ++self->counters.not_ready;
        }
    }
    self->dispatch_permit = 0;
    CAN_BRANCH("SP12"); /* */
    self->guard = CAN_SVC_IDLE;
    if (last_error != CAN_EMPTY) self->last_diagnostic = last_error;
    if (accepted_any) return CAN_ACCEPTED; /* 이 Pump의 실제 Port 수용. peer RX/App 완료는 비증명 */
    if (progress) return CAN_OK;
    return last_error; /* EMPTY 또는 실제 BUSY/NO_CAPACITY/NOT_READY/CLOCK_UNKNOWN */
}

CanCoreStatus CanSvc_OnDecoded(CanService *self, const CanRawMessage *decoded,
                              const CanRxRecord *rx, CanClockStamp now)
{
    if (self == NULL || decoded == NULL || rx == NULL || now.valid > 1 || rx->captured_at.valid > 1)
        return CAN_INVALID_ARG;
    CAN_BRANCH("SR01"); /* */
    if (!self->initialized) return CAN_NOT_READY;
    CanSvcGuard prior = self->guard;
    if (prior != CAN_SVC_IDLE && !(prior == CAN_SVC_PUMP && self->dispatch_permit == 1))
        return CAN_REENTRANT;
    if (self->transport->owner_guard != 0 || self->transport->port->owner_active != 0)
        return CAN_REENTRANT; /* lower API가 진행 중이면 callback/진단 commit까지 들어가지 않음 */
    self->dispatch_permit = 0; /* callback 전에 단회 permit 소비 */
    self->guard = CAN_SVC_RECEIVE;
    if (decoded->kind != CAN_REAR && decoded->kind != CAN_PERMISSION) { CAN_BRANCH("SR02"); /* */
        self->guard = prior;
        return CAN_UNSUPPORTED;
    }
    uint8_t k = (uint8_t)decoded->kind;
    if ((self->config.rx_mask & (1u << k)) == 0 || rx->controller_error != 0 ||
        rx->backend_generation != self->transport->health.generation ||
        !self->transport->health.ready || self->transport->health.bus_off ||
        self->transport->health.critical_loss ||
        rx->frame.can_id != (k == CAN_REAR ? I1_CIS_REAR_ID : I1_DOMAIN_PERMISSION_ID) ||
        rx->frame.ide != 0 || rx->frame.fdf != 1 || rx->frame.brs != 1 ||
        rx->frame.rtr != 0 || rx->frame.dlc != I1_CAN_DLC || rx->frame.length != I1_CAN_BYTES) {
        self->guard = prior;
        return CAN_NOT_READY; /* Codec 이전 Transport 격리와 별개인 entry 방어 */
    }
    /* Private input pair validation uses the existing serializer, never a second Wire codec. */
    CanFrame pair;
    CanCodecResult pairing=CanCodec_Encode(decoded,&pair);
    if (pairing.status!=CAN_OK || memcmp(pair.bytes,rx->frame.bytes,20)!=0) {
        self->guard=prior;return CAN_INVALID_ARG;
    }
    CanSvcRxContext *source = &self->rx_origin[k];
    uint32_t boot = k == CAN_REAR ? decoded->fields.rear.boot_id : decoded->fields.permission.boot_id;
    uint32_t tx = k == CAN_REAR ? decoded->fields.rear.tx_sequence : decoded->fields.permission.tx_sequence;
    uint32_t update = k == CAN_REAR ? decoded->fields.rear.update_sequence : decoded->fields.permission.update_sequence;
    uint16_t wire_age = k == CAN_REAR ? decoded->fields.rear.source_age_ms : decoded->fields.permission.source_age_ms;
    if (!source->bound || boot != source->admitted_boot) { CAN_BRANCH("SR03"); /* */
        self->guard = prior;
        return CAN_NOT_READY;
    }
    CanSvcRxView candidate = {0};
    candidate.original = *decoded; /* 원 I1 AGE/BOOT/TX 값은 그대로 복사한다. */
    candidate.received_at = rx->captured_at;
    candidate.age_anchor = now;
    candidate.owner_context = source->owner_context;
    uint32_t owner_gap = (uint32_t)(now.ms - self->last_owner_now.ms);
    uint8_t owner_clock_known = now.valid == 1 && self->config.external_horizon_proven == 1 &&
        (!self->last_owner_now.valid || (now.epoch == self->last_owner_now.epoch &&
         owner_gap < UINT32_C(0x80000000) && owner_gap <= self->config.max_observation_gap_ms));
    if (!owner_clock_known) {
        for (uint8_t c = 0; c < 2; ++c) {
            self->pending[c].age_anchor.valid = 0;
            self->pending[c].age_state = CAN_AGE_UNCERTAIN;
            self->tx_origin[c].age_anchor.valid = 0;
            self->tx_origin[c].age_state = CAN_AGE_UNCERTAIN;
            self->rx_origin[c].last.age_anchor.valid = 0;
            self->rx_origin[c].last.age_state = CAN_AGE_UNCERTAIN;
        }
    }
    self->last_owner_now = now;
    uint32_t wait = (uint32_t)(now.ms - rx->captured_at.ms);
    uint8_t time_known = owner_clock_known && now.valid == 1 && rx->captured_at.valid == 1 &&
                         now.epoch == rx->captured_at.epoch && wait < UINT32_C(0x80000000);
    if (!time_known) { CAN_BRANCH("SR04"); /* */
        candidate.effective_age_ms = wire_age;
        candidate.age_state = wire_age == UINT16_MAX ? CAN_AGE_SATURATED : CAN_AGE_UNCERTAIN;
    } else {
        uint64_t sum = (uint64_t)wire_age + wait;
        candidate.effective_age_ms = sum >= UINT16_MAX ? UINT16_MAX : (uint16_t)sum;
        candidate.age_state = candidate.effective_age_ms == UINT16_MAX ? CAN_AGE_SATURATED : CAN_AGE_LOWER_BOUND;
        /* Port accepted→실TX/홉 미계상 구간은 여전히 하한이며 fresh 상한을 증명하지 않는다. */
    }
    uint8_t same_update = 0;
    if (source->has_record) { CAN_BRANCH("SR05"); /* */
        uint32_t gap = (uint32_t)(now.ms - source->last_valid_frame.ms);
        if (now.valid != 1 || source->last_valid_frame.valid != 1 || now.epoch != source->last_valid_frame.epoch ||
            gap >= UINT32_C(0x80000000) || gap > self->config.source_horizon_ms[k] ||
            self->config.external_horizon_proven != 1) {
            self->guard = prior;
            return CAN_CLOCK_UNKNOWN;
        }
        uint32_t previous_tx = k == CAN_REAR ? source->last.original.fields.rear.tx_sequence : source->last.original.fields.permission.tx_sequence;
        uint32_t tx_difference = (uint32_t)(tx - previous_tx);
        if (tx_difference == 0 || tx_difference >= UINT32_C(0x80000000)) { CAN_BRANCH("SR06"); /* */
            if (self->counters.order_rejects != UINT32_MAX) ++self->counters.order_rejects;
            self->guard = prior;
            return CAN_ORDER_REJECT;
        }
        uint32_t previous_update = k == CAN_REAR ? source->last.original.fields.rear.update_sequence : source->last.original.fields.permission.update_sequence;
        uint32_t update_difference = (uint32_t)(update - previous_update);
        if (update_difference >= UINT32_C(0x80000000)) { CAN_BRANCH("SR07"); /* */
            if (self->counters.order_rejects != UINT32_MAX) ++self->counters.order_rejects;
            self->guard = prior;
            return CAN_ORDER_REJECT;
        }
        same_update = update_difference == 0;
    }
    if (same_update) { CAN_BRANCH("SR08"); /* */
        uint8_t equal;
        if (k == CAN_REAR) {
            const i1_cis_rear *a = &decoded->fields.rear, *b = &source->last.original.fields.rear;
            equal = a->rear_distance_deci_cm == b->rear_distance_deci_cm && a->validity == b->validity &&
                    a->quality_reason == b->quality_reason && a->proximity_status == b->proximity_status;
        } else {
            const i1_domain_permission *a = &decoded->fields.permission, *b = &source->last.original.fields.permission;
            equal = a->vehicle_power_permission == b->vehicle_power_permission &&
                    a->power_permission_quality == b->power_permission_quality &&
                    a->power_operation_state == b->power_operation_state &&
                    a->operation_allowed == b->operation_allowed && a->operation_quality == b->operation_quality;
        }
        if (!equal) {
            if (self->counters.origin_conflicts != UINT32_MAX) ++self->counters.origin_conflicts;
            self->guard = prior;
            return CAN_ORIGIN_CONFLICT;
        }
    }
    if (same_update) { CAN_BRANCH("SR09"); /* */
        uint32_t elapsed = (uint32_t)(now.ms - source->last.age_anchor.ms);
        uint8_t retained_time_known = owner_clock_known && now.valid == 1 && source->last.age_anchor.valid == 1 &&
            now.epoch == source->last.age_anchor.epoch && elapsed < UINT32_C(0x80000000);
        uint16_t floor = source->last.effective_age_ms;
        if (retained_time_known) {
            uint64_t sum = (uint64_t)floor + elapsed;
            floor = sum >= UINT16_MAX ? UINT16_MAX : (uint16_t)sum;
        } else {
            candidate.age_state = CAN_AGE_UNCERTAIN; /* 부족한 경과를 0으로 확정하지 않고 미확인 메타 유지 */
        }
        if (floor > candidate.effective_age_ms) candidate.effective_age_ms = floor;
        if (source->last.age_state == CAN_AGE_UNCERTAIN) candidate.age_state = CAN_AGE_UNCERTAIN;
        if (candidate.effective_age_ms == UINT16_MAX) candidate.age_state = CAN_AGE_SATURATED;
        candidate.new_observation = 0;
    } else { CAN_BRANCH("SR10"); /* */
        candidate.new_observation = 1; /* 첫/새 UPDATE. 원 FAULT·NO_OBJECT·품질은 그대로 */
    }
    source->last = candidate;
    CAN_BRANCH("SR11"); /*: 모든 검증 뒤 한owner commit */
    source->has_record = 1;
    source->last_valid_frame = rx->captured_at;
    if (candidate.new_observation) source->last_new_update = rx->captured_at;
    if (self->config.diagnostic_sink != NULL) { CAN_BRANCH("SR12"); /* */
        uint8_t saved_transport_guard = self->transport->owner_guard;
    CAN_BRANCH("SR01"); /*에서 IDLE=0 확인 */
        uint8_t saved_port_guard = self->transport->port->owner_active;
    CAN_BRANCH("SR01"); /*에서 0 확인 */
        self->guard = CAN_SVC_CALLBACK;
        self->transport->owner_guard = 3; /* 같은 owner의 CALLBACK 구간을 T에도 전파 */
        self->transport->port->owner_active = 1; /* P 직접 API 재진입도 거부 */
        self->config.diagnostic_sink(self->config.sink_context, &candidate);
        self->transport->owner_guard = saved_transport_guard;
        self->transport->port->owner_active = saved_port_guard;
        /* 반환 뒤 &candidate는 무효. consumer 보관은 전체 복사; void는 App 수용/DONE이 아님 */
        if (self->counters.delivered != UINT32_MAX) ++self->counters.delivered;
    }
    self->guard = prior;
    return CAN_OK; /* Service 진단 record commit만. C2 투영/Permission 정책 성공은 비증명 */
}

CanCoreStatus CanSvc_BindSource(CanService *self, CanMsgKind kind,
                              uint32_t admitted_boot, uint64_t owner_context)
{
    if (self == NULL) return CAN_INVALID_ARG;
    CAN_BRANCH("SB01"); /* */
    if (!self->initialized) return CAN_NOT_READY;
    if (self->guard != CAN_SVC_IDLE) return CAN_REENTRANT;
    if (kind != CAN_REAR && kind != CAN_PERMISSION) return CAN_UNSUPPORTED;
    CAN_BRANCH("SB02"); /* */
    uint8_t k = (uint8_t)kind;
    if ((self->config.rx_mask & (1u << k)) == 0 || admitted_boot == 0 || owner_context == 0)
        return CAN_INVALID_ARG;
    CanSvcRxContext *old = &self->rx_origin[k];
    if (old->bound) { CAN_BRANCH("SB03"); /* */
        if (owner_context == old->owner_context)
            return admitted_boot == old->admitted_boot ? CAN_OK : CAN_ORIGIN_CONFLICT;
        if (owner_context < old->owner_context) return CAN_ORDER_REJECT;
        /* UINT64_MAX 다음 새 context는 표현할 수 없다. 0으로 wrap하지 않는다. */
    }
    CanSvcRxContext candidate = *old;
    if (old->bound && old->admitted_boot == admitted_boot) { CAN_BRANCH("SB04"); /* */
        candidate.owner_context = owner_context;
        *old = candidate; /* 원 source 기록/순서/AGE/관측 시각은 그대로 */
        return CAN_OK;
    }
    candidate.bound = 1;
    CAN_BRANCH("SB05"); /* */
    candidate.admitted_boot = admitted_boot;
    candidate.owner_context = owner_context;
    candidate.has_record = 0;
    candidate.last_valid_frame.valid = 0;
    candidate.last_new_update.valid = 0;
    *old = candidate; /* 과거 last raw는 비활성 진단; 다른 kind/TX/pending/inflight는 변경 없음 */
    return CAN_OK;
}
