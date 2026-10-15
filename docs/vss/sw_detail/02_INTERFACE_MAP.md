# VSS 인터페이스 지도 — 원 의미·변환·소비 경계

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[처음 읽기](00_START_HERE.md) · [Layer·Module 지도](01_VSS_MAP.md)

이 지도는 Domain의 원 의미가 VSS 입력으로 전달되는 경계와 VSS 보고가 Domain으로 전달되는 경계를 연결한다. **GUIDE / LOGICAL_APPROVED**인 기존 VSS 계약과 **PROPOSED_IF / BINDING_TBD**인 양단 연결을 구분한다. R6 논리 원형은 실제 C ABI가 아니며 실제 Wire·등록 callback·Task·TxPort·제품 source/image와 HW 검증은 미확정이다.

문서 기준은 `VSS@d8a2220ffc1e9c701bbec505e5b101790d3bfc50`다. 외부 비교 입력은 Network `c60dcf65de52b7fb28e0464d616ba00d350890bb`의 **제안**과 Domain `e814c08a40110c4ac278c9d0d8a540cbf182b756`의 **논리 참고**다. 같은 타입명·함수명이나 링크 수신 성공은 양단 채택·실제 결합의 증거가 아니다.

<a id="ingress"></a>
## Domain → Network → VSS 입력

| 원 자료 | Producer와 원 의미 | Converter / 전달 경계 | Consumer와 상태 writer | 확인 수준·실패 경계 |
|---|---|---|---|---|
| One-shot Event | Domain 기능 owner가 확정한 한 발생. Welcome/Goodbye·Door 완료 등의 실제 발생 조건/caller는 Q02 미결 | Domain의 기존 `DomainIf_TxVssOneShotEvent`/TxPort 논리 참고 → Network 명시 encode/decode → 제안 `VssApp_OnDomainEvent` → 기존 INPUT 연결. 실제 adapter와 변환 위치는 TBD | INPUT은 값·Meta·출처/세대/순서/시간을 검증하고 자기 문맥을 적용. `Store_ApplyInput`이 원 Occurrence·수용/replay 이력의 단일 writer | RX `void`는 local 전달뿐. received ≠ valid ≠ accepted. 실제 생산·복사·bounded 소비·수용 후 실패 sink 미검증 |
| Stateful Warning | 위험·CLEAR는 Domain 판단. 세 경고의 원 evidence·품질·가용성·age는 각각 독립 | 기존 `DomainIf_TxVssWarningState` 논리 참고 → Network 제안 `NetVssWarningState` → 제안 `VssApp_OnDomainWarningState` → INPUT. 공통 Meta에서 독립 근거를 합성하지 않음 | INPUT이 적용 가능한 현재 근거를 전달하고 STORE가 마지막 유효 판단·현재 품질·Hold·Rear 결합을 별개로 관리 | INVALID/STALE/NO_DATA/미수신/SNA는 CLEAR가 아님. 한 경고의 근거 부족으로 다른 유효 경고를 일괄 폐기하지 않음 |
| Rear 위험·활성 | CAUTION/EMERGENCY/CLEAR 위험과 ACTIVE/DISABLED 활성은 Domain의 서로 다른 판단 | Network 제안의 activation meta format과 원 활성 sequence/age/quality를 별도로 검증. 동일 DLC로 구형/신형 지원을 판정하지 않음 | INPUT/STORE가 유효 활성과 유효 위험 근거를 결합. DISABLED는 local 후보/Hold 제거이며 위험 CLEAR와 별개 | format0/미지원은 활성 근거 부족. 재활성만으로 옛 위험 후보 부활 없음. Q06 양단 format/owner 합의 미결 |
| `DOMAIN_PERMISSION` | Domain permission owner의 자료. 독립 입력의 합성·age 축약과 실제 생성원이 Q07 미결 | 제안 `VssNetPriv_OnDomainPermission`은 **Network-private** 수신 감독. 하나의 메시지의 VSS 포함 4수신 의무와 App 소비는 별개 | 기본 소비는 Network 감독. VSS App의 필수 permission callback/새 Core를 추가하지 않음 | App 소비 필요성은 TEAM_DECISION_REQUIRED. HEALTH의 `VssRecoveryPermission`으로 자동 변환하지 않음 |

양단 자료: [Network Data 제안](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/c60dcf65de52b7fb28e0464d616ba00d350890bb/docs/network/interface/02_NODES/VSS/10_DATA.md#type-netvssevent) · [VSS API 제안](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/c60dcf65de52b7fb28e0464d616ba00d350890bb/docs/network/interface/02_NODES/VSS/20_API.md) · [Domain 고정 헤더의 세 DTO](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/e814c08a40110c4ac278c9d0d8a540cbf182b756/docs/Domain/architecture/2_Domain_interface/Domain_Interface_v0.5.h#L361-L389). Domain의 Event 3필드·Warning 5필드·Status 3필드와 Network 확장 DTO를 같은 ABI로 취급하지 않는다. Network가 인용한 두 `DECLARED_HEADERS/DOMAIN` 원 Header는 그 고정 Commit에서 미확보이며 다른 Revision으로 대체하지 않는다.

<a id="input-lifetime"></a>
## 자료·호출·품질·수명·실패

| 경계 | 전달/반환과 보호 | 반영·후속 또는 실패 |
|---|---|---|
| Network RX → INPUT | 제안 typed message/meta는 callback 반환까지 읽기 전용 borrow. 반환 뒤 필요한 값·원 identity·시간 근거는 실제 owner가 안정적으로 복사해야 함 | CRC/length/unsupported code는 Network의 구조 검증. 의미값/품질/source/time 검증은 INPUT. callback/poll/queue 실제 binding은 미결이며 ISR에서 의미 판단하지 않음 |
| INPUT → STORE | `Input_Process(input=NULL, time)`도 RX 없는 품질/기한 진행. 정상 Event·State·Quality 중 배타적인 한 입력만 `Store_ApplyInput`에 전달 | `VssInputApplication`의 입력 문맥 적용과 개별 STORE 수용 결과는 별개. STORE 신규 거부로 검증된 INPUT 문맥 rollback·원 이력 삭제 없음 |
| Event 원 identity·age | Domain 발생/수용 source generation·Occurrence와 원 age를 보존. 링크 Boot/TX 순번·처리 now는 원 발생/새 발생 번호가 아님 | 재전달·재부팅/wrap·Wake·문맥 손실의 양단 계약은 Q04. same 발생 재전달로 새 음향을 만들지 않음 |
| Stateful 품질·Hold | 마지막 유효 ACTIVE/CLEAR, 현재 품질, 선택 후보를 분리. Hold는 첫 신뢰 상실과 원 validUntil 중 더 이른 경계의 기존 의미 | 반복 invalid/재평가/선점/복구로 Hold 연장 없음. Hold 만료는 후보 제거이며 CLEAR/품질 정상화 아님. 입력별 후보값은 A1 선택, 실제 시간 비교/변환은 E04 |
| 시간 후보 | 원 발생 → 최초 actual의 `A1 age < L`와 Network `USE_LIMIT=500ms` 제안을 구분 | Q03 A1 D04 설계 선택 완료; 실측/DOMAIN 생산 확인은 외부 조건. A1 D04의 L을 최초 actual까지 적용하며 Wire 값으로 상한을 늘리지 않음. 재수신으로 age 리셋 없음 |
| PRODUCT / TEST / WINDOW | TEST의 출처·저장·시간·이력·실행은 PRODUCT와 격리. WINDOW/Anti-Pinch는 **[구현 보류 — 설계 유지]** | 격리 TEST의 같은 의미 경로는 유지. 구현 보류·입력 미평가 자체를 Fault/DEGRADED로 만들지 않음 |

VSS 정본·상세: [Input_Process](02_SERVICE/INPUT/functions/Input_Process.md#validation) · [Store_ApplyInput](02_SERVICE/STORE/functions/Store_ApplyInput.md#stateful) · [입력 Data](../design/40_DATA/10_INPUT_DATA.md#input) · [원 시간 계약](../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#source-age) · [Network 호출·실패 제안](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/c60dcf65de52b7fb28e0464d616ba00d350890bb/docs/network/interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-ownership).

<a id="status-path"></a>
## VSS → Network → Domain Status

| 단계 | Producer / Converter / Consumer | 반환·결과와 자료 수명 | 미확정 또는 실패의 영향 |
|---|---|---|---|
| 원본 관측의 반영 | INPUT·STORE·PLAYBACK·CONTROL·STREAM·HEALTH 각 owner가 자기 원본을 반영하고 FLOW가 평가 인과를 연결 | 읽기 getter에서 숨은 반영 없음. owner stamp 숫자의 일치가 아니라 각 적용 완료·인과·원 시간·관련 변경을 확인 | 선택용 basis를 보고용으로 그대로 재사용하지 않음. 미반영·혼합·변경은 재수집 |
| HEALTH 보고 생성 | FLOW → `Health_BuildStatus`. HEALTH 현재 Fault/허용 writer는 별도 `Health_Evaluate` | `VSS_STATUS_CREATED`에서만 snapshot written=1. `VSS_STATUS_REEVALUATE`는 written=0이며 새 정상 보고 없음 | 초기 미평가와 확인 실패 구분. serviceLevel FULL/current mask0으로 READY/FULL/accepting=true 합성 금지 |
| 논리 DTO 변환 | HEALTH → FLOW → **기존 INPUT 통신 연결 한 곳**에서 11필드 변환 | HEALTH 2슬롯·불변 목록/basis, Network 동기 전체 복사 반환까지 보호 | [A2 §4~5](../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#fields), 실제 ABI/게시 E-S01~04 |
| 로컬 Network 인계 | 기존 VssNet_PublishStatus, 안정된 전체 사본 | OK는 local 인계; INVALID/NOT_READY/NO_CAPACITY 구분 | 원 key/raw 후발 실패를 기존 Network→INPUT 진단/loss latch→FLOW로 연결 |
| Wire·수신·Domain 투영 | Network 송신/Codec → Domain Network 제안 `DomainNetPriv_OnVssStatus` → 기존 C2 논리 ingress/기능 consumer | 원 VSS 의미·평가 번호·age·Fault 발생·원 세대와 링크 Boot/TX를 구분하여 보존 | localOK ≠ 실제 송신 ≠ peer RX ≠ Domain 반영. 구조 오류와 의미 미표현/부분 손실을 분리 |
| Domain 상태·고장 소비 | State→Fault 두 경계, 원 key별 결과 분리 | 실제 v0.5는 service→VSM, Fault→observer-only; 확장 C2 후보와 구분 | [A2 §6](../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#domain), State 수용 뒤 전체 재호출 금지; E-S05 |

[HEALTH 보고 상세](02_SERVICE/HEALTH/functions/Health_BuildStatus.md#ownership) · [Status 자료·수명](../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [Network Status 권장안](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/c60dcf65de52b7fb28e0464d616ba00d350890bb/docs/network/interface/02_NODES/VSS/10_DATA.md#r2-status-proposal) · [Domain private 경계 제안](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/c60dcf65de52b7fb28e0464d616ba00d350890bb/docs/network/interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#L71-L86) · [고정 C의 실제 논리 처리](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/e814c08a40110c4ac278c9d0d8a540cbf182b756/docs/Domain/architecture/2_Domain_interface/Domain_Interface_v0.5.c#L1302-L1363).

<a id="status-reduction"></a>
## Status 축약에서 남겨야 할 원 근거

| 원 보고 → Network 후보 | 보존할 의미와 미결 |
|---|---|
| state / availability / accepting | [A2 조합표](../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#reduction), 의미 이름 대응. PLAYING은 전체 Session 보고 |
| 완전한 currentFaults → current mask/active | 현재 전체 목록 완전성이 확인된 때만 빈 목록→현재 mask0 가능. 최근 이력에서 current bit 생성 금지. 여러 instance/대상은 HEALTH 원본에 남김 |
| HEALTH 현재/최근 원기록 → 주요 최근 code/occurrence/age/quality | A2 firstOccurredAt/reportOccurrence와 Snapshot lastFault 필드. kind 목록 순서로 추정 금지, current clear 뒤 이력 보존 |
| basis → update_sequence/source_age | 실제 새 보고 평가 번호·원 평가 시각에 연결. basis cast·CAN TX_SEQUENCE·수신 now 대체 금지. 원 평가 age와 last-fault age는 다름 |
| Wire LAST_FAULT_QUALITY → Domain 품질 | 이력 있음0/확인된 부재3는 서비스·현재 전체 Fault 품질이 아님. 미확인·INVALID를 정상0/READY/mask0 또는 NO_DATA로 보완하지 않음 |
| source generation·원 boot/순서 → Domain 소비 | A2 reportGeneration/평가 번호와 CAN BOOT 명시 대응. 실제 lifecycle E-S02·consumer E-S05, 링크 성공으로 품질 합성 금지 |

위 항목의 필수 근거가 없으면 **해당 새 fresh Status/Domain 투영만 제한**하고 마지막 확정 보고의 원 번호·age를 갱신하지 않는다. 독립적인 유효 Event/Warning이나 확인된 원 사실을 일괄 폐기하지 않는다. 실제 송신·consumer·실물 시험은 계속 미검증이다.

<a id="core-call-return"></a>
## 기존 18 Core CALL / RETURN / 독립 OUT 색인

아래는 기존 승인 함수의 호출 책임·논리 전달 색인이다. 함수 구현·의사코드의 복사본이나 실제 C call graph가 아니다. 기존 5 Layer/10 Module을 유지하며 CORE-16은 HAL/BSP 내부 Boundary, INFRA는 시간/손실 Boundary다. 표의 화살표는 호출·결과 연결 책임을 표현하며 모든 경계가 한 동기 호출이라는 뜻이 아니다. 동기 반환 이후의 장치/출력/복구 사실은 원 owner의 별도 후속이다.

각 R6 링크에서 원 인자 타입/배타·OPTIONAL 조건을 확인하고, 설명/의사코드에서 실제 의미 분기를 읽는다. OUT 군마다 capacity/written·원 귀속·보호수명은 독립이다. void/scalar·빈 OUT·알림 하나는 다른 결과군의 성공이나 보호 종료를 증명하지 않는다.

| Core / Owner | CALL 경계 | 주요 IN (타입·조건은 R6) | 동기 RETURN / 결과 소비 | 독립 OUT 이름 | 원 의미·수명 | 승인 설명 / 의사코드 / R6 |
|---|---|---|---|---|---|---|
| CORE-01 `Flow_Process` / FLOW | app main/task → FLOW → INPUT/STORE/POLICY/PLAYBACK/STREAM/HEALTH | opportunity/time | `VssFlowProgress`: 조율·후속 필요성 → 기존 app 기회 | 없음 | 동기 조율. 다른 owner 원본 변경 없음 | [설명](01_APP/FLOW/functions/Flow_Process.md) / [의사코드](01_APP/FLOW/pseudocode/Flow_Process.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-flow-process) |
| CORE-02 `Input_Process` / INPUT | FLOW → INPUT → STORE | OPTIONAL input/time | `VssInputApplication`: validation/contextApplied/storeAdmission → FLOW | `diagnostics` | RX 없어도 품질 진행; STORE 거부≠INPUT 문맥 rollback | [설명](02_SERVICE/INPUT/functions/Input_Process.md) / [의사코드](02_SERVICE/INPUT/pseudocode/Input_Process.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-input-process) |
| CORE-03 `Store_ApplyInput` / STORE | INPUT → STORE → 기존 Runtime | ONE_OF event/state/quality | `VssStoreAdmission`: 개별 STORE 수용 → INPUT | 없음 | STORE ledger/Stateful/Rear만 writer; 품질로 CLEAR 합성 없음 | [설명](02_SERVICE/STORE/functions/Store_ApplyInput.md) / [의사코드](02_SERVICE/STORE/pseudocode/Store_ApplyInput.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyinput) |
| CORE-04 `Store_ApplyPlaybackFact` / STORE | PLAYBACK 후속/통지 → STORE → 기존 Runtime | 단일 fact | `VssFactApplication`: 그 fact 적용 → PLAYBACK | 없음 | NOT_APPLIED 통지 보호 유지; 기존 동일 적용만 멱등 인정 | [설명](02_SERVICE/STORE/functions/Store_ApplyPlaybackFact.md) / [의사코드](02_SERVICE/STORE/pseudocode/Store_ApplyPlaybackFact.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyplaybackfact) |
| CORE-05 `Store_AdvanceDeadlines` / STORE | FLOW → STORE → 기존 Runtime | time/단일 playback 관측 | `VssStoreProgress`: 기한 진행/재확인 → FLOW | 없음 | 원 fact 적용·notificationPending 확인이 expiry보다 먼저 | [설명](02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md) / [의사코드](02_SERVICE/STORE/pseudocode/Store_AdvanceDeadlines.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-advancedeadlines) |
| CORE-06 `Select_Choose` / POLICY | FLOW → POLICY → read-only owner 관측 | candidates/playback/restrictions/assets/devices/cleanup/basis | `VssSelectionDecision`: 선택 판단 → FLOW → PB 요구 | 없음 | POLICY가 PB/STREAM을 실행 호출하지 않음 | [설명](02_SERVICE/POLICY/functions/Select_Choose.md) / [의사코드](02_SERVICE/POLICY/pseudocode/Select_Choose.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-select-choose) |
| CORE-07 `Playback_RequestTransition` / PLAYBACK | FLOW → PLAYBACK → STREAM Prepare/RequestControl | ONE_OF decision/terminationSession; reason/현재 관측/basis | `VssRequestDisposition`: PB 의도 채택 → FLOW | 없음 | 동기 ADOPTED≠prepared/actual/retirement | [설명](02_SERVICE/PLAYBACK/functions/Playback_RequestTransition.md) / [의사코드](02_SERVICE/PLAYBACK/pseudocode/Playback_RequestTransition.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-playback-requesttransition) |
| CORE-08 `Playback_Advance` / PLAYBACK | FLOW/즉시결과 적용 → PLAYBACK → STREAM/STORE | requests/outputs/cleanup/현재 조건/basis/time | `void`: OUT 원 PB 관측·fact·적용·진단 → FLOW/STORE/HEALTH 연결 | `observation`, `facts`, `applications`, `diagnostics` | void≠성공; facts/applications 동일 원 순서/written | [설명](02_SERVICE/PLAYBACK/functions/Playback_Advance.md) / [의사코드](02_SERVICE/PLAYBACK/pseudocode/Playback_Advance.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-playback-advance) |
| CORE-09 `Asset_Read` / ASSET | STREAM Prepare/Advance → ASSET → read-only image/Runtime | ONE_OF request/referenceEnd; readerReferencesEnded | `VssAssetReadDisposition`: 읽기/참조 종료 disposition → STREAM | `span`, `diagnostics` | span bytes는 실제 reader 참조 종료까지 보호 | [설명](02_SERVICE/ASSET/functions/Asset_Read.md) / [의사코드](02_SERVICE/ASSET/pseudocode/Asset_Read.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-asset-read) |
| CORE-10 `AudioStream_Prepare` / AUDIO_STREAM | PLAYBACK 요구/후속 → STREAM → ASSET/CONTROL/TX | 원 preparation | `VssRequestDisposition`: 동기 준비 의도 → PB; 원 requests/diagnostics는 별도 | `requests`, `diagnostics` | ADOPTED≠PREPARED/actual; 준비 실패 뒤 보호 유지 | [설명](02_SERVICE/AUDIO_STREAM/functions/AudioStream_Prepare.md) / [의사코드](02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_Prepare.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-prepare) |
| CORE-11 `AudioStream_RequestControl` / AUDIO_STREAM | PB 또는 FLOW Recovery → STREAM → TX/CONTROL | ONE_OF playback/recovery; 현재 조건/basis | `VssRequestDisposition`: 동기 의도 → caller; 원 requests/diagnostics는 별도 | `requests`, `diagnostics` | Recovery는 Session 없음·requests written=0 | [설명](02_SERVICE/AUDIO_STREAM/functions/AudioStream_RequestControl.md) / [의사코드](02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_RequestControl.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-requestcontrol) |
| CORE-12 `AudioStream_Advance` / AUDIO_STREAM | FLOW 처리 기회 → STREAM → ASSET/TX/CONTROL | time; 보호된 하위 원 사실은 내부 연결 | `void`: 독립 7 OUT → PB/FLOW/HEALTH 연결 | `requests`, `outputs`, `cleanup`, `devices`, `diagnostics`, `performed`, `verified` | devices는 CONTROL 관측 전달이며 STREAM이 readiness writer 아님 | [설명](02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md) / [의사코드](02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_Advance.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-advance) |
| CORE-13 `AudioTx_Request` / AUDIO_TX | STREAM Prepare/RequestControl/Advance → TX → HAL/Runtime | ONE_OF pcm/control; continuingOperation/targetOperations/time | `VssTxRequestState`: 요청 단계 scalar → STREAM; requests/returned/diagnostics 별도 | `requests`, `returned`, `diagnostics` | 효력 전 새 무접근 거부와 기존 작업의 부분 효력 구분 | [설명](03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md) / [의사코드](03_DRIVER/AUDIO_TX/pseudocode/AudioTx_Request.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request) |
| CORE-14 `AudioTx_Advance` / AUDIO_TX | STREAM 후속 → TX → 보호 HAL raw/Runtime | time; 원 operation/raw는 내부 | `void`: 독립 5 OUT → STREAM | `requests`, `outputs`, `consumed`, `returned`, `diagnostics` | 원 request/output/consumed/returned/diagnostics를 하나로 합치지 않음 | [설명](03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md) / [의사코드](03_DRIVER/AUDIO_TX/pseudocode/AudioTx_Advance.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-advance) |
| CORE-15 `AudioControl_Service` / AUDIO_CONTROL | STREAM Prepare/RequestControl/Advance → CONTROL → HAL/Runtime | OPTIONAL intent/time | `void`: 독립 5 OUT → STREAM/FLOW/HEALTH 연결 | `devices`, `results`, `diagnostics`, `performed`, `verified` | device/config 별 readiness; performed와 verified 별개 | [설명](03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md) / [의사코드](03_DRIVER/AUDIO_CONTROL/pseudocode/AudioControl_Service.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service) |
| CORE-16 `AudioHAL_Callback` / `AUDIOHAL_BOUNDARY` (내부 경계, Module 아님) | vendor callback/IRQ → HAL Boundary → 최소 acknowledge/알림 | OPTIONAL originalTx/originalDevice; observation | `void`; 보호 raw는 TX/CONTROL 후속 소비 | 없음 | ISR에서 TX/CONTROL/상위 Core 실행 호출 없음 | [설명](04_HAL_BSP/AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md) / [의사코드](04_HAL_BSP/AUDIOHAL_BOUNDARY/pseudocode/AudioHAL_Callback.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiohal-callback) |
| CORE-17 `Health_Evaluate` / HEALTH | FLOW → HEALTH → 필요한 기존 Runtime | diagnostics/performed/verified/playback/devices/cleanup/basis | `VssHealthChange`: 제한 변화 → FLOW; faults/permissions 독립 | `faults`, `permissions` | 현재 Fault/permission writer; 직접 Backend 수행/송신 없음 | [설명](02_SERVICE/HEALTH/functions/Health_Evaluate.md) / [의사코드](02_SERVICE/HEALTH/pseudocode/Health_Evaluate.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-evaluate) |
| CORE-18 `Health_BuildStatus` / HEALTH | FLOW 별도 보고 단계 → HEALTH → 필요한 기존 Runtime | 입력/후보/PB/assets/devices/cleanup; OPTIONAL 수용/추적 scalar; basis | `VssStatusBuildResult`: CREATED 또는 REEVALUATE → FLOW; snapshot 별도 | `snapshot` | 읽기 전용 보고; CREATED에서만 snapshot written=1 | [설명](02_SERVICE/HEALTH/functions/Health_BuildStatus.md) / [의사코드](02_SERVICE/HEALTH/pseudocode/Health_BuildStatus.md) / [R6](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus) |

<a id="external-open"></a>
## 외부 결정·근거가 필요한 범위

| 기존 질문 | 미결 의미 / Owner | 필요한 근거 / 종속 영향 |
|---|---|---|
| Q01 | Domain Header/C2·Network 문서·VSS IF owner: 고정 C와 선언 target의 차이 | 원 Header2개·Commit·채택/미채택·DTO/enum 차이. 실제 ABI/최종 IF 채택 미확정 |
| Q02 | Domain 발생 기능 owner·VSS 의미 소비: Event 생산 조건 | Event별 실제 발생/caller·대상·수용 근거. 완료/무실행/이미 목표/실패 구분 미확정 |
| Q03 | Domain 원 발생·VSS POLICY/PLAYBACK·Network: USE_LIMIT와 최초 actual | 기산·검사 단계·경계·후속 cue·후보값 합의. A1에서 원 USE_LIMIT·500ms·2000ms 중 최솟값 L을 선택; 실제 DOMAIN 생산/성능 확인은 외부 조건 |
| Q04 | Domain runtime·VSS INPUT/STORE·Network 양단: Startup/Wake/identity | 원 identity·시각·재기동/wrap/재전달/만료/실패 계약. old 발생 부활 금지 |
| Q05 | Domain 경고·원 evidence·VSS INPUT/STORE/POLICY·Network: 품질/SNA/Hold | 독립 evidence·원 기한·첫 신뢰 상실·종료 합의. stale를 CLEAR로 만들지 않음 |
| Q06 | Domain Rear·VSS INPUT/STORE·양단 Codec: 활성 근거 | format/원 활성 평가/재활성·동일 DLC의 legacy 판별. 미지원 활성 근거를 정상으로 채우지 않음 |
| Q07 | Domain permission·기능·Network, VSS App 필요성 owner | 합성/AGE 축약·실제 생성원·4수신/필수 App 소비 근거. 복구 허용으로 치환 금지 |
| Q08 | VSS HEALTH/FLOW/INPUT·Network·Domain 서비스 | A2 §2~5에서 coherent 조합·11필드·원 평가·단일 INPUT 변환·동기 복사 선택. 실제 E-S01~04 |
| Q09 | VSS 진단·Domain 진단/이력·Network | A2 §1에서 완전 current·최신 원기록/최초 occurrence·시각·기동 내 보존 선택. 실제 보호/세대 E-S01~02 |
| Q10 | Domain C2/VSM/진단/observer·VSS HEALTH·Network | A2 §6에서 이력/전체 품질·State→Fault 부분 결과/재호출 선택. 실제 v0.5 서비스 한정/observer-only 확인, 설치 consumer E-S05 |
| Q11 | Network adapter·Domain TxPort/실패 owner·VSS INPUT/STORE/FLOW | bounded 복사/반환/수용 후 실패 sink·identity·소비 완료 근거. localOK/RX void로 DONE 없음 |
| Q12 | 양단 Network/Codec·각 CAN/보드 owner | 실제 양단 profile 채택·encoder/decoder/검증 벡터·물리 근거. offset 산술이나 SAI로 Wire 채택을 입증하지 않음 |
| Q13 | VSS STORE/PLAYBACK/HEALTH·Domain·Network | 입력/통신 복구의 pre-Pending 폐기 제안과 출력 Fault 복구의 NO_START+fresh 재평가를 유형별 구분. 다른 scope로 합치지 않음 |
| Q14 | Domain 의미·VSS INPUT/STORE·Codec | 품질 enum/미표현 경로의 명시 대응·거부/제한·다른 입력 독립성. 숫자 cast·SNA/UNCONFIRMED→CLEAR 금지 |

Q01~Q14는 원 질문·owner·closure를 보존한 요약이며 해결/새 정책 승인 표가 아니다. Network [원 Open 제안](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/c60dcf65de52b7fb28e0464d616ba00d350890bb/docs/network/interface/06_OPEN_FOR_TEAM.md)과 [R6 미결 원문](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register)을 함께 읽는다. 외부 누락 참조 **15건/7대상**은 역사적 원문 미확보이고 Q14나 15개 새 정책 결정과 다르다. 원 G1-B 최종 결과는 SOURCE_NOT_RECOVERED이며 복사본 문서 보정 PASS가 그 원본·실제 결합을 복원하지 않는다.

R6-01 Header·R6-02 C ABI·R6-03 복사/retain·R6-04 멱등/원 사실 보호·R6-05 STOP 원 대상 scope·R6-06 원 시간/권한·R6-07 HAL/DMA/물리 충분성·R6-08 Startup/Recovery/보고 정책의 OPEN은 유지한다. 실제 저장·직렬화·수명·실패 관측·양단 소비와 보드 충분조건의 증거가 필요한 곳만 제품 결합 HOLD다. 독립 입력 검증·품질·중복/replay·읽기/coherence·Status 변환 제한·late 귀속은 Software-only/Host-only 구현 후보로 남기며 실제 코드는 별도 승인 뒤 가능하다.

## A1 현재 입력 기준과 이력

현재 입력 연결은 [92_NETWORK_BINDING](../design/90_BINDING/92_NETWORK_BINDING.md)과 A1으로 개정한 Data/계약/의사코드다. Network 고정 원문은 `70028972ec713a3ec6b5f0a419e0dc89fd963844`이다. 과거 SHA·Q01~Q14·PASS·P1B HOLD 기록은 당시 검토 이력이며 현재 미결 전체가 아니다. D01 WARNING60, D04 L, D05 RX Age, D06 Hold/활성 후보, D07 owner/용량, D08 복구 분리는 설계 반영했다. Q 관련 DOMAIN 판단 순서/생산·계보·Boot·실제 시간/ABI/HW는 E01~E05로 구체화했다. Status 관련 Q/D09~D11은 아래 A2 선택으로 보완했으며 Network A2 기준은 b73faedd + A1-C1 통합 패치다.

## A2 현재 Status 결정과 과거 Q 기록

Q08의 전체/초기 reduction, Q09의 완전 current/원 최근 발생, Q10의 단일 변환/부분 성공은 [A2 Status Binding](../design/90_BINDING/93_NETWORK_STATUS_BINDING.md)에서 선택했다. 위 과거 OPEN/미결 기록을 현재 내부 정책 전체 HOLD로 읽지 않는다. 실제 설치 source·시간/기동·게시/sink·DOMAIN consumer 검증은 E-S01~05이며 A3·제품 착수는 없다.
