#include "can_core_private.h"
static void can_port_request_drain(CanLinkPort *p,CanClockStamp now) {
    if (p->reset_pending && p->fence_applied && !p->health.quiescent &&
        p->config.backend->flush(p->config.backend->context,p,p->reset_reason,p->health.generation,now)==1) {
        p->health.quiescent=1;p->recovery_ready=1;
    }
}
CanCoreStatus CanLinkPort_Init(CanLinkPort *self, const CanPortConfig *config, CanPortStorage *storage, CanClockStamp now)
{
    size_t m;
    uint16_t i;
    CAN_BRANCH("PI01"); /* 모든 입력과 지속 생존 backing을 검증하며 아직 저장하지 않는다. */
    if (self == NULL || config == NULL || storage == NULL || storage->tx == NULL || storage->rx == NULL)
        return CAN_INVALID_ARG;
    if (config->tx_slots == 0 || config->rx_capacity == 0 ||
        storage->tx_slots != config->tx_slots || storage->rx_capacity != config->rx_capacity ||
        config->horizon_proven > 1 || config->fd_metadata_available > 1 || config->initial_quiescent > 1 || config->initial_ready > 1 || now.valid > 1 ||
        (config->binding_kind != CAN_PORT_HOST_PROFILE_ONLY && config->binding_kind != CAN_PORT_TARGET_UNBOUND))
        return CAN_INVALID_ARG;
    if (!config->backend || !config->backend->admit || !config->backend->flush) return CAN_INVALID_ARG;
    if (can_overlap(self,sizeof(*self),config,sizeof(*config)) || can_overlap(self,sizeof(*self),storage,sizeof(*storage)) ||
        can_overlap(storage->tx,(size_t)config->tx_slots*sizeof(CanPortSlot),storage->rx,(size_t)config->rx_capacity*sizeof(CanRxRecord)) ||
        can_overlap(self,sizeof(*self),storage->tx,(size_t)config->tx_slots*sizeof(CanPortSlot)) ||
        can_overlap(self,sizeof(*self),storage->rx,(size_t)config->rx_capacity*sizeof(CanRxRecord)) ||
        can_overlap(config,sizeof(*config),storage->tx,(size_t)config->tx_slots*sizeof(CanPortSlot)) ||
        can_overlap(config,sizeof(*config),storage->rx,(size_t)config->rx_capacity*sizeof(CanRxRecord)) ||
        can_overlap(storage,sizeof(*storage),storage->tx,(size_t)config->tx_slots*sizeof(CanPortSlot)) ||
        can_overlap(storage,sizeof(*storage),storage->rx,(size_t)config->rx_capacity*sizeof(CanRxRecord))) return CAN_INVALID_ARG;
    CAN_BRANCH("PI02"); /* target sizeof로 계산하되 모든 곱/합을 수행하기 전에 overflow를 차단한다. */
    m = sizeof(CanLinkPort);
    if (config->max_clock_gap_ms == 0 || config->max_clock_gap_ms >= UINT32_C(0x80000000)) return CAN_INVALID_ARG;
    if (config->tx_slots > (SIZE_MAX - m) / sizeof(CanPortSlot)) return CAN_INVALID_ARG;
    m += config->tx_slots * sizeof(CanPortSlot);
    if (config->rx_capacity > (SIZE_MAX - m) / sizeof(CanRxRecord)) return CAN_INVALID_ARG;
    m += config->rx_capacity * sizeof(CanRxRecord);
    if (config->memory_budget_bytes == 0 || m > config->memory_budget_bytes) return CAN_INVALID_ARG;
    CAN_BRANCH("PI03"); /* callback guard를 initialized보다 먼저 검사하고, generation1 재사용을 막는다. */
    if (self->owner_active) return CAN_REENTRANT;
    if (self->initialized || !config->initial_quiescent) return CAN_NOT_READY;
    CAN_BRANCH("PI04"); /* 이 지점부터만 성공 commit; config pointer는 보관하지 않고 backing view를 값 복사한다. */
    self->config = *config; self->storage = *storage;
    self->health = (CanPortHealth){ .generation = 1, .ready = 0, .bus_off = 0,
        .quiescent = 1, .critical_loss = 0, .rx_dropped = 0 };
    self->health.ready = config->binding_kind == CAN_PORT_HOST_PROFILE_ONLY &&
        config->initial_ready && config->fd_metadata_available && config->horizon_proven && now.valid == 1;
    for (i = 0; i < config->tx_slots; ++i) {
        self->storage.tx[i] = (CanPortSlot){0};
        self->storage.tx[i].state = CAN_PORT_FREE;
    }
    for (i = 0; i < config->rx_capacity; ++i) self->storage.rx[i] = (CanRxRecord){0};
    self->last_progress = now; self->reset_reason = CAN_RESET_REQUESTED;
    self->rx_head = self->rx_tail = self->rx_count = self->terminal_cursor = 0;
    self->stale_events = self->duplicate_events = self->bad_events = 0;
    self->owner_active = self->health_pending = self->health_transition_pending = 0;
    self->reset_pending = self->fence_applied = self->recovery_ready = 0;
    self->initialized = 1;
    return CAN_OK;
}

CanCoreStatus CanLinkPort_TryTx(CanLinkPort *self, const CanFrame *frame, CanTxToken token, CanClockStamp now)
{
    CanPortSlot *p;
    CanPortAdmissionObservation admission;
    uint16_t i;
    uint8_t free_exists = 0;
    uint32_t delta;
    CAN_BRANCH("PT01"); /* metadata/length 검사를 복사 전에 끝낸다. raw payload 의미는 기존 Codec 책임이다. */
    if (self == NULL || frame == NULL || !self->initialized || now.valid > 1 || can_port_alias(self,frame,sizeof(*frame))) return CAN_INVALID_ARG;
    if (token.slot >= self->config.tx_slots || token.serial == 0) return CAN_INVALID_ARG;
    if ((frame->can_id != UINT32_C(0x0A0) && frame->can_id != UINT32_C(0x080)) ||
        frame->ide != 0 || frame->fdf != 1 || frame->brs != 1 || frame->rtr != 0 ||
        frame->dlc != 11 || frame->length != 20) return CAN_PROFILE_REJECT;
    CAN_BRANCH("PT02"); /* callback이 이 함수를 재진입해도 owner 상태를 새로 변경하지 않는다. */
    if (self->owner_active) return CAN_REENTRANT;
    self->owner_active = 1; p = &self->storage.tx[token.slot];
    CAN_BRANCH("PT03"); /* 준비/세대/serial 한계를 확인한다. 과거 retained 사건은 그대로 둔다. */
    if (!self->health.ready || self->health.bus_off || self->health.critical_loss || self->reset_pending ||
        token.backend_generation != self->health.generation || p->highest_serial == UINT64_MAX) {
        self->owner_active = 0; return CAN_NOT_READY;
    }
    CAN_BRANCH("PT04"); /* 동일 local epoch와 증명된 제한간격에서만 u32 modular delta를 인정한다. */
    delta = (uint32_t)(now.ms - self->last_progress.ms);
    if (!self->config.horizon_proven || !now.valid || !self->last_progress.valid || now.epoch != self->last_progress.epoch ||
        delta >= UINT32_C(0x80000000) || delta > self->config.max_clock_gap_ms) {
        self->health.ready = 0;
        self->health_pending = self->health_transition_pending = 1;
        self->reset_pending = 1;
        self->owner_active = 0; return CAN_CLOCK_UNKNOWN;
    }
    CAN_BRANCH("PT05"); /* 같은 slot identity는 accepted/pinned 상태에서 절대 덮어쓰지 않는다. */
    if (token.serial <= p->highest_serial) { self->owner_active = 0; return CAN_INVALID_ARG; }
    if (p->state != CAN_PORT_FREE) {
        for (i = 0; i < self->config.tx_slots; ++i) free_exists |= self->storage.tx[i].state == CAN_PORT_FREE;
        self->owner_active = 0; return free_exists ? CAN_BUSY : CAN_NO_CAPACITY;
    }
    CAN_BRANCH("PT06"); /* reserve→독립 value copy→fulltoken publish 후 backend bounded try를 정확히 한 번 진행한다. */
    p->state = CAN_PORT_RESERVED; p->terminal_valid = 0; p->release_safe = 0;
    p->frame = *frame; p->token = token;
    admission=self->config.backend->admit(self->config.backend->context,self,&p->frame,token,now);
    /* binding 관측 지점: backend가 Port-owned p->frame과 제출 당시 token을 받는다.
     * 단 한 번의 admission 결과(status, no_start_proven)를 admission에 기록한다.
     * 동기 callback이면 이 RESERVED slot의 retained cell만 latch한다.
     * backend가 borrowed 주소를 읽을 수 있는 동안 위 frame은 바뀌지 않는다.
     * 실제 vendor/Mock 함수 구현은 U4C/U10 별도 범위다. */
    CAN_BRANCH("PT07"); /* 확실한 NO_START만 rollback한다. callback latch와 모순되면 이 경로를 쓰지 않는다. */
    if (admission.no_start_proven == 1 && !p->terminal_valid &&
        (admission.status == CAN_BUSY || admission.status == CAN_NO_CAPACITY || admission.status == CAN_NOT_READY)) {
        p->state = CAN_PORT_FREE; p->frame = (CanFrame){0}; p->token = (CanTxToken){0};
        self->owner_active = 0; return admission.status;
    }
    CAN_BRANCH("PT08"); /* 수용된 frame/token은 반환 이후에도 Port가 안정 소유한다. */
    p->highest_serial = token.serial; p->accepted_at = now;
    self->last_progress = now; self->health.quiescent = 0;
    if (admission.status == CAN_ACCEPTED) {
        p->state = p->terminal_valid ? CAN_PORT_TERMINAL_READY : CAN_PORT_ACCEPTED;
        self->owner_active = 0; return CAN_ACCEPTED;
    }
    CAN_BRANCH("PT09"); /* MAY_START/불법 결과/reject와 callback 모순도 첫 retained terminal을 덮지 않는다. */
    if (!p->terminal_valid) {
        p->terminal_result = CAN_TX_UNKNOWN; p->terminal_cause = CAN_NOT_READY; p->terminal_at = now;
        p->terminal_valid = 1; p->release_safe = 0;
    }
    p->state = CAN_PORT_TERMINAL_READY;
    self->health.ready = 0; self->health.critical_loss = 1;
    self->health_pending = self->health_transition_pending = self->reset_pending = 1;
    self->owner_active = 0; return CAN_ACCEPTED;
}

CanCoreStatus CanLinkPort_PollEvent(CanLinkPort *self, CanPortEvent *out, CanClockStamp now)
{
    uint16_t n, i;
    uint8_t clock_bad;
    uint32_t delta;
    CanPortSlot *p;
    CanPortEvent event;
    CAN_BRANCH("PP01"); /* 실패 때 out 내용이 바뀌지 않는다. */
    if (self == NULL || out == NULL || !self->initialized || now.valid > 1 || can_port_alias(self,out,sizeof(*out))) return CAN_INVALID_ARG;
    CAN_BRANCH("PP02"); /* 단일 owner에서도 callback 재진입을 명시 거부한다. */
    if (self->owner_active) return CAN_REENTRANT;
    self->owner_active = 1;
    delta = (uint32_t)(now.ms - self->last_progress.ms);
    clock_bad = !self->config.horizon_proven || !now.valid || !self->last_progress.valid || now.epoch != self->last_progress.epoch ||
        delta >= UINT32_C(0x80000000) || delta > self->config.max_clock_gap_ms;
    CAN_BRANCH("PP03"); /* critical capture/clock 불명확성은 기존 conclusive 결과를 보존한 채 한 번 fence한다. */
    if (self->reset_pending || self->health.bus_off || self->health.critical_loss || clock_bad) {
        self->health.ready = 0;
        if (!self->reset_pending) { self->reset_pending = 1; self->fence_applied = 0; }
        for (i = 0; i < self->config.tx_slots; ++i) {
            p = &self->storage.tx[i];
            if (p->state == CAN_PORT_ACCEPTED && !p->terminal_valid) {
                p->terminal_result = CAN_TX_UNKNOWN;
                p->terminal_cause = clock_bad ? CAN_CLOCK_UNKNOWN : CAN_NOT_READY;
                p->terminal_at = now; p->terminal_valid = 1; p->release_safe = 0;
                p->state = CAN_PORT_TERMINAL_READY;
            }
        }
        if (!self->fence_applied) {
            if (self->health.generation != UINT64_MAX) {
                ++self->health.generation;
                for (i = 0; i < self->config.tx_slots; ++i) self->storage.tx[i].highest_serial = 0;
            }
            self->fence_applied = 1; self->health.quiescent = 0; self->recovery_ready = 0;
            self->health_pending = self->health_transition_pending = 1;
            /* binding에 old TX 취소/drain·callback generation 분리를 요구한다.
             * 이 요청 자체는 실제 quiescence의 증거가 아니다. */
        }
    }
    can_port_request_drain(self,now);
    CAN_BRANCH("PP04"); /* current generation의 genuine drain proof만 storage release와 recovery를 허용한다. */
    if (self->reset_pending && self->fence_applied && self->health.quiescent) {
        for (i = 0; i < self->config.tx_slots; ++i) {
            p = &self->storage.tx[i];
            if (p->state == CAN_PORT_UNKNOWN_PINNED) { p->state = CAN_PORT_FREE; p->terminal_valid = 0; p->release_safe = 1; }
            else if (p->state == CAN_PORT_TERMINAL_READY) p->release_safe = 1;
        }
        if (!self->health_transition_pending && self->recovery_ready && self->config.horizon_proven && now.valid &&
            self->health.generation != UINT64_MAX && self->config.binding_kind == CAN_PORT_HOST_PROFILE_ONLY &&
            self->config.fd_metadata_available) {
            self->health.bus_off = 0; self->health.critical_loss = 0; self->health.ready = 1;
            self->reset_pending = self->fence_applied = 0; self->last_progress = now;
            self->health_pending = self->health_transition_pending = 1;
        }
    }
    if (!clock_bad && !self->reset_pending) self->last_progress = now;
    CAN_BRANCH("PP05"); /* 중요한 health 전이 한 번을 먼저 인수한다. 단순 RX drop 변화는 이 우선권을 갖지 않는다. */
    if (self->health_transition_pending) {
        event.kind = CAN_PORT_HEALTH; event.data.health = self->health;
        *out = event; self->health_pending = self->health_transition_pending = 0;
        self->owner_active = 0; return CAN_OK;
    }
    CAN_BRANCH("PP06"); /* live slot fulltoken의 retained terminal은 현재 generation과 다르다고 버리지 않는다. */
    for (n = 0; n < self->config.tx_slots; ++n) {
        i = (self->terminal_cursor + n) % self->config.tx_slots; p = &self->storage.tx[i];
        if (p->state != CAN_PORT_TERMINAL_READY || !p->terminal_valid) continue;
        event.kind = CAN_PORT_TX_TERMINAL;
        event.data.terminal.token = p->token; event.data.terminal.result = p->terminal_result;
        event.data.terminal.cause = p->terminal_cause; event.data.terminal.observed_at = p->terminal_at;
        *out = event;
        if (p->release_safe) { p->state = CAN_PORT_FREE; p->terminal_valid = 0; }
        else p->state = CAN_PORT_UNKNOWN_PINNED;
        self->terminal_cursor = (i + 1) % self->config.tx_slots;
        self->owner_active = 0; return CAN_OK;
    }
    CAN_BRANCH("PP07"); /* 실제 raw metadata/error/time/generation을 그대로 독립 value copy한다. */
    if (self->rx_count != 0) {
        event.kind = CAN_PORT_RX_FRAME; event.data.rx = self->storage.rx[self->rx_head];
        *out = event; self->rx_head = (self->rx_head + 1) % self->config.rx_capacity; --self->rx_count;
        self->owner_active = 0; return CAN_OK;
    }
    CAN_BRANCH("PP08"); /* 일반 overflow/drop snapshot은 coalesce하되 diagnostic evidence를 지우지 않는다. */
    if (self->health_pending) {
        event.kind = CAN_PORT_HEALTH; event.data.health = self->health; *out = event;
        self->health_pending = 0; self->owner_active = 0; return CAN_OK;
    }
    CAN_BRANCH("PP09"); /* wake count가 남아도 owned 사건이 없으면 EMPTY다. */
    self->owner_active = 0; return CAN_EMPTY;
}

CanCoreStatus CanLinkPort_Reset(CanLinkPort *self, CanResetReason reason, CanClockStamp now)
{
    uint16_t i;
    uint8_t clock_bad;
    uint32_t delta;
    CanPortSlot *p;
    CAN_BRANCH("PR01"); /* reset도 표현 오류로 기존 자료를 삭제하지 않는다. */
    if (self == NULL || !self->initialized || now.valid > 1 ||
        (reason != CAN_RESET_REQUESTED && reason != CAN_RESET_BUS_OFF && reason != CAN_RESET_EVENT_LOSS)) return CAN_INVALID_ARG;
    CAN_BRANCH("PR02"); /* 내부 callback/ISR에서 reset을 재진입하지 않는다. */
    if (self->owner_active) return CAN_REENTRANT;
    self->owner_active = 1;
    delta = (uint32_t)(now.ms - self->last_progress.ms);
    clock_bad = !self->config.horizon_proven || !now.valid || !self->last_progress.valid || now.epoch != self->last_progress.epoch ||
        delta >= UINT32_C(0x80000000) || delta > self->config.max_clock_gap_ms;
    CAN_BRANCH("PR03"); /* 아직 확정되지 않은 accepted만 UNKNOWN; 결과와 bytes ownership을 보존한다. */
    self->health.ready = 0;
    if (!self->reset_pending) {
        self->reset_pending = 1; self->fence_applied = 0; self->reset_reason = reason;
        self->health_pending = self->health_transition_pending = 1;
    }
    if (reason == CAN_RESET_BUS_OFF) self->health.bus_off = 1;
    if (reason == CAN_RESET_EVENT_LOSS) self->health.critical_loss = 1;
    for (i = 0; i < self->config.tx_slots; ++i) {
        p = &self->storage.tx[i];
        if (p->state == CAN_PORT_ACCEPTED && !p->terminal_valid) {
            p->terminal_result = CAN_TX_UNKNOWN; p->terminal_cause = clock_bad ? CAN_CLOCK_UNKNOWN : CAN_NOT_READY;
            p->terminal_at = now; p->terminal_valid = 1; p->release_safe = 0; p->state = CAN_PORT_TERMINAL_READY;
        }
    }
    CAN_BRANCH("PR04"); /* 단 한 번 generation fence하며 uint64 wrap으로 old callback alias를 만들지 않는다. */
    if (!self->fence_applied) {
        if (self->health.generation != UINT64_MAX) {
            ++self->health.generation;
            for (i = 0; i < self->config.tx_slots; ++i) self->storage.tx[i].highest_serial = 0;
        }
        self->fence_applied = 1; self->health.quiescent = 0; self->recovery_ready = 0;
        /* binding은 old TX cancel/drain, generation-tagged hook 분리 및 recovery를 진행한다.
         * 원형은 실제 SDK reset 함수가 아니며 이 요청만으로 ready/quiescent를 true로 쓰지 않는다. */
    }
    can_port_request_drain(self,now);
    CAN_BRANCH("PR05"); /* drain 미확인이어도 retained UNKNOWN은 이후 Poll에서 인수할 수 있다. */
    if (self->health.generation == UINT64_MAX || !self->health.quiescent) {
        self->health_pending = 1; self->owner_active = 0; return CAN_NOT_READY;
    }
    CAN_BRANCH("PR06"); /* genuine proof로 이미 통지한 pinned storage만 회수하고 미통지 terminal을 유지한다. */
    for (i = 0; i < self->config.tx_slots; ++i) {
        p = &self->storage.tx[i];
        if (p->state == CAN_PORT_UNKNOWN_PINNED) { p->state = CAN_PORT_FREE; p->terminal_valid = 0; p->release_safe = 1; }
        else if (p->state == CAN_PORT_TERMINAL_READY) p->release_safe = 1;
    }
    if (!self->health_transition_pending && self->recovery_ready && self->config.horizon_proven && now.valid &&
        self->config.binding_kind == CAN_PORT_HOST_PROFILE_ONLY && self->config.fd_metadata_available) {
        self->health.bus_off = 0; self->health.critical_loss = 0; self->health.ready = 1;
        self->reset_pending = self->fence_applied = 0; self->last_progress = now;
        self->health_pending = self->health_transition_pending = 1;
    }
    self->owner_active = 0; return CAN_OK;
}
