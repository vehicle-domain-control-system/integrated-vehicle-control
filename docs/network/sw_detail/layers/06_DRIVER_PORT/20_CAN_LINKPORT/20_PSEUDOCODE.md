# CAN 논리 LinkPort — 한국어 C형 의사코드

[모듈/전체 타입](00_MODULE_MAP.md) · [함수 동작](10_BEHAVIOR.md) · [공통 후보 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [TX/RX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / BINDING_OPEN / NOT_COMPILED.** 아래는 C11 파일/Mock/SDK 구현이 아니다. 공통 타입은 [Transport 정본](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#shared-types), Port 전체 타입·capture 규약은 [모듈](00_MODULE_MAP.md#port-owned-types)을 따른다. 코드의 `admission`은 [bounded binding 관측](00_MODULE_MAP.md#capture-contract) 한 번의 결과다. 이 외부 관측 지점은 실제 vendor 함수·새 Core helper로 주장하지 않는다. 표의 모든 Test는 **후속 U4C DESIGN_ONLY / NOT_EXECUTED**다.

owner 한 곳에서 호출한다. Service sink CALLBACK 진입은 Transport owner_guard=0/Port owner_active=0을 먼저 확인하고, sink 직전 owner가 각각 3/1로 설정하여 네 Port 함수 직접 재진입을 차단하며 sink 정상 반환 후 0/0으로 복원한다. 아래 상태 접근은 [atomic capture/consume 규약](00_MODULE_MAP.md#capture-contract)의 bounded exclusion 아래 이루어진다. 실제 OS lock/ISR 함수를 호출하는 코드가 아니며 단순 C field 대입으로 ISR 동시 안전성이 자동 생긴다는 뜻도 아니다. frame/slot 바깥 pointer 범위는 truthful backing 계약이다.

<a id="fn-u4d-can-port-init"></a>
## FN-U4D-CAN-PORT-INIT — CanLinkPort_Init

**원형:** `CanCoreStatus CanLinkPort_Init(CanLinkPort *self, const CanPortConfig *config, CanPortStorage *storage, CanClockStamp now);`

[Behavior](10_BEHAVIOR.md#fn-u4d-can-port-init) · [Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

```c
CanCoreStatus CanLinkPort_Init(CanLinkPort *self, const CanPortConfig *config, CanPortStorage *storage, CanClockStamp now)
{
    size_t m;
    uint16_t i;
    /* PI01: 모든 입력과 지속 생존 backing을 검증하며 아직 저장하지 않는다. */
    if (self == NULL || config == NULL || storage == NULL || storage->tx == NULL || storage->rx == NULL)
        return CAN_INVALID_ARG;
    if (config->tx_slots == 0 || config->rx_capacity == 0 ||
        storage->tx_slots != config->tx_slots || storage->rx_capacity != config->rx_capacity ||
        config->horizon_proven > 1 || config->fd_metadata_available > 1 || config->initial_quiescent > 1 || config->initial_ready > 1 || now.valid > 1 ||
        (config->binding_kind != CAN_PORT_HOST_PROFILE_ONLY && config->binding_kind != CAN_PORT_TARGET_UNBOUND))
        return CAN_INVALID_ARG;
    /* 별도로 증명된 backing count·nonoverlap·cold pair precondition도 필요하다. */
    /* PI02: target sizeof로 계산하되 모든 곱/합을 수행하기 전에 overflow를 차단한다. */
    m = sizeof(CanLinkPort);
    if (config->max_clock_gap_ms == 0 || config->max_clock_gap_ms >= UINT32_C(0x80000000)) return CAN_INVALID_ARG;
    if (config->tx_slots > (SIZE_MAX - m) / sizeof(CanPortSlot)) return CAN_INVALID_ARG;
    m += config->tx_slots * sizeof(CanPortSlot);
    if (config->rx_capacity > (SIZE_MAX - m) / sizeof(CanRxRecord)) return CAN_INVALID_ARG;
    m += config->rx_capacity * sizeof(CanRxRecord);
    if (config->memory_budget_bytes == 0 || m > config->memory_budget_bytes) return CAN_INVALID_ARG;
    /* PI03: callback guard를 initialized보다 먼저 검사하고, generation1 재사용을 막는다. */
    if (self->owner_active) return CAN_REENTRANT;
    if (self->initialized || !config->initial_quiescent) return CAN_NOT_READY;
    /* PI04: 이 지점부터만 성공 commit; config pointer는 보관하지 않고 backing view를 값 복사한다. */
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
```

| 분기 ID | 조건 | 상태·rollback·관측 | 반환 | 후속 U4C Test |
|---|---|---|---|---|
| PI01 | NULL, bool/enum/count/backing/alias precondition 오류 | 새로운 저장/상관 없음, 객체 불변 | CAN_INVALID_ARG | CAN-D03, CAN-D22 |
| PI02 | SIZE_MAX 곱/합 overflow, budget 부족, gap=0 또는 gap≥2^31 | 초기화 전 거부 | CAN_INVALID_ARG | CAN-D05, CAN-D13 |
| PI03 | owner_active=1을 우선 검사, 이후 이미 initialized 또는 cold quiescence 증거 없음 | 재진입 거부, 기존 hook/slot 수명을 건드리지 않음 | CAN_REENTRANT 또는 CAN_NOT_READY | CAN-D11, CAN-D19 |
| PI04 | 검증 성공 | cfg/view copy, FREE/high-water0/ring0, gen1, 조건부 ready | CAN_OK | CAN-D17, CAN-D22 |

<a id="fn-u4d-can-port-try-tx"></a>
## FN-U4D-CAN-PORT-TRY-TX — CanLinkPort_TryTx

**원형:** `CanCoreStatus CanLinkPort_TryTx(CanLinkPort *self, const CanFrame *frame, CanTxToken token, CanClockStamp now);`

[Behavior](10_BEHAVIOR.md#fn-u4d-can-port-try-tx) · [TX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

```c
CanCoreStatus CanLinkPort_TryTx(CanLinkPort *self, const CanFrame *frame, CanTxToken token, CanClockStamp now)
{
    CanPortSlot *p;
    CanPortAdmissionObservation admission;
    uint16_t i;
    uint8_t free_exists = 0;
    uint32_t delta;
    /* PT01: metadata/length 검사를 복사 전에 끝낸다. raw payload 의미는 기존 Codec 책임이다. */
    if (self == NULL || frame == NULL || !self->initialized || now.valid > 1) return CAN_INVALID_ARG;
    if (token.slot >= self->config.tx_slots || token.serial == 0) return CAN_INVALID_ARG;
    if ((frame->can_id != UINT32_C(0x0A0) && frame->can_id != UINT32_C(0x080)) ||
        frame->ide != 0 || frame->fdf != 1 || frame->brs != 1 || frame->rtr != 0 ||
        frame->dlc != 11 || frame->length != 20) return CAN_PROFILE_REJECT;
    /* PT02: callback이 이 함수를 재진입해도 owner 상태를 새로 변경하지 않는다. */
    if (self->owner_active) return CAN_REENTRANT;
    self->owner_active = 1; p = &self->storage.tx[token.slot];
    /* PT03: 준비/세대/serial 한계를 확인한다. 과거 retained 사건은 그대로 둔다. */
    if (!self->health.ready || self->health.bus_off || self->health.critical_loss || self->reset_pending ||
        token.backend_generation != self->health.generation || p->highest_serial == UINT64_MAX) {
        self->owner_active = 0; return CAN_NOT_READY;
    }
    /* PT04: 동일 local epoch와 증명된 제한간격에서만 u32 modular delta를 인정한다. */
    delta = (uint32_t)(now.ms - self->last_progress.ms);
    if (!self->config.horizon_proven || !now.valid || !self->last_progress.valid || now.epoch != self->last_progress.epoch ||
        delta >= UINT32_C(0x80000000) || delta > self->config.max_clock_gap_ms) {
        self->health.ready = 0;
        self->health_pending = self->health_transition_pending = 1;
        self->reset_pending = 1;
        self->owner_active = 0; return CAN_CLOCK_UNKNOWN;
    }
    /* PT05: 같은 slot identity는 accepted/pinned 상태에서 절대 덮어쓰지 않는다. */
    if (token.serial <= p->highest_serial) { self->owner_active = 0; return CAN_INVALID_ARG; }
    if (p->state != CAN_PORT_FREE) {
        for (i = 0; i < self->config.tx_slots; ++i) free_exists |= self->storage.tx[i].state == CAN_PORT_FREE;
        self->owner_active = 0; return free_exists ? CAN_BUSY : CAN_NO_CAPACITY;
    }
    /* PT06: reserve→독립 value copy→fulltoken publish 후 backend bounded try를 정확히 한 번 진행한다. */
    p->state = CAN_PORT_RESERVED; p->terminal_valid = 0; p->release_safe = 0;
    p->frame = *frame; p->token = token;
    /* binding 관측 지점: backend가 Port-owned p->frame과 제출 당시 token을 받는다.
     * 단 한 번의 admission 결과(status, no_start_proven)를 admission에 기록한다.
     * 동기 callback이면 이 RESERVED slot의 retained cell만 latch한다.
     * backend가 borrowed 주소를 읽을 수 있는 동안 위 frame은 바뀌지 않는다.
     * 실제 vendor/Mock 함수 구현은 U4C/U10 별도 범위다. */
    /* PT07: 확실한 NO_START만 rollback한다. callback latch와 모순되면 이 경로를 쓰지 않는다. */
    if (admission.no_start_proven == 1 && !p->terminal_valid &&
        (admission.status == CAN_BUSY || admission.status == CAN_NO_CAPACITY || admission.status == CAN_NOT_READY)) {
        p->state = CAN_PORT_FREE; p->frame = (CanFrame){0}; p->token = (CanTxToken){0};
        self->owner_active = 0; return admission.status;
    }
    /* PT08: 수용된 frame/token은 반환 이후에도 Port가 안정 소유한다. */
    p->highest_serial = token.serial; p->accepted_at = now;
    self->last_progress = now; self->health.quiescent = 0;
    if (admission.status == CAN_ACCEPTED) {
        p->state = p->terminal_valid ? CAN_PORT_TERMINAL_READY : CAN_PORT_ACCEPTED;
        self->owner_active = 0; return CAN_ACCEPTED;
    }
    /* PT09: MAY_START/불법 결과/reject와 callback 모순도 첫 retained terminal을 덮지 않는다. */
    if (!p->terminal_valid) {
        p->terminal_result = CAN_TX_UNKNOWN; p->terminal_cause = CAN_NOT_READY; p->terminal_at = now;
        p->terminal_valid = 1; p->release_safe = 0;
    }
    p->state = CAN_PORT_TERMINAL_READY;
    self->health.ready = 0; self->health.critical_loss = 1;
    self->health_pending = self->health_transition_pending = self->reset_pending = 1;
    self->owner_active = 0; return CAN_ACCEPTED;
}
```

| 분기 ID | 조건 | 상태·rollback·관측 | 반환 | 후속 U4C Test |
|---|---|---|---|---|
| PT01 | NULL/init/profile/token 표현 오류 | reserve 전 불변 | CAN_INVALID_ARG 또는 CAN_PROFILE_REJECT | CAN-D03, CAN-D04 |
| PT02 | owner_active=1 | 재진입 작업 없음 | CAN_REENTRANT | CAN-D19 |
| PT03 | not ready/bus-off/critical loss/reset pending, 이전 generation 또는 serial 한계 | 새 수용 없음, 이미 retained 자료 유지 | CAN_NOT_READY | CAN-D05, CAN-D10, CAN-D11 |
| PT04 | invalid clock/epoch break/ambiguous 또는 gap 미증명 | ready=false·health pending, 기존 accepted는 reset 경로에서 UNKNOWN | CAN_CLOCK_UNKNOWN | CAN-D13, CAN-D14 |
| PT05 | slot이 FREE가 아님 또는 이전 serial 재사용 | silent overwrite 없음 | CAN_BUSY/NO_CAPACITY 또는 CAN_INVALID_ARG | CAN-D05, CAN-D06, CAN-D09, CAN-D11 |
| PT06 | slot FREE와 token 유효 | RESERVED→독립 copy, callback latch 허용, backend 시도1회 | 분기 계속 | CAN-D01, CAN-D08, CAN-D19 |
| PT07 | backend NO_START 확정, latch 없음 | FREE rollback, copy 무효, highest_serial 불변 | CAN_BUSY/NO_CAPACITY/NOT_READY | CAN-D05 |
| PT08 | backend 수용, fulltoken/copy 생존 | high-water commit, ACCEPTED 또는 latch된 TERMINAL_READY, bytes immutable | CAN_ACCEPTED | CAN-D01, CAN-D08, CAN-D09 |
| PT09 | MAY_START 또는 reject/latch 모순 | high-water commit, 첫 terminal 보존/없을 때 UNKNOWN+pin, admissionoff/reset pending; 다음 owner fence 실행 | CAN_ACCEPTED | CAN-D09, CAN-D19 |

<a id="fn-u4d-can-port-poll-event"></a>
## FN-U4D-CAN-PORT-POLL-EVENT — CanLinkPort_PollEvent

**원형:** `CanCoreStatus CanLinkPort_PollEvent(CanLinkPort *self, CanPortEvent *out, CanClockStamp now);`

[Behavior](10_BEHAVIOR.md#fn-u4d-can-port-poll-event) · [RX/terminal Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

```c
CanCoreStatus CanLinkPort_PollEvent(CanLinkPort *self, CanPortEvent *out, CanClockStamp now)
{
    uint16_t n, i;
    uint8_t clock_bad;
    uint32_t delta;
    CanPortSlot *p;
    CanPortEvent event;
    /* PP01: 실패 때 out 내용이 바뀌지 않는다. */
    if (self == NULL || out == NULL || !self->initialized || now.valid > 1) return CAN_INVALID_ARG;
    /* PP02: 단일 owner에서도 callback 재진입을 명시 거부한다. */
    if (self->owner_active) return CAN_REENTRANT;
    self->owner_active = 1;
    delta = (uint32_t)(now.ms - self->last_progress.ms);
    clock_bad = !self->config.horizon_proven || !now.valid || !self->last_progress.valid || now.epoch != self->last_progress.epoch ||
        delta >= UINT32_C(0x80000000) || delta > self->config.max_clock_gap_ms;
    /* PP03: critical capture/clock 불명확성은 기존 conclusive 결과를 보존한 채 한 번 fence한다. */
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
    /* PP04: current generation의 genuine drain proof만 storage release와 recovery를 허용한다. */
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
    /* PP05: 중요한 health 전이 한 번을 먼저 인수한다. 단순 RX drop 변화는 이 우선권을 갖지 않는다. */
    if (self->health_transition_pending) {
        event.kind = CAN_PORT_HEALTH; event.data.health = self->health;
        *out = event; self->health_pending = self->health_transition_pending = 0;
        self->owner_active = 0; return CAN_OK;
    }
    /* PP06: live slot fulltoken의 retained terminal은 현재 generation과 다르다고 버리지 않는다. */
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
    /* PP07: 실제 raw metadata/error/time/generation을 그대로 독립 value copy한다. */
    if (self->rx_count != 0) {
        event.kind = CAN_PORT_RX_FRAME; event.data.rx = self->storage.rx[self->rx_head];
        *out = event; self->rx_head = (self->rx_head + 1) % self->config.rx_capacity; --self->rx_count;
        self->owner_active = 0; return CAN_OK;
    }
    /* PP08: 일반 overflow/drop snapshot은 coalesce하되 diagnostic evidence를 지우지 않는다. */
    if (self->health_pending) {
        event.kind = CAN_PORT_HEALTH; event.data.health = self->health; *out = event;
        self->health_pending = 0; self->owner_active = 0; return CAN_OK;
    }
    /* PP09: wake count가 남아도 owned 사건이 없으면 EMPTY다. */
    self->owner_active = 0; return CAN_EMPTY;
}
```

| 분기 ID | 조건 | 상태·rollback·관측 | 반환 | 후속 U4C Test |
|---|---|---|---|---|
| PP01 | NULL/init/output alias 또는 invalid bool representation | out 불변 | CAN_INVALID_ARG | CAN-D03 |
| PP02 | owner_active=1 | 재진입 없음, out 불변 | CAN_REENTRANT | CAN-D19 |
| PP03 | 새 bus-off/critical loss/clock continuity break | admissionoff, unresolved UNKNOWN, reset fence 한 번, conclusive terminal 유지 | 사건 선택 계속 | CAN-D10, CAN-D13, CAN-D19 |
| PP04 | recovery/drain proof 있고 quiescent | 미통지 terminal 유지, 통지 UNKNOWN slot만 release; 준비조건일 때 ready | 사건 선택 계속 | CAN-D10, CAN-D11 |
| PP05 | 중요한 health 전이 pending | 완전한 health copy, sticky critical cause 유지, 전이 pending만 소비 | CAN_OK | CAN-D10, CAN-D16, CAN-D19 |
| PP06 | retained terminal 있음 | token/result/cause/time value copy 후 FREE 또는 UNKNOWN_PINNED, cursor 전진 | CAN_OK | CAN-D08, CAN-D09, CAN-D11, CAN-D19 |
| PP07 | RX 있음 | full raw/meta/controller_error/time/generation copy 후 ring release | CAN_OK | CAN-D03, CAN-D16, CAN-D18 |
| PP08 | 일반 health만 있음 | coalesced health copy, counter 보존 | CAN_OK | CAN-D16, CAN-D22 |
| PP09 | 사건 없음 | out 불변, guard 해제 | CAN_EMPTY | CAN-D17, CAN-D22 |

<a id="fn-u4d-can-port-reset"></a>
## FN-U4D-CAN-PORT-RESET — CanLinkPort_Reset

**원형:** `CanCoreStatus CanLinkPort_Reset(CanLinkPort *self, CanResetReason reason, CanClockStamp now);`

[Behavior](10_BEHAVIOR.md#fn-u4d-can-port-reset) · [복구 Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

```c
CanCoreStatus CanLinkPort_Reset(CanLinkPort *self, CanResetReason reason, CanClockStamp now)
{
    uint16_t i;
    uint8_t clock_bad;
    uint32_t delta;
    CanPortSlot *p;
    /* PR01: reset도 표현 오류로 기존 자료를 삭제하지 않는다. */
    if (self == NULL || !self->initialized || now.valid > 1 ||
        (reason != CAN_RESET_REQUESTED && reason != CAN_RESET_BUS_OFF && reason != CAN_RESET_EVENT_LOSS)) return CAN_INVALID_ARG;
    /* PR02: 내부 callback/ISR에서 reset을 재진입하지 않는다. */
    if (self->owner_active) return CAN_REENTRANT;
    self->owner_active = 1;
    delta = (uint32_t)(now.ms - self->last_progress.ms);
    clock_bad = !self->config.horizon_proven || !now.valid || !self->last_progress.valid || now.epoch != self->last_progress.epoch ||
        delta >= UINT32_C(0x80000000) || delta > self->config.max_clock_gap_ms;
    /* PR03: 아직 확정되지 않은 accepted만 UNKNOWN; 결과와 bytes ownership을 보존한다. */
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
    /* PR04: 단 한 번 generation fence하며 uint64 wrap으로 old callback alias를 만들지 않는다. */
    if (!self->fence_applied) {
        if (self->health.generation != UINT64_MAX) {
            ++self->health.generation;
            for (i = 0; i < self->config.tx_slots; ++i) self->storage.tx[i].highest_serial = 0;
        }
        self->fence_applied = 1; self->health.quiescent = 0; self->recovery_ready = 0;
        /* binding은 old TX cancel/drain, generation-tagged hook 분리 및 recovery를 진행한다.
         * 원형은 실제 SDK reset 함수가 아니며 이 요청만으로 ready/quiescent를 true로 쓰지 않는다. */
    }
    /* PR05: drain 미확인이어도 retained UNKNOWN은 이후 Poll에서 인수할 수 있다. */
    if (self->health.generation == UINT64_MAX || !self->health.quiescent) {
        self->health_pending = 1; self->owner_active = 0; return CAN_NOT_READY;
    }
    /* PR06: genuine proof로 이미 통지한 pinned storage만 회수하고 미통지 terminal을 유지한다. */
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
```

| 분기 ID | 조건 | 상태·rollback·관측 | 반환 | 후속 U4C Test |
|---|---|---|---|---|
| PR01 | NULL/init/reason/valid 표현 오류 | 상태 불변 | CAN_INVALID_ARG | CAN-D03 |
| PR02 | owner_active=1 | 재진입 없음 | CAN_REENTRANT | CAN-D19 |
| PR03 | 첫 reset 요청 | admissionoff, unresolved UNKNOWN, conclusive 결과 유지, bytes pin, reset pending | 분기 계속 | CAN-D09, CAN-D10, CAN-D19 |
| PR04 | 첫 fence 및 generation<UINT64_MAX | generation+1, old token/bytes 보존, 새세대 admission high-water0 | 분기 계속 | CAN-D11, CAN-D13 |
| PR05 | generation 최대값 또는 pending drain 미확인 | wrap 없음, UNKNOWN pin/retained terminal 보존, health pending | CAN_NOT_READY | CAN-D10, CAN-D11 |
| PR06 | 실제 quiescence/old TX flush·hook separation 확인 | 미통지 terminal 보존, 통지 pinned만 FREE, 준비조건에 따른 recovery health | CAN_OK | CAN-D10, CAN-D11, CAN-D15 |

## 수명·시간·검증 범위

callback은 제출 때 받은 full token을 저장하며 도착 당시 generation을 붙이지 않는다. old generation terminal은 live slot token 일치 때만 인수하고, 이미 terminal/재사용 slot의 late 사건은 결과를 되돌리지 않는다. controller read 수명은 token ABA와 별도로 지킨다. UNKNOWN_PINNED의 immutable bytes를 backend drain 전에 덮지 않는다.

첫 UNKNOWN capture는 terminal cell publish와 함께 ready=0·reset_pending=1·sticky health 전이를 atomically latch한다. 기존 conclusive terminal을 duplicate UNKNOWN으로 바꾸는 capture는 금지한다. 따라서 UNKNOWN을 T가 소비했어도 Port recovery 없이 같은 pinned slot에 새 TX가 반복 제출되는 상태를 만들지 않는다. 모순된 synchronous admission도 첫 terminal을 보존하며 critical health/reset만 추가한다.

Rx metadata/controller_error와 실제 length를 보존하는 [capture 계약](00_MODULE_MAP.md#capture-contract)은 Codec 실행 전의 최소 copy다. controller_error≠0·old backend generation·malformed profile은 Transport/Codec에서 진단 격리하고 원 INVALID/STALE raw quality와 구별한다. Port는 producer BOOT/UPDATE/AGE 및 Function ALLOW를 수정하지 않는다.

초기 pair/cold quiescence·serial lifetime·메모리 예산·clock origin의 [소유 필드](00_MODULE_MAP.md#port-owned-types) 및 [원 Source SHA/OPEN](00_MODULE_MAP.md#source-contract)을 함께 검수한다. superloop/RTOS는 동일 owner 호출 순서이고 실제 SDK, RAM, latency, IRQ/Task 숫자는 OPEN이다. 이 문서의 분기표는 구현 시험 계획이며 Host/보드 PASS를 주장하지 않는다.
