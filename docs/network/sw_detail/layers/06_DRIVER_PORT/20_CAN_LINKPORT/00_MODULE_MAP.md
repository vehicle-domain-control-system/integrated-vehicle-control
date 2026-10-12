# CAN 논리 LinkPort — 모듈 지도

[↑ Driver Layer](../00_LAYER_OVERVIEW.md) · [기존 U2 의미 계약](../10_CAN_LINKPORT_SEMANTICS.md) · [공통 후보 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [공통 TX/RX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / BINDING_OPEN / TARGET_NOT_VERIFIED / NOT_COMPILED.** 기준 `docs/network@9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4`. 문서 후보이며 실제 SDK/HAL, 공개 Node API, 확정 ABI 또는 U4C 구현이 아니다. 기존 U2 LinkPort 원문은 보존하고 그 아래 논리 상태를 구체화한다.

## 책임과 최소 함수

Port는 bounded 독립 frame 복사, backend admission/terminal, RX capture, health/reset fence를 소유한다. Message Service의 원 관측·TX_SEQUENCE, Codec의 raw 값 검사, Node/App의 생존·기능 허용 판단을 소유하지 않는다. accepted는 Port 저장소와 correlation 인계이며 controller complete·특정 peer RX·App 결과와 다르다.

| 후보 FN-ID | 정확한 함수명 | 책임 | Behavior | Pseudocode |
|---|---|---|---|---|
| FN-U4D-CAN-PORT-INIT | `CanLinkPort_Init` | cfg/storage·cold quiescence·bounded 예산 검증 | [Init](10_BEHAVIOR.md#fn-u4d-can-port-init) | [Init](20_PSEUDOCODE.md#fn-u4d-can-port-init) |
| FN-U4D-CAN-PORT-TRY-TX | `CanLinkPort_TryTx` | 전용 slot copy→backend admission→accepted commit | [TryTx](10_BEHAVIOR.md#fn-u4d-can-port-try-tx) | [TryTx](20_PSEUDOCODE.md#fn-u4d-can-port-try-tx) |
| FN-U4D-CAN-PORT-POLL-EVENT | `CanLinkPort_PollEvent` | retained terminal/RX/health 한 건 인계·회수 | [PollEvent](10_BEHAVIOR.md#fn-u4d-can-port-poll-event) | [PollEvent](20_PSEUDOCODE.md#fn-u4d-can-port-poll-event) |
| FN-U4D-CAN-PORT-RESET | `CanLinkPort_Reset` | admission 정지·UNKNOWN·세대 fence·quiescence | [Reset](10_BEHAVIOR.md#fn-u4d-can-port-reset) | [Reset](20_PSEUDOCODE.md#fn-u4d-can-port-reset) |

호출자는 [Transport](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md) 한 owner다. 모든 공통 `CanCoreStatus`, `CanFrame`, `CanTxToken`, `CanClockStamp`, `CanPortEvent`, `CanPortHealth`, `CanResetReason`의 전체 선언·필드 의미는 [Transport 정본](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#shared-types)을 따른다. Port와 Transport의 `NT`는 같고 `token.slot`이 두 저장소의 같은 index를 지정한다. token serial은 Transport가 발급하고 Port는 accepted high-water를 보존한다.

<a id="port-owned-types"></a>
## Port 소유 후보 타입 전체 선언

```c
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
typedef struct CanLinkPort {
    CanPortConfig config;
    CanPortStorage storage;
    CanPortHealth health;
    CanClockStamp last_progress;
    CanResetReason reset_reason;
    uint16_t rx_head, rx_tail, rx_count, terminal_cursor;
    uint32_t stale_events, duplicate_events, bad_events;
    uint8_t initialized, owner_active, health_pending, health_transition_pending;
    uint8_t reset_pending, fence_applied, recovery_ready;
} CanLinkPort;
```

| 필드 | Owner·수명 | 초기값·유효 조건·변경 의미 |
|---|---|---|
| config `tx_slots`, `rx_capacity` | init copy 후 instance 수명 불변 | 각각 1..`UINT16_MAX`, 실제 static backing count와 일치. `NT`는 Transport와 일치 |
| config `max_clock_gap_ms` | Infra 계약에 대한 Port 검사 한도 | `0 < gap < 2^31`; Host 구성은 TEST_PROFILE_ONLY. 실제 호출 간격/clock 연속성 증거가 있어야 delta 유효 |
| config `horizon_proven` | 각 owner 호출에서 유효한 Infra 간격 계약 | 0/1. 1은 실제 elapsed<2^31·missed multiwrap 없음의 외부 증거 요구이며 숫자 config 자체가 증거가 아님. 증거 상실 시 now.valid=0 또는 새 epoch 제공 필수 |
| config `memory_budget_bytes` | 호출자가 지정한 논리 상한 | 0 금지, overflow 검사한 전체 `sizeof` 식 이상. MCU RAM 확정값이 아님 |
| config `binding_kind` | Port binding 구분 | HOST_PROFILE_ONLY는 후속 Mock 계약용; TARGET_UNBOUND는 ready false를 유지. 실 타깃 능력 승인 아님 |
| admission `status`, `no_start_proven` | TryTx 동안만 존재하는 binding 관측값 | status는 ACCEPTED/BUSY/NO_CAPACITY/NOT_READY 중 하나, no_start_proven은 0/1. ACCEPTED 이외이고 no_start_proven=0이면 MAY_START로 분류. 새로운 vendor API나 frame field가 아님 |
| config `fd_metadata_available`, `initial_quiescent`, `initial_ready` | init 동안 빌리는 정직한 binding 근거를 값 복사 | 0/1. cold quiescence 필수. 초기 ready는 Host profile·FD metadata·valid clock 모두 있을 때만 |
| slot `state` | Port/ISR publish 규약 아래 독점 | FREE 초기; RESERVED는 backend 호출 전; ACCEPTED는 수용 후; TERMINAL_READY는 미소비 terminal; UNKNOWN_PINNED는 UNKNOWN 인계 후 bytes 보존 |
| slot `frame` | Port-owned inline 20 B; accepted부터 immutable | FREE에서 무효. complete/fail 증거+poll 또는 실제 quiescence 전 재사용 금지 |
| slot `token` | Port가 전달받은 full tuple copy | accepted 대상의 generation/slot/serial. wire BOOT_ID와 다름. callback은 제출 당시 tuple을 그대로 전달 |
| slot `highest_serial` | current backend generation의 admission high-water | cold init 0, 같은 generation의 accepted 때만 증가하고 FREE에서도 보존. 새 generation fence에서만 0. Transport의 issuer serial 자체는 Reset에서 지우지 않음. generation/serial MAX는 wrap 금지 |
| slot `accepted_at` | 로컬 Port 수용 기록 | accepted 때 now copy. watchdog origin은 Transport가 동일 시각을 보관하며 Port는 별도 timeout owner를 만들지 않음 |
| slot `terminal_result`, `terminal_cause`, `terminal_at` | 슬롯당 retained cell 한 개 | `terminal_valid=0`이면 무효. terminal 첫 관측 후 결과 불변; UNKNOWN을 late complete로 승격하지 않음 |
| slot `terminal_valid` | callback publish 및 owner 소비 | 0 초기. RESERVED에서도 동기 callback latch 가능. accepted commit 뒤 TERMINAL_READY로 노출 |
| slot `release_safe` | backend lifetime 증거 | 0 초기. complete/confirmed-fail 또는 실제 drain/quiescence 근거가 있어야 1; epoch fence만으로 설정 금지 |
| storage `tx`, `rx` | 호출자가 정적으로 확보, instance/등록 hook 종료까지 생존 | NULL·overlap·불충분 count 금지. view를 값 복사하므로 `CanPortStorage *` 인자 자체는 호출 이후 생존 불필요. backing pointer는 지속 소유 계약 |
| storage counts | backing 배열의 정직한 원소 수 | config와 같음. 초기화 후 변경 금지 |
| instance `config`, `storage` | Port 소유 view/copy | 성공 init 때만 commit. runtime malloc/free 없음 |
| instance `health` | Port와 capture glue가 atomic snapshot 규약으로 갱신 | generation=1(cold proof), ready 조건부, bus_off=0, quiescent=1, critical_loss=0, rx_dropped=0. generation 최대값이면 fence 후 ready false |
| instance `last_progress` | Port 로컬 time continuity watermark | init now; valid epoch·proven gap에서만 전진. remote timestamp 차감 금지 |
| instance `reset_reason` | 최근 실제 reset 논리 사건 | REQUESTED 초기; BUS_OFF/EVENT_LOSS 등 원인을 보존. Vehicle failure로 번역하지 않음 |
| instance ring `rx_head`, `rx_tail`, `rx_count` | bounded ISR producer / owner consumer | 0 초기, modulo RP index, count≤RP. full이면 newest drop하고 기존 사본 보존 |
| instance `terminal_cursor` | owner round-robin cursor | 0 초기, 0..NT-1. terminal 한 건 반환 이후 전진 |
| instance counters `stale_events`, `duplicate_events`, `bad_events` | instance 로컬 진단 | 0 초기, `UINT32_MAX` saturation. 다른 노드의 health로 전파 금지 |
| `initialized`, `owner_active` | owner 문맥 guard | 0 초기, init 후 initialized=1. Service sink CALLBACK 직전 lower IDLE을 확인한 owner가 Transport owner_guard=3과 Port owner_active=1을 함께 설정하고 정상 반환 후 0으로 복원한다. 네 Port 함수는 owner_active=1이면 CAN_REENTRANT로 거부하며 Init에서는 기존 initialized 검사보다 먼저 검사한다. 다중 task mutex 대체 아님 |
| `health_pending`, `health_transition_pending`, `reset_pending`, `fence_applied` | 한 개 sticky health·중요 전이 통지·현재 reset attempt·fence 실행 여부 | 0 초기; 새로운 health 사건은 pending=1, bus-off/reset/recovery 전이는 transition_pending=1. 단순 drop counter 변화가 terminal/RX보다 계속 우선하지 않음. reset 요청과 세대 fence를 구별하여 같은 요청에 반복 세대 증가 없음 |
| `recovery_ready` | current generation binding의 복구 근거 한 bit | 0 초기/reset 시 0. 실제 old TX flush·hook separation·controller 준비 근거로만 1. 아직 전달하지 않은 bus-off/critical flag를 지우는 근거로 사용하지 않음 |

cold init은 등록된 이전 callback과 backend read가 없다는 근거가 필요하다. 같은 instance를 0으로 초기화해 generation=1을 재생성하는 것으로 old callback alias를 해결하지 않는다. 이미 초기화된 instance에 Init 재호출은 NOT_READY이며 Reset을 통해서만 수명을 정리한다.

Port와 Transport는 한 생애에 한 pair다. 최초 pair만 proven cold generation=1로 시작한다. 기존 backend 위에 새 Port 객체를 붙여 generation=1을 재사용하는 것도 금지한다. Transport instance/storage 교체로 serial=0을 다시 만들기 전에는 기존 Port의 새 generation fence·genuine quiescence·retained terminal 정리가 증명되어야 한다. Port의 per-generation admission high-water만 fence에서 0으로 시작하며 기존 T issuer serial은 계속 유지한다. 증거가 없으면 Init NOT_READY다. generation counter를 관리하는 별도 manager/API를 만들지 않는다.

<a id="capture-contract"></a>
## ISR/callback capture 계약 — SDK 함수 신설 아님

| 사건 | bounded capture / atomic publish | full·상실·늦은 사건 |
|---|---|---|
| TX terminal | 제출 당시 full token과 실제 result/cause/time만 일치 slot cell에 publish. RESERVED이면 latch, ACCEPTED면 TERMINAL_READY. immutable frame을 변경하지 않음 | RX ring 공간을 쓰지 않음. 첫 retained terminal의 result/time/release_safe는 불변. 같은 terminal 중복은 counter만. 이전 generation/serial은 stale. 현재 accepted 상관 자체가 불가능하면 sticky critical_loss와 admission stop. UNKNOWN capture는 ready=0·reset_pending=1·health_pending/health_transition_pending=1로 진입하고 release_safe=0으로 pin; 실제 drain 전 새 TX를 시도하지 않음 |
| RX frame | 실제 ID/IDE/FDF/BRS/RTR/DLC/length, controller_error, local capture time, backend generation을 복사. payload는 `min(actual_length,20)`까지만 읽고 actual length는 보존 | RP full은 newest drop, rx_dropped saturation·health pending. length>20을 정상 20으로 보정하지 않음; 후속 Codec 접근 전 reject. 시간 부재는 captured_at.valid=0으로 진단 보존 |
| health | bus-off/reset/recovery/overflow를 sticky owned snapshot에 publish. ISR은 ready=false를 즉시 latch할 수 있음. 현재 generation의 genuine drain은 quiescent=1 및 recovery_ready 근거로 인수 | 최신 recovery가 아직 owner가 인수하지 않은 bus-off/critical loss를 지우지 못함. recovery ready는 owner가 drain 및 critical health 전달을 확인한 뒤만 |

ISR은 최대 20 B와 고정 메타만 복사하고 Codec/Service/App·문자열 로그를 호출하지 않는다. ISR producer와 owner consumer의 reserve→copy→release publish, acquire consume 및 reset fence는 backend가 bounded critical section 또는 증명된 atomics로 제공해야 한다. RTOS/SDK primitive 이름·IRQ priority·시간 수치는 이 문서에서 설치하지 않는다. callback wake count를 packet count로 사용하지 않는다.

후속 Mock 또는 실제 binding은 `NO_START`와 `ACCEPTED`, `MAY_START`를 구별하는 admission 관측을 제공한다. MAY_START라도 Port owned copy와 token이 완성되었다면 CAN_ACCEPTED로 반환한다. 이미 동기 callback의 첫 terminal이 있으면 결과/time/release_safe를 보존하고, 없을 때만 UNKNOWN을 별도 retained terminal로 남긴다. 모순된 backend admission은 admissionoff·critical health·reset 경로로 진단한다. 확실한 NO_START이고 terminal도 없을 때만 reserve rollback한다. backend가 copy 수명도 보장 못 하면 admission 전에 NOT_READY다.

## bounded 메모리·진척·복구

`Mport = sizeof(CanLinkPort) + NT*sizeof(CanPortSlot) + RP*sizeof(CanRxRecord)`이다. initializer의 scratch와 owner call stack은 별도 bounded budget이다. 각 곱/합을 `SIZE_MAX` 범위에서 검사한 후 config budget과 대조한다. Host 예시 NT=2/RP=4는 **TEST_PROFILE_ONLY**이며 S32K RAM·CPU·Task·ISR 보장이 아니다.

Poll 한 호출은 slot sweep 최대 NT·retained terminal scan 최대 NT·RX 한 건·health 한 건 선택 중 한 사건만 반환한다. critical health를 우선 한 번 제공하고 다음 호출에서 retained terminal round-robin/RX를 처리한다. 일반 overflow counter 변화는 terminal/RX를 계속 가리지 않도록 coalesce한다. 새 critical 사건이 반복되는 동안은 정상 admission이 중지되므로 ready/fresh를 꾸미지 않는다. Transport의 budget으로 owner cycle의 총 호출 수도 유한하다.

complete/confirmed-fail은 실제 backend read 종료 근거가 있어야 한다. UNKNOWN 인수 후 `UNKNOWN_PINNED`는 bytes를 유지한다. reset은 admission stop→미확정 accepted UNKNOWN→generation 증가(fence)→old callback 분리→실제 drain/quiescence→이미 인수한 slot 회수/미인수 terminal 보존→recovery health 순서다. Bus-off 후 old mailbox 자동 재전송이 없다는 binding 증거가 READY의 조건이다. UNKNOWN을 fail·complete나 기능 ALLOW로 바꾸지 않는다.

<a id="source-contract"></a>
## 원 근거·OPEN·후속 검증

| 근거 @ base SHA | 파일 SHA256 | 재사용 의미 |
|---|---|---|
| [Architecture](../../../../network_sw_architecture.md) | `e08b9dc47bbd17a6051bd72570042fa832a479be4241196ed5995a1922f08842` | Driver/HAL 책임, CAN sequence accepted commit, Task/superloop 의미 |
| [U2 LinkPort](../10_CAN_LINKPORT_SEMANTICS.md) | `53f0366d471977b25c46781eea1e33ca4f9600b7b8f3e9ec7ca7bcc0b529961c` | 5결과 경계, stable copy, late terminal/health |
| [API 실패](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) | `80c58423edb0c84262a74aca190bb2f981e388ca139120ae5e4ba60d2fae19f7` | local 인계와 Vehicle result 분리 |
| [CAN FD](../../../../interface/03_COMMUNICATION/10_CAN_FD.md) | `d081fe7295bf105119f2d785c77cabed340381def2c79aa4845a01f35d79c71a` | 두 profile raw metadata, 19used/20tx/DLC11 |
| [U3 RAW C](../../../../../../tools/network_host/u3_raw_diag/u3_raw_diag.c) / [H](../../../../../../tools/network_host/u3_raw_diag/u3_raw_diag.h) | C `669be76441c489a107c39af7d2779d259fefba697101f01e63aeb0df97d70293`; H `eb70af02b92c61e4e57e8b7858708e263b1f74c94ff6eea725b81a4d00cead1a` | readonly G3 Host 진단 근거이며 Port에서 호출·제품 TX 승격하지 않음 |

`OPEN-PORT-CAPABILITY / OPEN-SDK / OPEN-HW / OPEN-TIME`는 실제 callback metadata, atomic capture, quiescence, clock/gap/RAM/latency의 Owner 증거가 필요하다. 논리 slot/FSM·반환·copy는 이 문서에서 완성한다. `OPEN-WIRE / OPEN-PERM / OPEN-PROVENANCE / OPEN-UNIT / OPEN-ENUM / OPEN-C2-SOURCE`는 그대로 유지하며 Port가 기능 허용·변환·producer 합의를 해결하지 않는다. 모든 CAN-D01..22 연결은 **후속 U4C DESIGN_ONLY / NOT_EXECUTED**다.
