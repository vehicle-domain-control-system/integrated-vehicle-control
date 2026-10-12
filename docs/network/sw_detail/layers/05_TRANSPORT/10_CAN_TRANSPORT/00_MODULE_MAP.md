# CAN Transport — 모듈 지도와 공유 private 타입

PROPOSED_PRIVATE / DESIGN_ONLY / BINDING_OPEN. U4C용 문서 후보이며 실제 헤더·SDK ABI·G4D 독립 PASS가 아니다. 기준 9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4.

[Map/공통 타입](00_MODULE_MAP.md) · [Behavior](10_BEHAVIOR.md) · [Pseudocode](20_PSEUDOCODE.md) · [CAN 함수 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN Flow/22사례](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

## 책임과 제외

Transport는 이미 선택된 raw message를 즉시 Codec→Port에 제출하고, accepted 이후 token 수명과 retained outcome/RX ring을 관리한다. 미수용 TX pending은 Service 단독 소유다. 전역 Router·malloc·다른 UART·Node 기능정책·자동 retry가 없다. 두 가상 ECU는 서로 다른 config/storage/Port/Service instance를 가진다.

## 함수 목록

| FN-ID | 후보 함수 | Behavior / Pseudocode |
|---|---|---|
| FN-U4D-CAN-TRANSPORT-INIT | `CanTransport_Init` | [동작](10_BEHAVIOR.md#fn-u4d-can-transport-init) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-transport-init) |
| FN-U4D-CAN-TRANSPORT-TRY-TX | `CanTransport_TryTx` | [동작](10_BEHAVIOR.md#fn-u4d-can-transport-try-tx) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-transport-try-tx) |
| FN-U4D-CAN-TRANSPORT-PUMP | `CanTransport_Pump` | [동작](10_BEHAVIOR.md#fn-u4d-can-transport-pump) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-transport-pump) |
| FN-U4D-CAN-TRANSPORT-TAKE-OUTCOME | `CanTransport_TakeOutcome` | [동작](10_BEHAVIOR.md#fn-u4d-can-transport-take-outcome) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-transport-take-outcome) |
| FN-U4D-CAN-TRANSPORT-TAKE-RX | `CanTransport_TakeRx` | [동작](10_BEHAVIOR.md#fn-u4d-can-transport-take-rx) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-transport-take-rx) |

<a id="shared-types"></a>
<a id="common-types"></a>
## 공통 타입 전체 선언

`uint8_t/16_t/32_t/64_t`, `size_t`, 기존 I1 실제 `i1_cis_rear`, `i1_domain_permission`, `i1_status`를 전제로 한다. `CanLinkPort`는 [Port 선언](../../06_DRIVER_PORT/20_CAN_LINKPORT/00_MODULE_MAP.md)에서 정의되는 opaque 논리 instance다. Cpadding/enum수치는 Wire와 무관하다.

```c
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
```

| 타입/필드 | 타입·owner·수명 | 유효성·wrap·실패 |
|---|---|---|
| CanMsgKind | private discriminator; caller→value copy | REAR/PERMISSION 2개만. 미래 discrete enum 추가 없음 |
| CanCoreStatus | private 반환; 해당 함수 호출 | Wire/NetSubmit enum과 별개; 계약별 변환만 |
| CanTxResult | Port의 retained local terminal | COMPLETE/CONFIRMED_FAIL/UNKNOWN; peer/기능 결과 아님 |
| CanAgeState | Service 진단 metadata | LOWER_BOUND/SATURATED/UNCERTAIN; fresh/ALLOW 의미 없음 |
| CanClockStamp.epoch/ms/valid | u32/u32/u8; Clock owner origin, value copy | valid=1·sameepoch·증명된 gap<2^31. epoch reset은 새 origin; wire time 아님 |
| CanFrame.can_id | u32; Codec/T/P별 owned copy | I1 ID2개, 확장bit 금지, 공개 identifier 추가 없음 |
| CanFrame.ide/fdf/brs/rtr | u8 각각; Codec profile→Port | 각각0/1필수; IDE0 FDF1 BRS1 RTR0 |
| CanFrame.dlc/length/bytes | u8/u8/u8[20]; I1결과→Tcopy→Pcopy | DLC11 length20/used19/B19zero; C sizeof 송신 금지 |
| CanRawMessage.kind/fields | discriminator + 두 기존 I1 struct union | payloadraw폭/enum 그대로; input borrowed는 호출 동안만 |
| CanTxToken.backend_generation | u64; Port origin, T snapshot | 비0. reset fence+1, MAX소진 중지; peerBOOT 아님 |
| CanTxToken.slot/serial | u16/u64; T index/T monotoneper-slot | slot<NT≤UINT16_MAX. serial1..MAX, wrap금지; fulltuple 비교 |
| CanCodecResult.status/i1/i1_called | private enum/i1_status/u8; Codec | called=0이면 i1는 사용하지 않음, 호출 시 exact I1 반환 보존 |
| CanRxRecord.frame/captured_at/backend_generation/controller_error | owned frame/Clock/u64/u32; P→T→caller copies | RX ingress시간, 출생시각 아님; current generation 비교; error0 clean/비0 raw진단격리, SDKbits새확정 없음 |
| CanTxOutcome.token/kind/source_update | fulltoken/kind/u32; T retained mailbox | accepted header source identity 그대로; 재생성 없음 |
| CanTxOutcome.result/cause/observed_at | Port결과/status/localClock | UNKNOWN을 late COMPLETE로 승격하지 않음 |
| CanPortHealth.generation | u64; P sticky state | 초기1은 backend startup quiescence 근거 전제; MAX회수 금지 |
| CanPortHealth.ready/bus_off/quiescent/critical_loss | u8 각0/1; P backend facts | generation fence는 quiescence증거를 대신하지 않음 |
| CanPortHealth.rx_dropped | u32; P ingress | sat atMAX, RX손실이 기능fail 의미는 아님 |
| CanPortEvent.kind/data | private enum/terminal or rx or health union | kind맞는 arm만 읽기; Poll 1value/호출 |
| terminal.token/result/cause/observed_at | fulltoken/localresult/status/Clock | accepted때capture한token; arrival현재gen 덧씌움 금지 |
| CanResetReason | private enum; owner→Port | REQUESTED/BUS_OFF/EVENT_LOSS; SDK ABI 아님 |

## Transport 소유 타입 전체 선언

```c
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
typedef struct CanTransport {
    CanTransportConfig config;
    CanTransportStorage storage;
    CanLinkPort *port;
    CanPortHealth health;
    size_t rx_head, rx_tail, rx_used, outcome_cursor;
    uint32_t rx_dropped, late_event_count;
    CanRxRecord last_rejected_rx;
    CanCodecResult last_codec_result;
    uint8_t rejected_rx_valid, codec_result_valid, initialized, owner_guard;
} CanTransport;
```

| 필드 | owner·수명·조건 |
|---|---|
| Slot.state | owner 전용 FREE→RESERVED→INFLIGHT→OUTCOME_READY→FREE; 미수용rollback만FREE |
| Slot.serial/token | slot provider수명; serial clear/reinit 금지, token generation/slot/serial 3개 모두 비교 |
| Slot.frame/kind/source_update | T owned20B+원 kind/UPDATE; RESERVED~outcome 소비; sourceAge/UPDATE 변경 0 |
| Slot.accepted_at/deadline_fenced | local Clock/0or1; accepted origin·한 reset요청 표시; clockproof미확인 UNKNOWN 요청 |
| Slot.outcome | slot당1 retained; TakeOutcome copy후만FREE |
| Config.tx_capacity/rx_capacity | size_t NT/RT>0, NT=P NT≤UINT16_MAX, backend numeric MB binding OPEN |
| Config.memory_budget_bytes | size_t owner가제공한 실제storage상한; checked 곱/합 적용 |
| Config.watchdog_ms | u32 0<value<2^31, HostTEST_PROFILE_ONLY; 기능기한/강제cancel 아님 |
| Config.max_observation_gap_ms/horizon_proven | u32 gap<2^31/u8=1 외부horizon증거; modulo delta만으로긴interval 증명불가 |
| Storage.tx_slots/tx_count/rx_records/rx_count | provider-owned arrays+길이; instance수명, 다른owner/arrayalias 금지 |
| Transport.config/storage/port/health | configvalue/storageview/비소유Portpointer/stickyPvalue; init부터shutdownproof까지 |
| rx_head/rx_tail/rx_used/outcome_cursor | size_t bounded ringindex<count/used≤RT; modulo nonzero capacity |
| rx_dropped/late_event_count | u32 saturatingdiagnostic; 기능Fault 합성없음 |
| codec_result_valid/last_codec_result | u8/CanCodecResult; 마지막 TX encode exact I1 오류 원인 보존, 값이 없으면 valid0 |
| rejected_rx_valid/last_rejected_rx | u8/CanRxRecord; 최신 진단 raw/error snapshot 1개만 보존, semantic dispatch 없음 |
| Config.generation_fresh_for_pair | u8=1; 동일 Port와 Transport 일대일 생애의 새 token 공간 증거. 초기 backend quiescence 또는 이전 issuer 종료 후 Port generation 증가 필수 |
| initialized/owner_guard | u8 init/IDLE(0),TX(1),PUMP(2),CALLBACK(3); 단일owner Service callback 전후에lowerguard설정/복원허용 |

## 수명·full·복구

Caller message→Codec scratch→T slot copy→P 독립 copy. T와 P byte owner는 별개이며 P UNKNOWN_PINNED 메모리는 T TakeOutcome/free와 무관하다. Port가 NO_START를 증명한 미수용만 Trollback, backend MAY_START는 P CAN_ACCEPTED+UNKNOWN으로 인수한다. T full은 CAN_NO_CAPACITY; 결과 소비가 늦으면 해당 slot을 보존한다. RX full은 drop-newest+satcounter, terminal mailbox는 RX ring과 분리한다.

Watchdog 만료/clockbreak는 **Port Reset 1회 요청**이며 T가 결과를 즉석합성하지 않는다. P에 이미 저장된 COMPLETE/CONFIRMED_FAIL은 유지하고 아직 불명확한 accepted만 UNKNOWN cell을 만든다. oldgeneration terminal도 fulltoken이 liveTslot에 일치하면 인수한다. late/outcome-consumed token은 진단만. RX는 현재 backendgeneration만 내보낸다. reset 후 P quiescence·oldTX drain증거 없으면 ready=0, 재시도새frame 허용없음.

Owner cycle `CanTransport_Pump(now,budget)` 다음 `CanSvc_Pump(now,budget)`; Service는 Transport Pump를 중복호출하지 않는다. RTOS notify는 wake hint이고 record count가 아니다. ISR/backend atomic release/acquire+boundedcapture는 Port binding obligation, Core에 SDK/HAL/RTOS 심볼 추가없음.

## 메모리·시간 예산

`sizeof(CanTransport)+NT*sizeof(CanTransportSlot)+RT*sizeof(CanRxRecord)`를 checked SIZE_MAX 연산으로 계산한다. Port/Svc도 독립 storage이며 전체RAM은 각 모듈 합+최대동시scratch/callstack이다. NT2/RT4/RP4/pending2/budget8는 Host TEST_PROFILE_ONLY 예제이고 targetRAM 수치가 아니다. Pump 최대 budget Portpoll+NT slot scan, TakeRx stale discards≤RT, TakeOutcome scan≤NT. 실제latency/MB/FIFO/priority/stack/IRQ는 OPEN이고 이 유한 FSM을 막지 않는다.

## 보존 Source 근거

- [docs/network/network_sw_architecture.md](../../../../network_sw_architecture.md) SHA256 `e08b9dc47bbd17a6051bd72570042fa832a479be4241196ed5995a1922f08842`.
- [docs/network/interface/04_COMMON/20_API_USAGE_AND_FAILURE.md](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) SHA256 `80c58423edb0c84262a74aca190bb2f981e388ca139120ae5e4ba60d2fae19f7`.
- [tools/network_host/u3_raw_diag/i1/i1_host_codec.c](../../../../../../tools/network_host/u3_raw_diag/i1/i1_host_codec.c) SHA256 `07a9dd07bbac485067f2478870941fe56858b40ddb5a660892108de6d0f4263a`.
- [docs/network/sw_detail/layers/06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md) SHA256 `53f0366d471977b25c46781eea1e33ca4f9600b7b8f3e9ec7ca7bcc0b529961c`.

U4C private 선언 순서는 공통 enum/value types와 opaque forward declarations→Port owned declarations→Transport owned declarations→Service owned declarations→private prototypes다. taggedstruct의 동일type forward/full선언이며 실제SDK헤더/공개ABI 추가가아니다. initializer scratch는 각모듈의 최대callstack과같이전체provider 예산으로계산한다.

상태보유Core Svc/T/P 재진입은diagnostic callback에도동일거부한다. Service가sameowner에서lowerguards를callback전후설정/복원하는유일한예외다. Port ISR capture는ownerAPIguard와별개publish이므로terminal/health를잃지않는다. statelessCodec에는공유stateguard를만들지않는다.
