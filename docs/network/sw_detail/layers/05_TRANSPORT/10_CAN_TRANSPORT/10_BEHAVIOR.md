# CAN Transport — Behavior

PROPOSED_PRIVATE / DESIGN_ONLY / NOT_COMPILED. [Map/공통 타입](00_MODULE_MAP.md) · [Behavior](10_BEHAVIOR.md) · [Pseudocode](20_PSEUDOCODE.md) · [CAN 함수 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN Flow/22사례](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

공통 types/state는 Map 정본, 비alias는 init/storage 할당 시 증명하는 계약이며 서로 무관한 C pointer의 순서 비교로 추측하지 않는다. named logical predicates 아래에 정의하며 임의 SDK helper 구현을 전제하지 않는다.

<a id="fn-u4d-can-transport-init"></a>
## FN-U4D-CAN-TRANSPORT-INIT

```c
CanCoreStatus CanTransport_Init(CanTransport *self, const CanTransportConfig *config, CanTransportStorage *storage, CanLinkPort *port);
```

호출자: 초기 Node owner. 반환: `CanCoreStatus`; 구조값 CAN_OK, 수용 CAN_ACCEPTED, empty/fail은 아래분기.

인자/입력: self/port: owner 전용 비NULL, 이미 활성인 instance 재초기화 금지. config/storage: 호출 중만 borrowed; 배열은 instance 수명 동안 독점 제공.

출력/소유/원자성: 설정과 storage view만 복사; 외부 array 소유권은 caller storage provider, 논리 slot 내용은 Transport. 실패 시 기존 instance/배열 바이트 불변.

예정검증: `CAN-D05 CAN-D11 CAN-D17 CAN-D22` DESIGN_ONLY / NOT_EXECUTED. Source: [실제 Source/SHA 및 shared fields](00_MODULE_MAP.md); Flow: [양단 TX/RX](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md); 짝문서: [동일 FN](20_PSEUDOCODE.md#fn-u4d-can-transport-init).

| Branch-ID | 조건/분기 | state·반환·failure owner |
|---|---|---|
| TI01 | NULL/config enum/겹치는 storage | CAN_INVALID_ARG; 쓰기 0 |
| TI02 | 이미 owner API 수행 중 / 활성 instance 재초기화 | guard 우선 CAN_REENTRANT; idle live 재Init은 CAN_NOT_READY; 쓰기 0 |
| TI03 | NT/RT=0, NT>UINT16_MAX, Port NT와 불일치, supplied array 길이 부족 | CAN_INVALID_ARG |
| TI04 | watchdog/관측 horizon 반범위 증명 부재 | CAN_INVALID_ARG; 임의 clock 보정 없음 |
| TI05 | size_t 곱/합 overflow 또는 budget 초과 | CAN_INVALID_ARG; 부분 초기화 0 |
| TI06 | Port 구조 초기화/비0 generation/fresh pair 증명 확인 불가 | CAN_NOT_READY; 과거issuer serial 재사용 금지 |
| TI07 | 사전조건 모두 만족 | CAN_OK; FREE/serial0/RX empty/health snapshot |

<a id="fn-u4d-can-transport-try-tx"></a>
## FN-U4D-CAN-TRANSPORT-TRY-TX

```c
CanCoreStatus CanTransport_TryTx(CanTransport *self, const CanRawMessage *input, CanClockStamp now, CanTxToken *accepted);
```

호출자: 동일 직렬 owner의 Service/worker. 반환: `CanCoreStatus`; 구조값 CAN_OK, 수용 CAN_ACCEPTED, empty/fail은 아래분기.

인자/입력: input: 현재 호출 중 immutable I1 raw envelope. now: 외부 Clock origin. accepted: 비NULL caller output; self와 겹치지 않음.

출력/소유/원자성: CAN_ACCEPTED일 때만 accepted 토큰 출력. 그 외 out 불변. input borrowed pointer 보관 0. Port가 자체 owned copy를 수용해야 T INFLIGHT commit.

예정검증: `CAN-D01 CAN-D02 CAN-D03 CAN-D04 CAN-D05 CAN-D06 CAN-D08 CAN-D09 CAN-D11 CAN-D14 CAN-D22` DESIGN_ONLY / NOT_EXECUTED. Source: [실제 Source/SHA 및 shared fields](00_MODULE_MAP.md); Flow: [양단 TX/RX](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md); 짝문서: [동일 FN](20_PSEUDOCODE.md#fn-u4d-can-transport-try-tx).

| Branch-ID | 조건/분기 | state·반환·failure owner |
|---|---|---|
| TT01 | NULL/미초기화/alias precondition 위반 | CAN_INVALID_ARG |
| TT02 | 동일 owner 재진입 | CAN_REENTRANT |
| TT03 | clock invalid/horizon 유효성 미증명 | CAN_CLOCK_UNKNOWN; out/slot 불변; epoch연속성은 Port owned anchor 검사 |
| TT04 | Port health admission 불가/generation0 | CAN_NOT_READY; out 불변 |
| TT05 | FREE slot 없음/serial 소진 | CAN_NO_CAPACITY 또는 CAN_NOT_READY; wrap 없음 |
| TT06 | Codec 실패 | 정확한 codec.status+I1진단 retained; slot/out 불변 |
| TT07 | 검증 성공 → bounded reservation | serial++ 단조; RESERVED + owned20B + candidate token |
| TT08 | P 명백한 미수용 | CAN_BUSY/NOT_READY/NO_CAPACITY 등 그대로; FREE rollback, accepted out 불변 |
| TT09 | P owned copy + fulltoken 수용 | CAN_ACCEPTED; T INFLIGHT; caller out 마지막 commit |

<a id="fn-u4d-can-transport-pump"></a>
## FN-U4D-CAN-TRANSPORT-PUMP

```c
CanCoreStatus CanTransport_Pump(CanTransport *self, CanClockStamp now, uint16_t budget);
```

호출자: 동일 직렬 owner의 Service/worker. 반환: `CanCoreStatus`; 구조값 CAN_OK, 수용 CAN_ACCEPTED, empty/fail은 아래분기.

인자/입력: now: caller Clock, budget>0 유한 event 수. owner는 Service Pump 전에 호출한다.

출력/소유/원자성: 최대 budget개 Port event를 인수하며 slot terminal이 유일한 outcome mailbox. 모든 INFLIGHT NT 유한 scan; 실제 deadline은 watchdog config의 local 진단 정책.

예정검증: `CAN-D08 CAN-D09 CAN-D10 CAN-D11 CAN-D13 CAN-D16 CAN-D17 CAN-D19 CAN-D22` DESIGN_ONLY / NOT_EXECUTED. Source: [실제 Source/SHA 및 shared fields](00_MODULE_MAP.md); Flow: [양단 TX/RX](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md); 짝문서: [동일 FN](20_PSEUDOCODE.md#fn-u4d-can-transport-pump).

| Branch-ID | 조건/분기 | state·반환·failure owner |
|---|---|---|
| TP01 | NULL/미초기화/zero budget | CAN_INVALID_ARG |
| TP02 | 재진입 | CAN_REENTRANT |
| TP03 | Port EMPTY 또는 NOT_READY지만 terminal cell 없음 | drain 종료; 상태 소실 0 |
| TP04 | Port 구조 fault | CAN_NOT_READY; P Reset 사건 요청, 인정 못한 event 폐기 |
| TP05 | Health | ready/generation sticky snapshot만 인수; T conclusive 결과 합성/덮기 0 |
| TP06 | TX terminal fulltoken가 live INFLIGHT와 일치 | old generation이라도 정확한 slot token은 수용, outcome1개 retained |
| TP07 | 중복/late/unmatched terminal | sat diagnostic count; 새 slot/result 불변 |
| TP08 | RX controller error/old generation 또는 full | last raw+error snapshot quarantine/drop-new count; peer/App fault 합성 0 |
| TP09 | 현재 generation RX, RT 여유 | caller-return 전 owned record 복사, ring count commit |
| TP10 | 유효 local delta watchdog 만료/epoch break/invalid clock | P Reset 1회 요청; UNKNOWN 직접 합성 금지; 완료 cell 보존 |
| TP11 | bounded cycle 종료 | CAN_OK; timeout 이후 terminal은 다음/이번 정상 Poll로 인수 |

<a id="fn-u4d-can-transport-take-outcome"></a>
## FN-U4D-CAN-TRANSPORT-TAKE-OUTCOME

```c
CanCoreStatus CanTransport_TakeOutcome(CanTransport *self, CanTxOutcome *out);
```

호출자: 동일 직렬 owner의 Service/worker. 반환: `CanCoreStatus`; 구조값 CAN_OK, 수용 CAN_ACCEPTED, empty/fail은 아래분기.

인자/입력: out: caller stack/value storage 비NULL, Transport storage와 비중첩.

출력/소유/원자성: CAN_OK 시 mailbox를 atomic value copy한 뒤 해당 T slot FREE. P unknown bytes 별도PINNED라 T reuse가 그 바이트를 해제하지 않는다.

예정검증: `CAN-D08 CAN-D09 CAN-D10 CAN-D11 CAN-D19` DESIGN_ONLY / NOT_EXECUTED. Source: [실제 Source/SHA 및 shared fields](00_MODULE_MAP.md); Flow: [양단 TX/RX](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md); 짝문서: [동일 FN](20_PSEUDOCODE.md#fn-u4d-can-transport-take-outcome).

| Branch-ID | 조건/분기 | state·반환·failure owner |
|---|---|---|
| TO01 | NULL/미초기화/owned alias | CAN_INVALID_ARG; out 불변 |
| TO02 | 재진입 | CAN_REENTRANT |
| TO03 | OUTCOME_READY 없음 | CAN_EMPTY; out 불변 |
| TO04 | retained outcome 존재 | CAN_OK; copy→FREE 단일 commit |

<a id="fn-u4d-can-transport-take-rx"></a>
## FN-U4D-CAN-TRANSPORT-TAKE-RX

```c
CanCoreStatus CanTransport_TakeRx(CanTransport *self, CanRxRecord *out);
```

호출자: 동일 직렬 owner의 Service/worker. 반환: `CanCoreStatus`; 구조값 CAN_OK, 수용 CAN_ACCEPTED, empty/fail은 아래분기.

인자/입력: out: caller-owned CanRxRecord 비NULL, ring과 비중첩.

출력/소유/원자성: CAN_OK일 때만 deep value copy + pop. reset 이후 ring oldgeneration은 정상 RX 반환하지 않으며 bounded drop count.

예정검증: `CAN-D03 CAN-D12 CAN-D13 CAN-D16 CAN-D18 CAN-D22` DESIGN_ONLY / NOT_EXECUTED. Source: [실제 Source/SHA 및 shared fields](00_MODULE_MAP.md); Flow: [양단 TX/RX](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md); 짝문서: [동일 FN](20_PSEUDOCODE.md#fn-u4d-can-transport-take-rx).

| Branch-ID | 조건/분기 | state·반환·failure owner |
|---|---|---|
| TR01 | NULL/미초기화/owned alias | CAN_INVALID_ARG |
| TR02 | 재진입 | CAN_REENTRANT |
| TR03 | old generation retained records | 최대 RT개 discard; return out 불변 |
| TR04 | empty | CAN_EMPTY |
| TR05 | 유효 oldest RX | CAN_OK; record copy→pop |

## 비교식·오류 정의

`active_reinit`: 이미 initialized=1이거나 live frame/storage가 존재한다. self는 최초 호출 전 zero-initialized storage다. `storage_overlap/invalid_owned_alias`: owner가 배열/출력 주소영역을 생성할 때 증명하는 비중첩 계약이다. 서로 무관한 C 포인터의 순서 비교로 판단하지 않는다. `NT/RT/watchdog_ms/max_gap_ms`: config의 해당 필드를 읽은 local 값이다. `checked_sum_overflow_or_budget_exceeded`: 각 곱셈 전에 count>SIZE_MAX/sizeof(element), 각 덧셈 전에 a>SIZE_MAX-b를 검사하여 총합>memory_budget_bytes도 거부한다.

`clock_invalid_or_different_epoch_or_gap_unproven`: now/accepted_at valid!=1, epoch불일치, 외부 finite horizon 증거 부재, 또는 accepted_at→now unsigneddelta≥2^31 중 하나다. accepted lifetime 총기간을 max_observation_gap_ms와 비교하지 않는다. max 관측간격은 Port.last_progress 기반 연속 호출 검사에 적용한다. T config max_observation_gap_ms는 Port.config.max_clock_gap_ms와 같은 owner 호출 간격 상한이며 Init에서 일치해야 한다. 설정 horizon_proven은 매 호출의 외부 Clock 연속성 계약이며 raw modulo 결과만으로 증명할 수 없다. Boot/source clock binding 숫자를 확정하지 않는다. `all_three_token_fields_equal`: generation==storedgeneration && slot==storedslot && serial==storedserial. `saturating_increment(x)`: x==UINT32_MAX면 유지, 그 외 x+1. `kind/source_update`: Codec 검증된 kind의 I1 named update_sequence 필드다. 다른 union arm을 읽지 않는다.

Pseudo FREE/RESERVED/INFLIGHT/OUTCOME_READY는 Map CAN_T_* 상태, IDLE/TX/PUMP는 owner_guard 0/1/2에 대응한다. 모든 배열 순회는 NT/RT 이내다. Pump 단계 별 predicate는 명시한 단일 owner에서만 평가한다. TX deadline 때 P Reset이 이미 conclusive terminal을 보존하므로 다음 Poll이 해당 terminal을 인수한다. 같은 token에 UNKNOWN이 먼저 소비됐으면 후속 success는 진단만 남긴다. Reset 요청이 quiescence를 아직 증명하지 못했어도 retained terminal Poll은 허용한다.

Port와 Transport는 일대일 token issuer pair다. 최초 backend startup quiescence로 새 generation 공간을 증명하거나, 이전 issuer 수명 종료 뒤 Port Reset으로 generation을 증가시키고 old terminal/borrow 종료를 증명한 경우만 generation_fresh_for_pair=1이다. 새 T object/serial0를 기존 P generation 공간에 붙이는 초기화는 NOT_READY다. 활성 T 재초기화 금지이며 serial은 slot payload 정리 때 유지한다. 무근거 generation1 재설정은 허용하지 않는다.

TryTx clock epoch/gap검사는 Port last_progress에위임, accepted후watchdog는 Tslot.accepted_at 기준이다. 저장없는Tclockorigin을가정하지않는다.

Service의 diagnostic callback 전후에는 동일한단일owner가 T.owner_guard=CALLBACK(3), P.owner_active=1을설정/원상복원하여상태보유Svc/T/P privateAPI 재진입을거부한다. 이때만Service는lowerguard를갱신하는명시적owner예외를갖는다. Codec stateless value함수는공유mutablestate가없으므로상태guard추가없으며noalias/scratch계약을따른다.
