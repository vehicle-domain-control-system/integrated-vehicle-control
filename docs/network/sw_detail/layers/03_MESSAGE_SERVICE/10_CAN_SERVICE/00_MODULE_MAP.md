# CAN Message Service — 원 관측과 pending의 소유

[↑ Layer](../00_LAYER_OVERVIEW.md) · [Network](../../../00_START_HERE.md) · [후보 함수 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN 공통 Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / BINDING_OPEN / NOT_COMPILED.** 기준 `docs/network@9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4`. 두 current 메시지의 원 값·품질·UPDATE·AGE를 보존하면서 caller와 무관한 pending을 접수하고, owner 진행에서 실제 Port 수용을 시도한다. RX에서는 명시 source 문맥과 순서를 확인해 진단 사본을 보관한다. U4C 코드·Mock 시험은 이번에 실행하지 않았다. 공개 Node API·Permission 정책·팀 Wire 계약을 추가하지 않는다.

| 함수 ID | 책임 | 설명 / 의사코드 |
|---|---|---|
| FN-U4D-CAN-SVC-INIT | 독립 instance와 고정 용량 설정 | [설명](10_BEHAVIOR.md#fn-u4d-can-svc-init) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-init) |
| FN-U4D-CAN-SVC-SUBMIT | 구조·원 AGE 확인 후 current 전체 접수/교체 | [설명](10_BEHAVIOR.md#fn-u4d-can-svc-submit) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-submit) |
| FN-U4D-CAN-SVC-PUMP | terminal/RX 소비와 bounded TX 시도 | [설명](10_BEHAVIOR.md#fn-u4d-can-svc-pump) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-pump) |
| FN-U4D-CAN-SVC-ON-DECODED | source 순서·AGE 보존, 진단 callback | [설명](10_BEHAVIOR.md#fn-u4d-can-svc-on-decoded) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-on-decoded) |
| FN-U4D-CAN-SVC-BIND-SOURCE | owner의 명시 peer 문맥을 한 kind에 인수 | [설명](10_BEHAVIOR.md#fn-u4d-can-svc-bind-source) / [의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-bind-source) |

<a id="service-types"></a>
## 전체 사적 상태 선언

공유 `CanRawMessage`, `CanClockStamp`, `CanAgeState`, `CanTxOutcome`, `CanCoreStatus`와 실제 I1 두 struct는 [Transport 공유 타입](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#shared-types)을 따른다. 아래 선언은 문서상 후보이며 실제 header가 아니다. `CanTransport *`의 완전한 객체 소유는 Transport에 있고 Service는 runtime 종료까지 그 안정된 주소를 참조한다.

```c
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

typedef struct CanService {
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
} CanService;
```

### 필드별 소유·표현범위·수명

모든 배열은 `CAN_REAR=0 / CAN_PERMISSION=1`이라는 private key 순서로만 접근한다. 이 index를 CAN ID 또는 Wire code로 전송하지 않는다. 0/1 필드와 guard는 Service owner가 갱신하며 zero-init 뒤 정상 Init commit부터 유효하다.

| 필드 | owner · 초기값 · 유효조건 / 수명 |
|---|---|
| config.local_boot | Node runtime owner 제공, `1..UINT32_MAX`; Service 복사 후 불변. Backend reset으로 변경하지 않음 |
| config.pending_capacity | Service 설정, `1..2`; physical 2개 중 해당 범위만 접수. 실제 MCU 용량 채택 아님 |
| config.tx_mask / rx_mask | Node owner, bit0/bit1만 허용. CIS·DOMAIN·BCM·VSS의 실제 역할 바인딩은 U2와 OPEN 유지 |
| config.max_observation_gap_ms | owner의 연속 관측 한계 근거, `0 < value < 2^31`; source 기한이나 150 ms가 아님 |
| config.source_horizon_ms[2] | kind별 원 TX/UPDATE 비교의 외부 rate/window 근거, `0 < value < 2^31`. 이 기간 동안 두 순번의 실제 전진이 각각 반범위 미만이라는 별도 근거가 필요하며 owner poll 간격·기능 deadline과 다름 |
| config.external_horizon_proven | `0/1`, Init에는 1 필요. owner가 각 로컬 Clock 비교의 실제 구간이 반범위 미만이고 각 source_horizon_ms[k] 동안 TX/UPDATE 실제 전진이 각각 반범위 미만이라는 별도 근거를 제공함; Host sample은 TEST_PROFILE_ONLY |
| config.memory_budget_bytes | `size_t`, 아래 합산 공간 이상. 숫자를 실제 S32K RAM 예산으로 승인하지 않음 |
| config.diagnostic_sink / sink_context | nullable 사적 진단 sink와 borrowed context. 포인터만 config에 복사; owner가 instance lifetime 동안 대상 수명을 보장. sink는 같은 owner 문맥에서 정상 반환해야 하고 nonlocal exit로 guard 복원을 건너뛰지 않음; 실제 소요시간 상한은 OPEN |
| pending.occupied | Service, 초기0; complete pending snapshot commit에서1, Port accepted 뒤0. 다른 accepted bytes를 가리키지 않음 |
| pending.original | caller 필드의 독립 사본, pending 유효기간. TX 후보에서는 Boot/Tx/AGE만 scratch에 명시 보완; 원 값/quality/UPDATE는 불변 |
| pending.age_anchor / age_at_anchor / age_state | Service, 해당 anchor까지 계상한 u16 AGE와 LOWER_BOUND/SATURATED/UNCERTAIN. unknown 경과를 0/65535로 보충하지 않음 |
| tx_origin.has_observation | Service 초기0, 정상 Submit commit 뒤1. pending 소비 이후에도 재제출 anti-reset 근거로 유지 |
| tx_origin.original | kind별 마지막 실제 원 값·quality·UPDATE 사본. identity 동일 비교에는 Boot/TX/AGE와 C padding을 제외 |
| tx_origin.last_observed_at | 원 Submit이 안정 접수된 로컬 stamp. source별 UPDATE 순서 window 비교에 사용하며 AGE anchor와 별개; source silent 기간의 기능 deadline을 확정하지 않음 |
| tx_origin.age_anchor / age_at_anchor / age_state | Service의 누적 AGE watermark. 같은 UPDATE 재제출 시 현재 시점에 누적한 값보다 AGE를 낮출 수 없음 |
| rx_origin.bound / admitted_boot / owner_context | Node owner가 BindSource로 명시 인수. Boot는 0 예약, context는 `1..UINT64_MAX` 단조; context wrap 금지 |
| rx_origin.has_record / last | Service, 초기0. accepted source의 전체 진단 view commit 뒤1. 다른 Boot bind 시 과거 raw 사본은 남겨도 유효 current로 사용하지 않음 |
| rx_origin.last_valid_frame | 같은 source의 새 TX+적합 원 UPDATE가 실제 수용된 로컬 RX stamp. malformed/replay/다른 kind로 갱신하지 않음 |
| rx_origin.last_new_update | 실제 원 UPDATE가 바뀌거나 첫 원 기록을 수용한 로컬 RX stamp. 동일 UPDATE 새 TX로 갱신하지 않음 |
| rx_view.original | Codec가 검증한 원 I1 fields 전체 사본. 받은 Wire AGE/TX/Boot를 그대로 보존; struct 자체를 bytes로 cast하지 않음 |
| rx_view.received_at / age_anchor | 로컬 RX capture / 누적 계산 기준. 같은 epoch와 입증된 gap에서만 로컬 차감; 원 노드 시계와 차감하지 않음 |
| rx_view.effective_age_ms / age_state | Service 계산, `0..65535`; 수치 하한과 포화/미확인 구간을 분리. upper-bound freshness 또는 기능 ALLOW의 증거가 아님 |
| rx_view.new_observation / owner_context | 0/1의 실제 원 UPDATE 변경 표시와 현재 명시 접수 문맥. TX 반복은0; 진단 수명까지만 borrowed |
| initialized / config / transport | initialized 초기0, Init 최종 commit1; config는 복사, transport는 안정 주소 borrow. live 객체의 재Init 금지 |
| last_owner_now | Service 초기 invalid, 유효 Submit/Pump/OnDecoded의 가장 최근 로컬 now. owner 관측 gap만 검사; 단절 시 기존 AGE anchor를 invalid/UNCERTAIN으로 남기고 이후 단순 정상 poll로 치유하지 않음 |
| tx_next[2] | 송신 Network owner, 각각 최초1. 해당 kind Port accepted에서만 u32 modulo 증가; UPDATE와 독립 |
| fairness_cursor | Service, 초기0, `0..1`; 두 key의 TX 시도 순서를 번갈아 진행 |
| guard / dispatch_permit | Service, 초기 IDLE/0. Pump 내부 OnDecoded에만 일회 permit; sink 호출 전 소비하고 CALLBACK guard. ISR/외부 재진입은 거부 |
| transport.owner_guard / transport.port.owner_active의 callback 구간 | 평상시 각 모듈 owner; 같은 Core owner의 Service가 SR12에서만 T CALLBACK=3/P active=1로 전파하고 정상 반환 뒤 저장한 IDLE/0으로 복원. consumer의 T/P 직접 진입도 각 모듈 guard에서 거부; 새 helper/API 없음 |
| has_outcome[2] / last_outcome[2] | Service, 초기0. TakeOutcome로 한 번 소비한 local terminal의 독립 최근 사본. 원 token/update를 그대로 보존; 차량 failure로 변환하지 않음 |
| last_diagnostic | Service, 초기 CAN_EMPTY, 마지막 저장된 Submit admission/Pump 진단 요약. 반환값 전체의 history가 아니며 기능 Result를 합성하지 않음 |
| has_codec_reject / last_codec_result / last_rejected_rx | Service 초기0. 마지막 Codec/profile 거부의 status·정확한 I1 결과·I1 호출 여부와 원 수신 record를 1개만 독립 보존. I1_ERR_PADDING/ERR_RESERVED 등 원인 구분 유지; 별도 queue/공개 failure API 없음 |
| counters의 11개 u32 필드 | Service 초기0, 실제 해당 관측에서만 증가, UINT32_MAX에서 포화. rejected/full/replaced/busy/not_ready, origin_conflicts/order_rejects/delivered, complete/confirmed_fail/unknown을 별개로 보존 |

<a id="service-memory"></a>
## 저장과 처리 상한

`M_service = 2*sizeof(CanService) + sizeof(CanRawMessage) + sizeof(CanRxRecord) + sizeof(CanTxOutcome) + sizeof(CanSvcRxView)`는 Init의 전체 잠정 객체와 owner 호출의 동시 scratch를 포함한 보수적 합산식이다. C padding을 포함한 **RAM 예산 식**이며 Wire 크기를 결정하지 않는다. 실제 U4C sizeof/stack 측정과 U10 MCU 예산은 별도이다. 각 `SIZE_MAX - accumulated >= next_size` 검사 후 합산하고 budget 미달 시 Init 이전 상태를 보존한다. Runtime malloc/free는 없다.

pending은 capacity 1..2, TX watermark 2개, RX 문맥 2개, 최근 outcome 2개다. accepted byte는 [Transport](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md)와 [Port](../../06_DRIVER_PORT/20_CAN_LINKPORT/00_MODULE_MAP.md)가 각각 독립 소유한다. Service가 accepted slot 주소를 pending에 저장하지 않는다. retained 원 관측은 과거 결과 DB가 아니라 같은 UPDATE의 AGE 재초기화를 막는 두 watermark다.

Pump `budget>=3`을 요구한다. TX quota=`min(2,budget/3)`, outcome quota=`(budget-TXquota)/2`, RX quota=나머지다. phase별 empty를 만나면 한 번에 중단하고 TX는 key 최대2/slot 최대2를 탐색한다. 상한은 `O(budget+4)`이고 outcome/RX burst에도 TX quota가 보존된다. Host `pending=2,budget=8`은 TEST_PROFILE_ONLY이며 Task/IRQ priority·deadline 보장이 아니다.

<a id="service-source"></a>
## 기존 자료와 호출 경계

| 실제 근거 @ 기준 Git SHA | SHA256 / 재사용 |
|---|---|
| [CIS API](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishrear) | `7db6c5c3dacac4b39c2b28b030e40c1fd60b44fe200a69b7568e8f6da57cf5f6`; PublishRear·OnDomainPermission 원형 보존 |
| [DOMAIN API](../../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisrear) | `03a4e8dadfd78a41f73ce13af44eccb9a2504a48924d024b91570a48e6d0a96d`; C2 ingress/전원 TxPort의 local 수용 의미 |
| [DOMAIN private Publish](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnet_publishpermission) | `f99f1af5271c677e14c68bfbf3e9ab9d8f8ca8a68d62d5f1777eb9c122394cb2`; 원 composer 합의는 Service 밖 |
| [BCM private 감독](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnetpriv_ondomainpermission) | `1fca4f3d4fca86a6e32e7c7f0e77b23bcb363aeb28a69488aa639731a4cce809`; 새 App permission callback 없음 |
| [VSS private 감독](../../../../interface/02_NODES/VSS/20_API.md#api-vssnetpriv_ondomainpermission) | `a0b225e56c917b0848a0f2adfab1a862b5472bc5c7958dcca1f5cc67f2421a06`; recovery permission 변환 없음 |
| [공통 ID/AGE](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md#section-quality-freshness) | `cd7d0bbf2891d75dbeca1eb154e9589ecee9e768388e14574a428ea68b63a5af`; origin/tx/update 분리·AGE 단회 누적 |
| [CIS Data](../../../../interface/02_NODES/CIS/10_DATA.md#type-netcisrear) | `b272406da0a9d90e9f0a98d7f9d0a4ca7dadc22e7e41af11545e4867bebf4431`; raw 0.1cm/상태/sentinel 보존 |
| [실제 I1 C/H](../../../../../../tools/network_host/u3_raw_diag/i1/i1_host_codec.h) | header `803d44cd09784168aae68a00a99eef59f70a260ac3149f1f94e222ad1969cd20`; C `07a9dd07bbac485067f2478870941fe56858b40ddb5a660892108de6d0f4263a` |

[CIS_REAR U2 Flow](../../../flows/10_CIS_REAR_FLOW.md), [DOMAIN_PERMISSION U2 Flow](../../../flows/20_DOMAIN_PERMISSION_FLOW.md), [U2 LinkPort 원문](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)을 변경하지 않는다. Node의 실제 typed Source와 `CanRawMessage`는 명시 field binding이며 raw cast가 아니다. Permission composer/150 ms 원 producer·기한/coherence, C2 unit/enum/target header는 미확인이므로 제품 기능 사용 경로의 NOT_READY/BLOCKED_BY가 유지된다.

U3 `u3_raw_decode/encode/format`는 그대로 diagnostic 참고·재현 경계이고 이 Service에서 제품 TX adapter로 자동 호출하지 않는다. [Codec](../../04_PROTOCOL_CODEC/10_CAN_CODEC/00_MODULE_MAP.md)는 I1 두 CAN encode/decode를 재사용한다. UART, 전체 CAN schema, WINDOW runtime consumer, 기능 ALLOW/DENY·CLEAR/DONE/FAILED 합성은 없다.

<a id="future-discrete"></a>
## Current와 future discrete

| 종류 | 이번 설계의 처리 / 후속 제한 |
|---|---|
| 두 current kind | 아직 Service 소유 pending만 전체 교체. Port accepted 뒤 원 bytes는 다른 객체이며 불변 |
| 미지원 kind / Event·Command·Result·Query | `CAN_UNSUPPORTED`를 copy 전 반환, caller identity·저장소 유지. 이번 runtime queue/API/Wire를 새로 만들지 않음 |
| 미래 discrete 확장 조건 | U6 schema 승인 후 bounded FIFO·identity/body·required failure sink를 admission 전에 확보; full은 NO_CAPACITY, silent overwrite 없음 |
| 미래 ordered Query 조건 | current replace와 별개 순서·문맥·terminal count를 보존; U4D 두 프로파일에 Query 구현/재시도 승인 없음 |

모든 19 OPEN을 보존한다. 특히 `OPEN-PERM`, `OPEN-PROVENANCE`, `OPEN-UNIT`, `OPEN-ENUM`, `OPEN-C2-SOURCE`, `OPEN-WIRE`, `OPEN-TIME`, `OPEN-PORT-CAPABILITY`, `OPEN-SDK`, `OPEN-HW`, `OPEN-PEER-OWNER`, `OPEN-WINDOW-DEFERRED`의 owner·필요 증거는 기존 U2/팀 OPEN에 연결한다. 구조·고정 저장·FSM·Mock 관측 설계는 완료하며 실제 binding/정책 채택은 별도이다.
