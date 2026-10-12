# CAN Message Service — 함수별 동작

[↑ Module](00_MODULE_MAP.md) · [전체 상태/필드](00_MODULE_MAP.md#service-types) · [공통 Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md) · [후보 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / BINDING_OPEN / NOT_COMPILED.** 다섯 함수는 새로운 공개 API가 아닌 U4C용 내부 후보이다. 아래 CAN-Dxx는 모두 **DESIGN_ONLY / NOT_EXECUTED**인 후속 Host Mock 계획이다. 공통 타입·enum은 [Transport Map](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#shared-types), 전체 Source 경로·SHA는 [근거 표](00_MODULE_MAP.md#service-source)를 따른다.

## 모든 호출에 적용할 정확한 의미

한 owner가 superloop 또는 worker Task에서 Core를 직렬 호출한다. Node ingress는 기존 bounded 인계 경계를 사용하며 여러 Task가 직접 Service를 동시에 호출하지 않는다. ISR은 Service를 호출하지 않는다. owner는 각 stamp의 valid/epoch와 실제 비교 구간이 반범위 미만이라는 외부 Clock 근거를 제공한다. `max_observation_gap_ms`는 마지막 Service owner 관측→현재 now의 연속 poll 간격에만 적용한다. 원 AGE anchor가 오래되었다는 이유로 이 한계를 적용하지 않는다. 별도 `source_horizon_ms[k]`는 해당 source의 TX/UPDATE 전진이 각각 반범위 미만인 rate/window 근거이다. `external_horizon_proven=1`은 이 두 외부 근거의 전제이지 SDK 증거를 만드는 flag가 아니다.

원 AGE 로컬 차이 `d=(uint32_t)(now.ms-anchor.ms)`는 두 stamp valid=1, 동일 epoch, 실제 비교 구간의 외부 finite horizon 및 `d<0x80000000`일 때만 사용한다. 마지막 owner 관측 gap 단절은 기존 pending/tx-origin AGE anchor를 invalid, RX AGE를 UNCERTAIN으로 남긴다. 이후 정상 poll은 과거 anchor를 치유하지 않는다. 진짜 newer UPDATE의 별도 source-order 근거와 truthful accounted_at이 유효하면 새 AGE baseline을 수용할 수 있다. 여러 번 wrap/epoch 단절/근거 없는 gap은 CLOCK_UNKNOWN이다. 순서 `d=(uint32_t)(incoming-last)`는 입증된 source horizon 안에서 `0<d<0x80000000`만 newer, 0은 duplicate, 반범위 이상은 old/ambiguous로 격리한다. 원 AGE 합산은 uint64 scratch 후 u16 포화하며, 65535는 원 경과의 하한이고 fresh 상한이 아니다.

**원 identity 본문 동등성:** rear는 `rear_distance_deci_cm/validity/quality_reason/proximity_status` 네 필드, permission은 `vehicle_power_permission/power_permission_quality/power_operation_state/operation_allowed/operation_quality` 다섯 필드를 각각 비교한다. Boot는 먼저 문맥 확인, UPDATE는 먼저 identity 확인한다. TX와 AGE 및 struct padding은 본문 비교에서 제외한다. 예약 코드 검사는 I1의 정확한 허용 범위만 사용하며 새 물리거리/coherence 규칙을 만들지 않는다.

사적 Submit 결과의 기존 Node mapping은 ACCEPTED→NET_SUBMIT_OK, NO_CAPACITY→NET_SUBMIT_NO_CAPACITY, NOT_READY/CLOCK_UNKNOWN/ORDER_REJECT/UNSUPPORTED/REENTRANT→NET_SUBMIT_NOT_READY, INVALID_ARG/CODEC_REJECT/ORIGIN_CONFLICT→NET_SUBMIT_INVALID_ARGUMENT이다. 이 mapping은 stable **Submit admission**에만 적용한다. Pump의 Port accepted·terminal은 과거 Node 반환을 재작성하지 않는다. 기존 C2 mapping은 [원 반환표](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-return-status) 그대로이고 숫자 enum cast는 없다.

<a id="fn-u4d-can-svc-init"></a>
## FN-U4D-CAN-SVC-INIT — CanSvc_Init

```c
CanCoreStatus CanSvc_Init(CanService *self, const CanSvcConfig *config, CanTransport *transport);
```

[의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-init) · [색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md#fn-u4d-can-svc-init) · [메모리 식](00_MODULE_MAP.md#service-memory)

호출자는 Node runtime owner, 문맥은 quiescent owner 초기화이다. `self`는 caller-owned, zero-initialized 전체 CanService 저장소이며 NULL 불가이다. `config`는 const borrowed IN, Init 반환 전 전체 값 복사, range는 Map 필드표 그대로이다. `transport`는 caller-owned initialized Transport의 안정 주소, Init 때 RX/TX 작업이 없고 Service lifetime 동안 유효해야 한다. out 인자는 없다. 실패 시 기존 self bytes를 보존하고 CAN_OK는 instance 설정 완료만 증명한다.

| 분기 | 조건 → 처리·결과 | U4C 계획 |
|---|---|---|
| SI01 | NULL 인자 → INVALID_ARG, 변경 없음 | CAN-D03 |
| SI02 | guard≠IDLE → REENTRANT; live Service 재Init → NOT_READY (quiescent owner는 외부 호출 전제), reset 대용 사용 금지 | CAN-D10, CAN-D11, CAN-D19 |
| SI03 | Boot0/capacity1..2 밖/mask 불법/owner gap 또는 source horizon 0·반범위 이상/proof 미확인 → INVALID_ARG | CAN-D05, CAN-D13, CAN-D22 |
| SI04 | 보수적 memory 합산 overflow 또는 budget 미달 → INVALID_ARG | CAN-D05, CAN-D22 |
| SI05 | initialized Transport 준비 전 → NOT_READY, borrowed 주소 commit 없음 | CAN-D05 |
| SI06 | 잠정 zero snapshot에 config·transport·tx_next[2]=1·guard를 채운 뒤 전체 commit → OK | CAN-D01, CAN-D17, CAN-D22 |

Source: [U2 PublishRear](../../02_APPLICATION_PORT/CIS/10_BEHAVIOR.md#fn-cis-cisnet_publishrear), [U2 Permission Publish](../../01_NODE_COMMUNICATION/DOMAIN_PRIVATE/10_BEHAVIOR.md#fn-domain-domainnet_publishpermission), 기존 두 Flow. `OPEN-SDK/HW/TIME/PORT-CAPABILITY`는 타깃 설정 실증을 차단하지만 이 고정 Host 상태 초기화 설계를 막지 않는다.

<a id="fn-u4d-can-svc-submit"></a>
## FN-U4D-CAN-SVC-SUBMIT — CanSvc_Submit

```c
CanCoreStatus CanSvc_Submit(CanService *self, const CanRawMessage *input, CanClockStamp age_accounted_at, CanClockStamp now);
```

[의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-submit) · [색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md#fn-u4d-can-svc-submit) · [current/discrete](00_MODULE_MAP.md#future-discrete)

기존 Node-private binding/진단 Host caller가 owner IDLE 문맥에서 호출한다. `self`는 IN/OUT owner instance, `input`은 const IN 호출 동안 borrowed이며 self 저장소와 overlap 불가이다. key는 두 kind뿐, 모든 원 fields는 I1 raw struct 그대로이고 input Boot는 local_boot와 같아야 한다. input TX는 Network-owned 순번이 아니므로 실제 TX에서 Service가 key별 counter로 채운다. `age_accounted_at`은 input AGE에 이미 포함된 마지막 **이 노드 로컬** 기준점, `now`는 호출 시점 로컬 stamp이며 두 by-value IN은 config horizon을 충족해야 한다. 없는 producer 근거·원 AGE를 임의 stamp로 보완하지 않는다.

정상 흐름은 구조검사→같은 원 UPDATE 비교/AGE watermark→pending 위치 확보→전체 원 사본과 AGE 기준점 잠정 준비→pending와 tx_origin의 같은 owner commit이다. callback·Port는 이 함수에서 호출하지 않는다. ACCEPTED는 Service의 caller-independent 안정 접수만 증명한다. cap1에서 다른 key가 차지하면 NO_CAPACITY; matching current가 있으면 전체 교체 가능하다. 이미 accepted된 Transport/Port storage는 별개이므로 절대 수정하지 않는다.

| 분기 | 조건 → 처리·결과 / rollback | U4C 계획 |
|---|---|---|
| SS01 | NULL/clock.valid 표현 범위 밖/미초기화/guard≠IDLE → INVALID_ARG/NOT_READY/REENTRANT, 접수 없음 | CAN-D03, CAN-D19 |
| SS02 | 두 kind 이외 → UNSUPPORTED, future discrete identity는 caller에 남음 | CAN-D07 |
| SS03 | tx mask 미지원 또는 nonzero input Boot≠local_boot → NOT_READY; Boot0는 SS04 reserved 오류 | CAN-D05, CAN-D11, CAN-D22 |
| SS04 | I1 raw reserved/code 범위 오류 → CODEC_REJECT; 정상 INVALID/STALE/NO_DATA는 보존 | CAN-D02, CAN-D04, CAN-D18 |
| SS05 | owner clock progress 검사와 accounted stamp→now 경과 입증 불가 → CLOCK_UNKNOWN; 단절은 과거 AGE anchor invalid, 새 pending/원 관측 commit 없음 | CAN-D13, CAN-D14 |
| SS06 | 이전 원 관측→now의 source horizon/UPDATE 순서 불명확 또는 old → CLOCK_UNKNOWN/ORDER_REJECT, 기존 pending·watermark 보존 | CAN-D13, CAN-D15 |
| SS07 | 같은 UPDATE의 본문 값/quality 충돌 → ORIGIN_CONFLICT, 기존 자료 보존 | CAN-D12, CAN-D15 |
| SS08 | 같은 UPDATE 재제출 → retained AGE anchor 유효할 때만 동일 시점 incoming/retained max; 미확인 old anchor는 CLOCK_UNKNOWN이며 재초기화 금지 | CAN-D14, CAN-D15 |
| SS09 | matching current도 free slot도 없음 → NO_CAPACITY, 이전 자료·accepted TX 보존 | CAN-D05, CAN-D06 |
| SS10 | 원 사본·계상 AGE·now anchor를 준비하여 pending+watermark commit → ACCEPTED; 교체 count는 실제 교체에만 증가 | CAN-D01, CAN-D02, CAN-D06, CAN-D21 |

오류 경로에는 확보한 비동기 자원이 없다. 잠정 객체는 stack에서 폐기하며 원 pending의 partial write는 없다. counters는 관측에 맞게 u32 포화한다. `OPEN-PERM/PROVENANCE`가 unresolved인 제품 `DomainNet_PublishPermission` ALLOW/근거 없는 합성은 상위 기존 함수에서 NOT_READY이며, 이 private raw 진단 제출이 그 경로를 우회하지 않는다. mm/enum은 변환하지 않는다. Source: [CIS 원 Data](../../../../interface/02_NODES/CIS/10_DATA.md#type-netcisrear), [공통 AGE](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md#section-quality-freshness), [U2 Permission composer](../../../flows/20_DOMAIN_PERMISSION_FLOW.md#composer-boundary).

<a id="fn-u4d-can-svc-pump"></a>
## FN-U4D-CAN-SVC-PUMP — CanSvc_Pump

```c
CanCoreStatus CanSvc_Pump(CanService *self, CanClockStamp now, uint16_t budget);
```

[의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-pump) · [색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md#fn-u4d-can-svc-pump) · [Transport](../../05_TRANSPORT/10_CAN_TRANSPORT/10_BEHAVIOR.md)

owner가 같은 cycle에서 Transport_Pump를 먼저 호출한 뒤 이 함수를 호출한다. Service는 Transport_Pump를 다시 호출하지 않는다. `self` IN/OUT, `now` by-value IN, `budget` IN `3..UINT16_MAX`, out 없음이다. phase quota로 outcome/RX/TX progress를 각각 보장하며 실제 시간 상한 수치는 OPEN이다. inherited borrowed callback은 CALLBACK guard, 모든 external API 재진입은 REENTRANT이다. Pump→OnDecoded에는 일회 dispatch_permit를 쓰고 callback 이전 소비한다.

Outcome는 TakeOutcome에서 성공 사본을 얻을 때만 한 번 cache/count한다. 같은 token의 duplicate/late terminal을 별도 재실행하지 않으며 Port/Transport correlation이 판별한다. UNKNOWN은 이 Service가 직접 만들지 않는다. 이미 accepted된 frame을 automatic retry하지 않고, pending을 다음 owner cycle에서 다시 시도하는 것은 아직 미수용인 intent의 진행이다. reset/backend generation이 바뀌어도 source Boot/UPDATE/AGE를 새로 만들지 않는다.

| 분기 | 조건 → 처리·결과 / 자원 | U4C 계획 |
|---|---|---|
| SP01 | NULL/clock.valid 표현 범위 밖/미초기화/guard 재진입/budget<3 → INVALID_ARG/NOT_READY/REENTRANT, 작업 없음 | CAN-D17, CAN-D19 |
| SP02 | 유효 인자 → guard=PUMP, quota 계산 및 owner clock progress 검사; 모든 반환 경로에서 permit0/guardIDLE | CAN-D17, CAN-D22 |
| SP03 | bounded TakeOutcome 성공 → kind별 원 token/update/result 사본 cache, 실제 complete/fail/unknown counter만 증가 | CAN-D08, CAN-D09, CAN-D11 |
| SP04 | TakeOutcome EMPTY → phase 종료; 비empty 오류 → 실제 진단 보존, 무한 retry 없음 | CAN-D05, CAN-D19 |
| SP05 | bounded TakeRx 성공 → owned local 사본에서 Codec_Decode 한 번 | CAN-D03, CAN-D04, CAN-D16 |
| SP06 | Codec reject → exact status/I1/i1_called와 원 CanRxRecord를 bounded 최근 1개에 복사·격리, consumer/liveness commit 없음 | CAN-D04, CAN-D18 |
| SP07 | Codec 성공 → permit1 후 OnDecoded 내부 호출, 결과 보존; callback 반환 뒤 RX local 사본 폐기 | CAN-D12, CAN-D18, CAN-D20, CAN-D21 |
| SP08 | RX EMPTY/오류 → 해당 bounded phase 종료, 원 상태 보존 | CAN-D16, CAN-D19 |
| SP09 | Port mirror 미준비/bus-off/critical_loss → TX 시도 중지, pending 유지 | CAN-D05, CAN-D10, CAN-D11 |
| SP10 | pending→now AGE anchor invalid/경과 미입증 → 해당 pending UNCERTAIN/진단, TX 없음; 0/65535 창조 없음 | CAN-D13, CAN-D14 |
| SP11 | key별 TX scratch에 local Boot/tx_next/누적 AGE만 보완 후 TryTx 한 번; ACCEPTED는 pending 해제+해당 tx_next 증가, BUSY/NO_CAPACITY/NOT_READY는 pending 보존 | CAN-D05, CAN-D06, CAN-D08, CAN-D10, CAN-D14, CAN-D15 |
| SP12 | 최대2 key round-robin 종료 → permit0/guardIDLE. 적어도1 Port 수용은 ACCEPTED, 그 외 소비 progress OK, 무작업 EMPTY, 실패 단독은 실제 원인 | CAN-D17, CAN-D19, CAN-D22 |

Port accepted 뒤 bytes가 고정되므로 그 뒤 controller 대기/실TX/RX 구간은 Wire AGE에 소급 더할 수 없다. RX에서는 수치 하한/불확정 메타로 남긴다. 5 ms 홉·50/150 ms source 기한을 근거 없이 확정하지 않는다. Transport timeout/clock-break는 Port reset 요청으로 불확정 terminal을 보존한 뒤 TakeOutcome로만 인수한다. Source: [U2 다섯 성공 경계](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md#outcome-boundaries), [공통 health](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-network-health).

<a id="fn-u4d-can-svc-on-decoded"></a>
## FN-U4D-CAN-SVC-ON-DECODED — CanSvc_OnDecoded

```c
CanCoreStatus CanSvc_OnDecoded(CanService *self, const CanRawMessage *decoded, const CanRxRecord *rx, CanClockStamp now);
```

[의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-on-decoded) · [색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md#fn-u4d-can-svc-on-decoded) · [RX source 문맥](00_MODULE_MAP.md#service-types)

Codec가 성공한 소유 raw/message 사본에서 owner가 호출한다. `self` IN/OUT, `decoded` const IN 및 `rx` const IN은 호출 동안 borrowed이고 서로/self와 overlap 불가, out 없음이다. `rx` frame과 decoded는 **동일 Codec 성공 호출**의 결과여야 한다. metadata/length/code 검사 성공이 순서/producer 정책 승인까지 의미하지 않는다. `now`는 이 receiver 로컬 stamp이다. callable 문맥은 IDLE의 직접 owner 호출 또는 Pump의 단회 permit뿐이다.

RX의 원 I1 fields는 끝까지 그대로 보존한다. 별도 view에 WireAGE+RX waiting, 같은 UPDATE이면 retained view의 현재 AGE 중 max를 넣는다. 정상 형식의 열화 품질은 통과시키고 new TX와 원 new UPDATE를 분리한다. local clock이 불명확하면 원 raw 진단은 보존하되 view age_state=UNCERTAIN이며 기능 freshness를 주장하지 않는다. 앞선 기록의 관측 horizon을 입증 못한 경우에는 새 순서 수용·callback·liveness 갱신을 중단한다.

| 분기 | 조건 → 처리·결과 / commit | U4C 계획 |
|---|---|---|
| SR01 | NULL/clock.valid 표현 범위 밖/미초기화/불법 Service 문맥/T 또는 P guard 사용 중 → INVALID_ARG/NOT_READY/REENTRANT. Pump permit는 진입 즉시 소비 | CAN-D03, CAN-D19 |
| SR02 | unknown kind/rx mask 부재/controller_error 비0/old backend generation/health admission 불가 → UNSUPPORTED/NOT_READY, 다른 source 상태 불변 | CAN-D03, CAN-D10, CAN-D11, CAN-D16, CAN-D22 |
| SR03 | source 미bind 또는 decoded Boot≠admitted_boot → NOT_READY, 숫자 Boot 크기로 새 기동 판단 없음 | CAN-D12 |
| SR04 | owner clock progress 검사 및 RX capture→now 경과 불명확 → raw-preserving UNCERTAIN view 후보; 기존 source horizon은 별도 | CAN-D13, CAN-D14 |
| SR05 | 이전 source RX와 epoch/source_horizon_ms[k]/외부 rate 근거 불명확 → CLOCK_UNKNOWN, 새 순서·liveness·callback 없음 | CAN-D13 |
| SR06 | same source TX diff0/old/half-ambiguous → ORDER_REJECT, 기록/정상 수신시각 불변 | CAN-D12, CAN-D13, CAN-D15 |
| SR07 | TX newer이나 UPDATE old/half-ambiguous → ORDER_REJECT, 원 평가 갱신 없음 | CAN-D12, CAN-D13 |
| SR08 | 동일 UPDATE의 원 값/quality 충돌 → ORIGIN_CONFLICT, 기존 원 record 보존 | CAN-D12, CAN-D15 |
| SR09 | 동일 UPDATE·본문 동일 → 누적 AGE max, new_observation0, last_new_update 보존 | CAN-D14, CAN-D15 |
| SR10 | 첫 원 기록 또는 newer UPDATE → 원 품질 그대로 new_observation1, lower-bound/SAT/UNCERTAIN 구분 | CAN-D02, CAN-D14, CAN-D18 |
| SR11 | 완전한 view/순서 메타 commit → 검증된 해당 source last_valid_frame만 갱신; raw AGE 불변 | CAN-D12, CAN-D20, CAN-D21, CAN-D22 |
| SR12 | nullable 진단 sink가 있으면 Service CALLBACK과 T CALLBACK=3/P active=1을 전파하고 borrowed local view 한 번 통지; 정상 반환 뒤 lower guard 복원. 포인터 무효, void를 기능 성공으로 사용하지 않음 → OK | CAN-D18, CAN-D19, CAN-D20, CAN-D21 |

RX consumer가 반환 후 보관할 자료는 값/identity/품질/effective AGE 메타/로컬 stamp 전체를 스스로 독립 복사한다. sink 미연결이어도 Service 자체 진단 사본은 보존되며 기존 C2/App 호출 성공을 꾸미지 않는다. CIS→DOMAIN의 C2 투영은 `OPEN-C2-SOURCE/UNIT/ENUM/PROVENANCE`로 제한한다. Permission은 CIS App 및 BCM/VSS private 감독만 기존 binding을 참조하고 WINDOW는 callback·liveness 대상에 넣지 않는다. `OPEN-PERM/PEER-OWNER`는 기능 사용을 계속 차단한다. Source: [U2 CIS 수신](../../02_APPLICATION_PORT/CIS/10_BEHAVIOR.md#fn-cis-cisapp_ondomainpermission), [DOMAIN 수신](../../02_APPLICATION_PORT/DOMAIN/10_BEHAVIOR.md#fn-domain-domainif_rxcisrear), [BCM](../../02_APPLICATION_PORT/BCM/10_BEHAVIOR.md), [VSS](../../02_APPLICATION_PORT/VSS/10_BEHAVIOR.md).

<a id="fn-u4d-can-svc-bind-source"></a>
## FN-U4D-CAN-SVC-BIND-SOURCE — CanSvc_BindSource

```c
CanCoreStatus CanSvc_BindSource(CanService *self, CanMsgKind kind, uint32_t admitted_boot, uint64_t owner_context);
```

[의사코드](20_PSEUDOCODE.md#fn-u4d-can-svc-bind-source) · [색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md#fn-u4d-can-svc-bind-source) · [원 Boot/계보](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md#section-identity)

기존 Node peer/origin join owner 또는 명시 Host fixture owner만 IDLE 문맥에서 호출한다. `self` IN/OUT, `kind` by-value IN 두 kind뿐, `admitted_boot` IN nonzero u32, `owner_context` IN nonzero u64이며 명시 새 문맥은 이전보다 커야 한다. 이는 실제 peer 신뢰·team adoption·Boot allocator를 결정하는 함수가 아니다. ISR/CAN frame을 받았다는 이유만으로 자동 호출하지 않는다.

| 분기 | 조건 → 처리·결과 | U4C 계획 |
|---|---|---|
| SB01 | NULL/미초기화/guard 재진입 → INVALID_ARG/NOT_READY/REENTRANT, 변경 없음 | CAN-D19, CAN-D22 |
| SB02 | unknown kind/rx mask 비활성/Boot0/context0 → INVALID_ARG/UNSUPPORTED, 변경 없음 | CAN-D12 |
| SB03 | 기존 같은 context+Boot → OK idempotent; 같은 context 다른 Boot 또는 낮은 context → ORIGIN_CONFLICT/ORDER_REJECT | CAN-D11, CAN-D12 |
| SB04 | 명시 더 큰 context, Boot 동일 → context만 교체, 원 UPDATE/AGE/순서/liveness watermark 보존. UINT64_MAX 이후 새 context wrap 불가 | CAN-D12, CAN-D13, CAN-D15 |
| SB05 | 최초 bind 또는 다른 Boot의 명시 새 context → 한 kind의 접수 문맥 commit·has_record0/관측시각 invalid; 다른 kind/TX/pending/inflight는 그대로 → OK | CAN-D11, CAN-D12, CAN-D22 |

Boot 변경에 이전 ALLOW·정상 freshness를 승계하지 않는다. 첫 적합한 새 source raw record 전까지 해당 peer 준비·원 평가를 만들지 않는다. 기존 raw last 사본이 메모리에 남아도 has_record0의 과거 진단이며 current로 사용하지 않는다. owner가 잘못된 peer 문맥을 제공하는 위험의 실제 종료 증거는 `OPEN-PROVENANCE/PEER-OWNER`와 U10 binding에서 확보한다. 이 문맥 함수는 그 미결을 CLOSED로 바꾸지 않는다.
