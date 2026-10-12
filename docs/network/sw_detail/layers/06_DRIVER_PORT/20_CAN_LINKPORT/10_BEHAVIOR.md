# CAN 논리 LinkPort — 함수별 동작

[모듈/소유 타입](00_MODULE_MAP.md) · [의사코드](20_PSEUDOCODE.md) · [후보 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [TX/RX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

모든 원형은 **PROPOSED_PRIVATE / DESIGN_ONLY / BINDING_OPEN / NOT_COMPILED**다. 공통 타입은 [Transport 정본](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#shared-types), Port 소유 상태는 [전체 선언](00_MODULE_MAP.md#port-owned-types), ISR/동기 callback은 [capture 계약](00_MODULE_MAP.md#capture-contract)을 따른다. 함수는 동일 owner가 직렬 호출하며 콜백/ISR에서 호출하지 않는다. Service sink CALLBACK의 정상 진입은 Transport owner_guard=0과 Port owner_active=0을 요구하고, sink 직전 owner_guard=3/owner_active=1을 전파하여 네 Port 함수 직접 재진입도 CAN_REENTRANT로 막는다. sink 정상 반환 후 두 guard는 0으로 복원한다. `owner_active`는 다중 Task 안전성을 새로 보장하지 않는다. backend glue의 bounded atomic publish/consume/reset exclusion은 OPEN-PORT-CAPABILITY의 실제 검증 항목이다.

## 내부 결과와 기존 공개 결과

CAN_ACCEPTED는 Port 소유 copy/token 수용뿐이다. CAN_OK는 init/reset/사건 복사 같은 로컬 처리이며 TX complete가 아니다. CAN_BUSY/NO_CAPACITY/NOT_READY는 preaccept NO_START와 구분되고 기존 accepted를 취소하지 않는다. 공개 `NET_SUBMIT_OK`는 상위 Service 안정 인계에서만 매핑하며 Port 반환을 공개 API에 그대로 노출하지 않는다. `CanTxResult` COMPLETE/CONFIRMED_FAIL/UNKNOWN은 Vehicle DONE/FAILED/ALLOW가 아니다. private enum을 Wire 값이나 C2 enum 숫자로 cast하지 않는다.

<a id="fn-u4d-can-port-init"></a>
## FN-U4D-CAN-PORT-INIT — CanLinkPort_Init

```c
CanCoreStatus CanLinkPort_Init(CanLinkPort *self, const CanPortConfig *config, CanPortStorage *storage, CanClockStamp now);
```

[동일 의사코드](20_PSEUDOCODE.md#fn-u4d-can-port-init) · [Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

| 인자·반환 | 방향·수명 | 유효 조건 |
|---|---|---|
| `CanLinkPort *self` | IN/OUT, caller static instance | NULL 금지. cold/등록 hook 없음 근거가 있는 zero state만. 재초기화로 generation 1을 재사용하지 않음 |
| `const CanPortConfig *config` | IN borrowed, 성공 시 값 copy | 모든 bool 0/1, counts nonzero, clock gap/budget 범위. 호출 반환 후 config pointer 보관 안 함 |
| `CanPortStorage *storage` | IN borrowed view, 성공 시 값 copy | tx/rx backing이 NULL 아니고 counts 일치. backing은 instance 및 등록 hook 종료까지 독점 생존·서로 겹치지 않음 |
| `CanClockStamp now` | IN value | valid=0/1. ready에는 valid clock과 epoch continuity 근거 필요. clock 부재여도 unready 진단 instance 초기화 가능 |
| `CanCoreStatus` | 동기 결과 | OK는 구조 초기화. 실제 Port 준비/타깃 인증을 뜻하지 않음 |

init은 argument/count/budget을 검증하고 생애 검사 PI03에서 owner_active를 initialized보다 먼저 검사하여 CALLBACK 중에는 CAN_REENTRANT로 거부한다. 모든 검증을 통과한 뒤만 self/storage를 commit한다. 실패는 두 객체의 기존 내용 불변이다. guard가 IDLE인 cold quiescence=0 또는 기존 initialized instance는 NOT_READY로 남긴다. slot serial high-water=0, 상태 FREE, ring/counter=0, backend generation=1이다. initial ready는 HOST_PROFILE_ONLY·FD metadata·정직한 quiescence·horizon_proven 외부 증거·valid now를 모두 요구하고 TARGET_UNBOUND에서는 false다. Port/Transport가 새 pair인지와 이전 issuer의 generation 공간이 끝났는지에 관한 수명 근거는 Init 사전조건이며 storage 교체로 serial=0을 무근거 재생성하지 않는다.

| 분기 ID | 조건 | 상태·rollback·관측 | 반환 | 후속 U4C Test |
|---|---|---|---|---|
| PI01 | NULL, bool/enum/count/backing/alias precondition 오류 | 새로운 저장/상관 없음, 객체 불변 | CAN_INVALID_ARG | CAN-D03, CAN-D22 |
| PI02 | SIZE_MAX 곱/합 overflow, budget 부족, gap=0 또는 gap≥2^31 | 초기화 전 거부 | CAN_INVALID_ARG | CAN-D05, CAN-D13 |
| PI03 | owner_active=1을 우선 검사, 이후 이미 initialized 또는 cold quiescence 증거 없음 | 재진입 거부, 기존 hook/slot 수명을 건드리지 않음 | CAN_REENTRANT 또는 CAN_NOT_READY | CAN-D11, CAN-D19 |
| PI04 | 검증 성공 | cfg/view copy, FREE/high-water0/ring0, gen1, 조건부 ready | CAN_OK | CAN-D17, CAN-D22 |

**시간·OPEN:** now는 같은 노드 Infra stamp며 remote clock이 아니다. actual max gap, static memory budget, atomic copy, FD metadata는 `OPEN-TIME/SDK/HW/PORT-CAPABILITY`; Host 구성이 실 S32K 능력을 승인하지 않는다. raw/wire/permission 합의를 init flag로 닫지 않는다. 시험 상태는 DESIGN_ONLY / NOT_EXECUTED다.

<a id="fn-u4d-can-port-try-tx"></a>
## FN-U4D-CAN-PORT-TRY-TX — CanLinkPort_TryTx

```c
CanCoreStatus CanLinkPort_TryTx(CanLinkPort *self, const CanFrame *frame, CanTxToken token, CanClockStamp now);
```

[동일 의사코드](20_PSEUDOCODE.md#fn-u4d-can-port-try-tx) · [TX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

| 인자·반환 | 방향·수명 | 유효 조건 |
|---|---|---|
| `self` | IN/OUT, Port instance | initialized, 같은 owner 직렬 호출, tx/rx backing 지속 생존 |
| `const CanFrame *frame` | IN readonly borrowed, 호출 동안만 | 정확한 두 profile ID 0x0A0/0x080, IDE=0,FDF=BRS=1,RTR=0,DLC=11,length=20. reserved raw 값 검사는 Codec 책임. Port-owned backing과 alias 없음 |
| `CanTxToken token` | IN value full tuple | backend_generation=current P generation, slot<NT, serial>slot.highest_serial, serial≠0. T가 serial 발급; Port가 wire BOOT/TX/UPDATE를 만들지 않음 |
| `CanClockStamp now` | IN value | valid 동일 local epoch, last_progress 대비 delta<2^31·configured proven gap 이하. age/deadline 숫자 승인 아님 |
| `CanCoreStatus` | 동기 admission | ACCEPTED 때 Port owned frame/token 존재. NO_START 확인 실패 때만 rollback·동기 거부 |

입력 검사→ready/time/token 검사→지정 slot reserve→20 B와 전체 메타/token 독립 copy→backend bounded try 한 번→accepted commit 순서다. slot index는 T/P가 공유하고 Port의 별도 대기 FIFO는 없다. BUSY/NO_CAPACITY는 old accepted 또는 UNKNOWN_PINNED 자료를 덮지 않는다. backend callback이 try 반환 전에 오면 RESERVED의 terminal cell에 latch하며 owner dispatch는 하지 않는다. accepted commit 후에만 poll에서 terminal을 인수한다.

backend NO_START가 확정되고 동기 terminal latch도 없다면 copy를 무효화하고 slot FREE로 rollback한다. highest_serial은 증가하지 않는다. 반대로 MAY_START이거나 rejection과 terminal latch가 모순되면 완성된 Port copy/token을 accepted로 commit한다. 첫 terminal이 이미 있다면 result/cause/time/release_safe를 보존하고, 없을 때만 retained UNKNOWN(cause NOT_READY)·release_safe=0을 만든다. admission을 중지하고 세대 fence/reset을 요청한다. CAN_ACCEPTED 반환을 통해 T도 token을 보존하고 첫 terminal을 정확히 한 번 관측한다. admission 자체를 전송 성공으로 주장하지 않는다. 실제 backend 관측은 `CanPortAdmissionObservation`의 status와 no_start_proven 값을 생산하며 이는 사적 binding 계약이다. vendor API가 이 규약을 충족하지 못하면 사전에 NOT_READY로 차단한다.

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

SOURCE_AGE·UPDATE를 Port가 수정하지 않는다. accepted 이후 encoded AGE에 미계상인 Port residence/물리 hop은 Service 진단의 lower-bound/uncertainty로 남긴다. local TX terminal·CAN ACK로 네 소비자 실제 수신이나 ALLOW를 만들지 않는다. `OPEN-PORT-CAPABILITY/SDK/HW/TIME`는 실제 stable copy·NO_START/terminal/clock proof만 차단하며 논리 소유 FSM은 완료 상태다. `OPEN-WIRE/PERM/PROVENANCE/UNIT/ENUM`은 상위 계약의 미합의로 유지한다.

<a id="fn-u4d-can-port-poll-event"></a>
## FN-U4D-CAN-PORT-POLL-EVENT — CanLinkPort_PollEvent

```c
CanCoreStatus CanLinkPort_PollEvent(CanLinkPort *self, CanPortEvent *out, CanClockStamp now);
```

[동일 의사코드](20_PSEUDOCODE.md#fn-u4d-can-port-poll-event) · [RX/terminal Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

| 인자·반환 | 방향·수명 | 유효 조건 |
|---|---|---|
| `self` | IN/OUT owned instance | initialized, 같은 owner. backend atomic snapshot/capture 규약 충족 |
| `CanPortEvent *out` | OUT caller-owned value | NULL·backing alias 금지. OK에서만 완전한 독립 value copy. EMPTY/거부 때 내용 불변 |
| `CanClockStamp now` | IN value | validity/epoch/gap 검사. clock 부재는 정상/fresh로 보정하지 않으며 진단 사건 자체는 drain 가능 |
| `CanCoreStatus` | 로컬 한 사건 인계 | OK는 event 복사이며 App 처리 성공 아님. EMPTY는 지금 반환할 owned 사건 없음 |

critical-loss/bus-off/clock-break를 인수하면 admissionoff→미확정 accepted UNKNOWN→논리 reset fence를 한 번 진행한다. 이미 complete/confirmed-fail인 terminal은 보존한다. actual quiescence가 관측되기 전 bytes는 회수하지 않는다. generation 증가를 근거로 RX old generation을 격리하되, retained terminal은 현재 generation과 달라도 그 live slot token과 정확히 일치하면 반환한다.

한 호출은 atomic snapshot/sweep≤NT·terminal scan≤NT·RX 한 건 이내이며 최대 한 사건을 반환한다. sticky critical health를 먼저 한 번 인수하고, retained terminal을 round-robin으로 복사한 뒤 RX를 FIFO로 반환한다. 일반 overflow health 변화는 coalesce하여 terminal/RX를 계속 가리지 않는다. 무한 retry·ISR parsing·Node callback 호출은 없다.

terminal copy가 성공한 뒤 complete/fail+release_safe면 FREE, UNKNOWN+release_safe=0이면 UNKNOWN_PINNED다. UNKNOWN이 이미 통지된 slot은 genuine quiescence 뒤만 FREE다. backend UNKNOWN capture는 ready=false·reset pending·sticky health 전이로 들어간다. T가 결과를 소비해 자신의 slot을 FREE로 바꿔도 Port가 같은 pinned index에 계속 BUSY만 반환하며 재시도를 반복하지 않는다. drain/recovery 전 admission이 중지된다. 이미 terminal인 cell은 duplicate/late success로 결과를 바꾸지 않는다. RX overflow는 drop-newest·counter·health 사건이며 가짜 typed callback/기능 FAILED를 만들지 않는다. controller_error≠0 또는 wrong-generation RX는 raw 진단이며 정상 semantic consumer로 보내지 않는다.

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

Port Poll은 source BOOT/TX/UPDATE의 admission이나 specific source liveness를 결정하지 않는다. RX clock invalid/overflow/quality-degraded를 같은 정상 사건으로 보정하지 않는다. 정상 형식의 INVALID/STALE raw quality는 Codec/Service에서 구조 오류와 별개로 처리한다. CAN-D01..22 테스트는 후속 설계 추적이며 실행하지 않았다.

<a id="fn-u4d-can-port-reset"></a>
## FN-U4D-CAN-PORT-RESET — CanLinkPort_Reset

```c
CanCoreStatus CanLinkPort_Reset(CanLinkPort *self, CanResetReason reason, CanClockStamp now);
```

[동일 의사코드](20_PSEUDOCODE.md#fn-u4d-can-port-reset) · [복구 Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

| 인자·반환 | 방향·수명 | 유효 조건 |
|---|---|---|
| `self` | IN/OUT owned instance | initialized, 동일 owner. accepted slot 및 retained terminal 생존 |
| `CanResetReason reason` | IN value | REQUESTED/BUS_OFF/EVENT_LOSS만. Transport watchdog/clockbreak도 REQUESTED 사건으로 요청하며 terminal cause에 실제 time 상태 보존 |
| `CanClockStamp now` | IN value | UNKNOWN 관측 시각. invalid clock도 valid=0 그대로 보존하고 reset 안전 동작은 수행 |
| `CanCoreStatus` | 동기 로컬 reset 상태 | OK는 quiescence 및 논리 정리. NOT_READY는 drain 미확인/세대 한계이며 retained UNKNOWN은 poll 가능 |

backend reset API를 직접 설계하지 않고 논리 순서를 정의한다. admissionoff→미확정 accepted만 UNKNOWN terminal(cause NOT_READY/clock unknown)→full oldtoken retained→generation+1 without wrap→late callback 분리→binding의 취소/drain/quiescence 근거 확인→slot release/recovery 순서다. 이미 terminal complete/fail은 덮지 않는다. RX old generation은 진단 격리하고 source BOOT/UPDATE나 Service pending을 재초기화하지 않는다.

Reset pending에 같은 요청이 반복되어도 fence를 재발급하지 않는다. UINT64_MAX generation에 도달하면 admissionoff를 유지하고 증가하지 않는다. 이 경우 cold 새 instance로 전환할 때도 이전 backend/hooks가 완전히 종료됐다는 별도 근거가 필요하다. 결과 UNKNOWN인 frame을 Port가 자동 재전송하지 않는다. recovery 후 원 평가가 새로 생긴 것처럼 AGE=0·UPDATE+1·ALLOW=1을 합성하지 않는다.

| 분기 ID | 조건 | 상태·rollback·관측 | 반환 | 후속 U4C Test |
|---|---|---|---|---|
| PR01 | NULL/init/reason/valid 표현 오류 | 상태 불변 | CAN_INVALID_ARG | CAN-D03 |
| PR02 | owner_active=1 | 재진입 없음 | CAN_REENTRANT | CAN-D19 |
| PR03 | 첫 reset 요청 | admissionoff, unresolved UNKNOWN, conclusive 결과 유지, bytes pin, reset pending | 분기 계속 | CAN-D09, CAN-D10, CAN-D19 |
| PR04 | 첫 fence 및 generation<UINT64_MAX | generation+1, old token/bytes 보존, 새세대 admission high-water0 | 분기 계속 | CAN-D11, CAN-D13 |
| PR05 | generation 최대값 또는 pending drain 미확인 | wrap 없음, UNKNOWN pin/retained terminal 보존, health pending | CAN_NOT_READY | CAN-D10, CAN-D11 |
| PR06 | 실제 quiescence/old TX flush·hook separation 확인 | 미통지 terminal 보존, 통지 pinned만 FREE, 준비조건에 따른 recovery health | CAN_OK | CAN-D10, CAN-D11, CAN-D15 |

Transport는 accepted watchdog/clock-break에서 직접 T 결과를 합성하지 않고 Reset을 한 번 요청한 뒤 retained terminal을 drain한다. Port timeout owner를 중복하지 않는다. reset의 NOT_READY는 UNKNOWN 통지 차단이 아니다. backend capability/수명/atomicity/실측은 OPEN이며 Host 논리 reset 설계는 완성했다.

## 양방향 추적·설계 경계

모든 함수의 출처 SHA는 [모듈 근거 표](00_MODULE_MAP.md#source-contract), 원 실패 의미는 [공통 API](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md), inherited 두 Flow는 [CIS_REAR](../../../flows/10_CIS_REAR_FLOW.md) / [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)다. 이 문서는 U2의 기능 FN 원형을 변경하지 않는다. CAN-D02/D12/D14/D15/D20/D21의 기능 품질·source age·원 producer 논리는 Service/Node가 소유하며 Port는 raw metadata/bytes와 backend health만 인계한다. future discrete Command/Event/Result는 새 queue·schema로 구현하지 않고 identity 보존/no-silent-overwrite 조건만 상위에 둔다.
