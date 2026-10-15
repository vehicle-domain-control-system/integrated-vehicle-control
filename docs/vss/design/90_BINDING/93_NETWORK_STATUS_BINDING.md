# A2 — Status/Fault 생산·Network 변환·DOMAIN 수신

> 2026-10-15 · A1-C1 독립 PASS 인수 · A2 설계 선택 / 구현·시험 NOT_RUN · 독립 검수 대기.

[A1 입력·시간·복구](92_NETWORK_BINDING.md)는 유지한다. 이 문서는 D09~D11/Q08~Q10의 VSS·Network 소유 결정을 구체화한다. VSS 기준은 `73edd8e2` 위 A1-C1 미커밋본, Network 기준은 `b73faedd` + A1-C1 통합 패치다. Network 로컬 변경 6건은 별도 보존하며 A2는 PATCH_ONLY다. 18 Core 원형·5 Layer/10 Module·Fault5·Network 48 Wire·공유 C2 Header는 변경하지 않는다. 아래 필드는 기존 논리 Data의 보완이며 제품 C ABI 확정이 아니다.

<a id="producer"></a>
## 1. D09 — HEALTH 원기록과 완전한 보고

`Health_Evaluate`만 Fault instance/current/recent/restriction의 writer다. 기존 `VssFaultState.instance/diagnosis`를 유지하고 `reportOccurrence`(SemanticCount, 논리 범위 1..UINT32_MAX)와 `firstOccurredAt`(VssFactTime)을 같은 원기록에 추가한다. 새 instance를 최초 보호할 때 HEALTH가 기동 내 단조 발생 번호를 부여한다. 반복 진단·revision·복구·재전송은 번호/최초 시각을 바꾸지 않는다. 복구 뒤 재발은 새 instance/번호다. 불투명 instance, Session/Attempt, Event occurrence, CAN TX_SEQUENCE를 이 숫자로 cast하지 않는다.

`firstOccurredAt`은 원 producer의 `diagnosis.occurredAt` 값 복사다. 진단 없이 확인된 PB 불명확 최종은 그 원 final fact의 시각을 복사한다. 처리 now로 보충하지 않는다. 시각 불명확도 원 상태로 보관하고 실제 현재 제한은 적용하되 새 Status의 이력 투영은 제한한다. 발생 번호는 HEALTH 등록 순서이고 원 시각 순서를 대체하지 않는다.

보고의 current 목록은 같은 HEALTH revision에서 **모든 현재 instance의 kind를 중복 제거한 집합**이다(최대 5종). `Health_Evaluate`의 이번 OUT 부분목록은 전체 목록의 증거가 아니다. 원 진단의 적용 완료·누락/손실 부재·관련 owner coherence를 함께 확인해야 한다. 완전한 공집합만 mask0/active0이 된다. 같은 종류 두 instance는 bit 하나지만 HEALTH의 target/configuration/revision/복구 의무는 둘 다 남는다. recent kind 목록도 최대 5종의 이력 요약이며 occurrence/time을 제공하지 않는다.

최근 주요 기록의 대상은 기존 Fault5 전부이며, 현재/해제된 원기록 중 원 발생 시각이 가장 늦음이 입증된 하나를 선택한다. 정확히 같은 원 시각의 동시 발생만 큰 reportOccurrence로 결정한다. 불확실한 시간 구간이 겹쳐 선후를 증명할 수 없으면 선택 미확인으로 REEVALUATE다. 늦게 도착한 오래된 진단은 더 큰 번호라도 최신 이력을 밀어내지 않는다. 최신 원기록은 해당 기동 종료까지 고정 보존한다. current 해제는 recent 이동이며 이력 삭제가 아니다. 나머지 과거 원기록은 current가 아니고 snapshot/복구/진단 참조가 모두 끝난 뒤 기존 유한 저장 예산에서 회수할 수 있다. kind 이력 집합과 마지막 원기록은 회수로 사라지지 않는다.

원기록 보호 공간이 부족하면 미반영/손실을 기존 진단 latch에 남겨 완전성을 false로 유지한다. 기존 current나 미소비 원사실을 밀어내어 정상 공집합을 만들지 않는다. 실제 원기록 예산·회수/게시 수단은 E-S01의 입증 대상이다.

기존 `VssStatusSnapshot` 9필드에 `reportGeneration`, `reportSequence`, `evaluatedAt`, `lastFaultPresent`, `lastFaultKind`, `lastFaultOccurrence`, `lastFaultAt`만 추가한다. private FaultState 포인터나 별도 상태 DTO는 추가하지 않는다. `CREATED`는 서비스 판단·현재 집합·이력 존재/부재·시간/세대의 필요한 근거가 모두 유효하고 일관된 **완전한 보고**라는 생산 계약이다. INVALID/부분 진단을 Wire의 정상 품질로 축약할 수 없으므로 부족하면 `REEVALUATE/written0`이다. 이전 보고의 번호·시각은 그대로 남고 독립 Event/Warning 입력은 계속 처리한다.

<a id="reduction"></a>
## 2. 상태·가용성·accepting의 결정표

FULL/DEGRADED/UNAVAILABLE은 기존 opaque availability의 의미 이름이다. 전체 지원 범위는 현재 제품에서 지원하는 서비스 집합이며 보류 WINDOW는 포함시키지 않는다. source가 미평가라는 사실과 실제 제한을 구분한다.

| 확인한 원 관측(위 행 우선) | state | availability | serviceLevel |
|---|---|---|---|
| QUARANTINED/불명확 최종, 또는 확인된 Fault로 공통 정상 출력/새 start 불가 | FAULT | UNAVAILABLE | DEGRADED |
| 정상 초기화가 진행 중이고 아직 모든 서비스의 출력 준비 전임이 확인됨 | STARTUP | UNAVAILABLE | 지원 집합 정상 확인 시 FULL, 확인된 부분 제한 시 DEGRADED |
| 정상 초기화 완료, 적법 actual로 시작된 전체 Session이 retirement 전 | PLAYING | 전체 지원 준비 FULL, 일부만 준비 DEGRADED | 대응 FULL/DEGRADED |
| 정상 초기화 완료, 진행 중 Started Session 없음, 하나 이상 서비스 출력 가능 | READY | 전체 지원 준비 FULL, 일부만 준비 DEGRADED | 대응 FULL/DEGRADED |
| 준비 관측 부족/원인 불명, 또는 위 조건 어느 것도 입증 못 함 | 새 보고 없음 | REEVALUATE | 기본값 없음 |

초기화 완료·무고장인데 출력 가능 서비스가 전혀 없는 관측은 원 근거 모순/부족으로 재수집한다. 부분 asset Fault 하나가 있어도 다른 지원 서비스가 준비되었으면 READY/DEGRADED 또는 PLAYING/DEGRADED를 보고할 수 있다. current bit만으로 FAULT를 정하지 않는다. PLAYING은 cue 무음·반복 대기·시작 후 STOPPING/ABORTING을 포함하며 개별 Event DONE이 아니다. 첫 행은 진행 중 Session보다 우선한다. STARTUP은 확인된 초기화 진행이라는 사실이 필요하며 단순 관측 부재를 뜻하지 않는다.

두 OPTIONAL의 non-NULL 포인터는 먼저 안전/출처/scope를 검사한다. accepting은 INPUT 경로 준비와 STORE 추적/replay 준비의 다음 논리 결합이다.

| INPUT / STORE | true | false | NULL(unknown) |
|---|---|---|---|
| true | true | false | REEVALUATE |
| false | false | false | false |
| NULL(unknown) | REEVALUATE | false | REEVALUATE |

확인된 false 하나로 수용 불가를 보고해도 다른 NULL을 false로 간주하지 않는다. 나머지 Snapshot 필드의 필수 관측은 독립적으로 충족해야 한다. FAULT/UNAVAILABLE과 accepting=true 조합도 INPUT/STORE가 준비되었으면 허용한다. 이는 개별 신규 Event accepted/재생 보장이 아니며 A1의 L·자원·정책 gate가 그대로 적용된다. false라도 duplicate/CLEAR/품질/기한/정리를 계속 처리한다. 입력 품질 상실만으로 output Fault를 만들지 않는다.

<a id="time"></a>
## 3. 평가 번호·세대·원 시간

HEALTH의 보고 책임은 새 coherent 평가를 실제 완료하고 전체 Snapshot을 유효 공개하는 경계에서만 `reportSequence`를 1부터 증가시킨다(논리 1..UINT32_MAX). `evaluatedAt`은 이번 평가의 Runtime 원 사실 시각이며 basis.time과 같은 clock domain/epoch에서 대조 가능해야 한다. Snapshot 공개와 counter 확정은 같은 직렬 경계다. 실패한 공개·REEVALUATE·동일 Snapshot 재인계는 counter와 시각을 전진시키지 않는다. 값이 같아도 모든 원 관측을 다시 확인한 실제 새 평가는 새 번호를 가질 수 있다. 단순 주기 tick/송신 요청은 새 평가가 아니다.

`reportGeneration`은 VSS의 실제 기동 세대(VssGenerationType)다. HEALTH의 발생/평가 counter 수명과 수용된 CAN VSS BOOT lifecycle의 1:1 대응을 E-S02에서 입증한다. DOMAIN에서 들어오는 Event/Warning 원 세대와는 별개다. Network만 reset한 경우 old 보고를 새 BOOT로 재명명하지 않고 미인계 보고를 격리한다. 기존 논리 CAN BOOT와 보호 counter/원기록이 유지됨을 입증한 재결합 뒤 새 coherent 평가로만 재개한다. Network 재시작이 새 CAN BOOT를 요구하는데 HEALTH 원 세대가 그대로라면 1:1 대응을 만들 수 없으므로 Status만 제한한다. 실제 새 VSS 기동 세대가 확인되기 전 가짜 generation 발급·Fault 이력 삭제·강제 reset으로 해소하지 않는다. counter wrap은 같은 세대 내 재사용 금지이며 새 승인 세대 없이는 Status를 제한한다. 기동 시 과거 저장 이력이 없다면 확인된 history 없음으로 시작한다; 과거 로그를 현 기동 occurrence로 재발급하지 않는다.

INPUT 변환 시 하나의 확인된 시간 기준점 T에서 `source_age_ms=age(T,evaluatedAt)`, `last_fault_age_ms=age(T,lastFaultAt)`를 각각 계산한다. 같은 domain/epoch와 경과 산술을 증명해야 한다. VssFactTime이 구간이면 입증된 비음수 경과의 보수적 상한(현재 latest−원 earliest)을 사용하며 정확한 단일 시각이라고 주장하지 않는다. 상한/연속성을 입증하지 못하면 이번 제출을 보류한다. u16은 65535로 포화한다. 최근 이력의 65535는 65535ms 이상이라는 하한을 소비자에게 유지한다. source age가 포화한 보고는 fresh 투영하지 않는다. 대기/전송/RX 후에는 기존 Network 경과 누적 경계에서 미반영 경과만 한 번 더한다. RX now·TX 번호를 원 평가/발생으로 사용하지 않는다.

Network 250ms 보고/750ms 중단 감시는 기존 후보로 유지하며 원 서비스 사용 기한과 같다고 가정하지 않는다. 실제 fresh 허용 기한·소비시점 age 검사는 E-S03에서 DOMAIN 사용 계약과 결합한다. 기한 미확인인 consumer만 fresh 사용을 제한한다. 값·원 번호·age를 보존하는 검증/보관·확인된 정상/오류 Snapshot 생산 자체를 전역 HOLD하지 않는다.

<a id="fields"></a>
## 4. NetVssStatus 11필드 원본표

모든 행의 공통 수명은 보호된 HEALTH Snapshot → INPUT의 동기 변환 → Network 소유 전체 사본이다. 모든 행의 공통 실패는 부분 제출 없이 원 Snapshot 유지/명시 재수집이다. enum은 의미 이름으로 대응하며 숫자 cast하지 않는다.

| DTO 필드 | 원 생성 owner / 선택 | 타입·단위·변환 | 품질·기산점 / 추가 실패 |
|---|---|---|---|
| vss_state | HEALTH state, §2 | NetVssState: STARTUP0/READY1/PLAYING2/FAULT3 | 반영 완료 서비스 근거; 미정 state 거부 |
| vss_availability | HEALTH availability, §2 | NetVssAvailability: FULL0/DEGRADED1/UNAVAILABLE2 | serviceLevel/mask0에서 추정 금지 |
| vss_accepting_events | HEALTH accepting, INPUT/STORE 원 근거 | NetFlag false0/true1 | §2 unknown 조합은 생성 불가 |
| vss_fault_active | HEALTH 완전 current kind 집합 | NetFlag, mask!=0 | 부분목록을 false로 변환 금지 |
| fault_mask | 동일 current 집합, 중복 kind OR | u8 bits0..4: asset/start/output/state/init | 최근 이력 제외; 예약 bit 거부 |
| vss_last_fault | HEALTH 선택 lastFaultKind | u16 NONE0/asset1/start2/output3/state4/init5 | present일 때만 선택 kind 유효 |
| last_fault_occurrence | HEALTH 원기록 reportOccurrence → snapshot.lastFaultOccurrence | u32 1..UINT32_MAX, 없음0 | 원 identity, 평가/TX와 독립; 폭 초과 거부 |
| last_fault_age_ms | HEALTH lastFaultAt → INPUT age | u16 ms/65535 포화, 없음0 | 최초 원 발생부터; 시간 불명은 없음으로 바꾸지 않음 |
| last_fault_quality | HEALTH lastFaultPresent | NetValueQuality: 존재0/확인된 부재3 | 없음은 code/occurrence/age 모두0. current/서비스 품질 아님 |
| update_sequence | HEALTH reportSequence | u32 1..UINT32_MAX | 실제 새 완료 평가만; 재인계 불변 |
| source_age_ms | HEALTH evaluatedAt → INPUT age | u16 ms/65535 포화 | 원 평가부터; 재전송/대기 리셋 없음 |

lastFaultPresent=false이면 변환기는 lastFaultKind/lastFaultAt을 읽지 않고 code/occurrence/age0, quality3을 함께 만든다. Snapshot의 무효 의미 필드가 Wire로 새어 나오지 않는다.

`serviceLevel`은 availability 판단의 원 근거로 보존하되 별도 Wire 필드를 추가하지 않는다. reportGeneration은 기존 CAN BOOT 검증 binding의 원 근거이며 Wire에 새 세대 필드를 추가하지 않는다. 원 enum5 종류 및 Snapshot·basis는 [Diagnostic Data](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot)에 정의한다.

<a id="handoff"></a>
## 5. D10 — 단일 INPUT 변환·bounded 복사·실패 후속

HEALTH → FLOW → **기존 INPUT 통신 연결 한 곳**에서 위 11필드 변환 → `VssNet_PublishStatus` → Network의 전체 사본/Codec 순서를 선택한다. INPUT은 HEALTH의 state/current/history 정책을 재판단하지 않고 명시 대응/범위/시간 검증만 한다. HEALTH의 직접 Network 호출, 새 public converter/publisher/Manager/Core는 없다. `Input_Process`의 기존 입력 R6 원형도 바꾸지 않는다.

보고 storage는 기존 HEALTH 보고 책임의 정적 2슬롯(생성용/아직 인계 안 된 보고용)을 선택한다. 각 슬롯은 current/recent kind 각각 최대5개와 실제 basis stamps/시간 참조를 불변 보호한다. stamps 및 원 Fault pool은 제품의 고정 상한을 E-S01에서 입증하며 상한 초과 시 REEVALUATE다. 추가 동적 큐/무한 backlog는 없다. 변환과 PublishStatus 호출은 같은 기존 직렬 처리 경계에서 수행한다. Network는 반환 전에 DTO 11필드·generation 대응·두 Age 기준점·진단 상관 key를 자기 저장소에 복사/게시한다. 외부 Snapshot 포인터를 보관하지 않는다. 성공/거부 반환 후 INPUT 참조는 종료되지만 미인계 보고는 FLOW 후속에 필요하면 HEALTH 슬롯에 유지한다.

| 결과/사건 | 원자료·실패 sink | FLOW 후속 |
|---|---|---|
| CREATED | 완전 보고 1건, (generation, reportSequence) 고정 | INPUT 단일 변환 시도 |
| REEVALUATE | written0; 기존 counter/time 불변 | 원 관측 재수집; 다른 유효 입력 계속 |
| INVALID_ARGUMENT 또는 내부 변환 실패 | INPUT 기존 진단에 원 key/필드/단계/raw 결과; 이번 제출0 | 반복 호출 금지, 근거/변환 재수집; 잘못된 보고를 성공 처리하지 않음 |
| NOT_READY | 기존 INPUT 연결/시간/세대/sink 준비 부족을 구분 | 같은 원 보고를 유지하거나 새 coherent 보고로 미인계 슬롯 교체; 준비 변화 뒤 한 번 재검토 |
| NO_CAPACITY | Network 전체 미접수, accepted/Driver 사본 불변 | 다음 기존 처리 기회에서 최대1회 재검토; busy loop 금지 |
| NET_SUBMIT_OK | Network 안정된 전체 로컬 사본·원 key 보유 | HEALTH 소비 참조 해제 가능; TX/peer/DOMAIN 성공 생성 없음 |
| 접수 후 Driver terminal failure/bus-off/미확인 완료 | Network 기존 M09/전송 소유 관측에 원 key·BOOT/TX·raw·단계·참조 종료 여부 보존 → INPUT 기존 진단16/loss latch → FLOW | 원 보고 실패로 관측; 재전송은 새 평가가 아니며 자동 무한 retry 없음. 새 처리가 필요하면 별도 평가 |

Network는 기존 송신 슬롯 수를 넘는 접수를 NO_CAPACITY로 거절하고 terminal failure 관측 공간도 접수 전에 확보한다. sink 미연결은 NOT_READY다. sink 전달은 기존 INPUT 처리 기회에서 bounded drain하는 owner 관측 연결로 선택하며 IRQ에서 FLOW/HEALTH를 호출하지 않는다. 실제 기존 함수/관측 필드와 직렬화 증거는 E-S04다. 진단16이 가득 차면 C1의 첫/최근·count/loss latch를 보존하고 새 접수를 제한한다. 유실을 성공으로 세지 않는다. 통신 실패를 임의 AUDIO_OUTPUT_FAILURE나 Event 재생 실패로 위조하지 않는다.

아직 Network가 접수하지 않은 1건만 더 새 coherent 보고로 교체할 수 있다. 교체 사실과 이전 key의 미인계를 관측에 남기고, accepted/Driver 참조 중 사본은 변경하지 않는다. AGE는 인계/재검토 시 갱신된 경과만 더하며 counter/원시각은 불변이다. FLOW는 미인계/실패 관측·재수집이 남으면 FOLLOW_UP_REQUIRED 또는 RECOLLECT를 기존 의미대로 반환한다.

<a id="domain"></a>
## 6. D11 — DOMAIN 두 경계와 실제 source의 한계

Network 변환 정본은 패치의 `sw_detail/functions/can/VSS_STATUS.md`다. 수신 검증 후 같은 원 DTO/meta를 M09의 기존 보호 문맥에 보존하고 State → Fault 순서로 독립 결과를 기록한다. 원 reportGeneration/update_sequence와 source age, last occurrence/age, BOOT/TX를 서로 대체하지 않는다. `last_fault_quality=0/3`만 보고 C2 `service.meta.quality`/`faultQuality`를 OK로 만들지 않는다. 승인된 완전 보고 producer 계약 + source lineage + 신선도 + 같은 입력 association이 입증된 경우에만 현재 service/진단의 `DATA_QUALITY_OK`를 부여할 수 있다. 유효하게 보고된 FAULT도 자료 품질은 OK일 수 있다. 과거 이력 없음3은 전체 Fault 품질 NO_DATA가 아니다.

기본 대상은 Network 문서의 C2 확장형 StateReport(service, faultMask, lastFaultCode, lastFaultOccurrenceId, lastFaultAgeMs, faultQuality, sourceGeneration)다. enum STARTUP/READY/PLAYING/FAULT 및 FULL/DEGRADED/UNAVAILABLE을 각각 명시 대응한다. meta.updateSequence와 meta.ageMs는 원 평가/누적 age이며 수신 now가 원 평가가 아니다. lastFaultAgeMs의 u32 확대는 포화65535를 정확한 시각으로 복원하지 않는다. M09 원 DTO를 함께 유지하고 소비자는 포화 하한 의미를 받아야 한다.

`FaultInfo_t`는 **현재** 완전 mask의 가장 작은 정의 code(1..5) 하나를 대표로 선택한다. 이는 진단용 결정 순서이지 위험 우선순위/복구 정책이 아니다. category=FAULT_CATEGORY_FUNCTION, state=FAULT_STATE_ACTIVE, quality=DATA_QUALITY_OK, faultCode=선택 code다. 완전 공집합이면 code0/state INACTIVE로 정규 초기화하고 현재 부재만 전달한다. 최근 lastFaultCode를 현재 code로 사용하지 않는다. FaultInfo에는 occurrence/age/generation/전체mask가 없으므로 그것만으로 개별 발생 Event나 전체 진단 반영을 주장하지 않는다. 전체 원기록은 StateReport/M09에 남으며 callback은 현재 요약을 소비하는 조건이다.

실제로 읽은 로컬 참고 source는 `docs/Domain/architecture/2_Domain_interface/Domain_Interface_v0.5.{h,c}`, `1_Vehicle_Types/Vehicle_Types_v0.2.h`, `3_VehicleStateManager/VehicleStateManager_v0.2.c`다. v0.5 StateReport에는 service/lastFaultCode/faultQuality만 있으며 `RxVssState`(C 1302~1341)는 **service만** VSM에 전달한다. VSM UpdateVss는 service 사본과 그룹 갱신을 수행한다. `RxVssFault`(C 1343~1363)는 observer 부재 시 NOT_AVAILABLE, 존재하면 **observer 반환을 그대로 반환**한다. 이 Fault 경계에는 선행 Core commit이 없다. 참고 source가 설치된 제품이라는 증거는 없다.

따라서 실제 build의 프로파일을 E-S05에서 확인한다. 확장 C2 전체 경로와 v0.5 서비스 한정 경로를 혼합하지 않는다. v0.5에 확장 필드를 cast/추가하지 않는다. v0.5 동작/서명과 service meta의 소비 계약이 확인된 프로파일에서는 확인된 service만 수용할 수 있고 Fault 이력/세대 전체 전달은 미지원으로 별도 기록한다. Fault consumer가 없다는 이유만으로 그 State 수용까지 막지 않는다. 전체 이력 소비 PASS로 확대하지 않는다.

State OK 이후 Fault NOT_AVAILABLE/QUEUE_FULL/REJECTED 등은 State 성공과 Fault 원 반환을 각각 보존하며 전체 rollback/재호출하지 않는다. 반환 aggregate는 State 호출 실패 시 그 원 반환, State 성공 후 Fault 호출 시 그 원 반환이다. 호출하지 못한 단계는 실제 raw 반환이 없다고 표시하고 기존 NOT_AVAILABLE로 aggregate만 반환한다. candidate C2 7값(UNSUPPORTED 포함)과 v0.5의 6값을 구분하며 임의 enum을 기존 header에 추가하지 않는다.

M09는 원 보고별 State/Fault의 미호출·수용/반영 확인·결과 불명·실패를 별도로 추적한다. 이미 수용한 경계는 같은 key로 재호출하지 않는다. 미호출 또는 명시 미수용이 증명된 경계만 준비 변화 뒤 1회 재검토하며 원 report의 신선도/세대가 아직 유효하고 DOMAIN이 그 경계의 재시도 무부작용을 보장해야 한다. 반환 REJECTED/QUEUE_FULL만으로 미수용을 추정하지 않는다. observer가 부작용 뒤 실패했거나 결과가 불명인 경우 자동 재호출 금지다. 더 새 보고가 수용되면 옛 미수용 Fault 재시도는 종료하고 실패/미전달 원 key를 남긴다. 옛 current 요약으로 새 State를 역행시키지 않는다. old 비동기 쓰기 종료가 미확인이면 해당 VSS 경계만 새 호출을 보류한다. 새 보고는 새 key로 처리하되 observer는 상태 요약의 반복을 새 Fault occurrence로 해석하지 않는다. M09는 현재 처리/부분 미결 1건과 기존 bounded 진단/손실 latch를 사용한다. 원 순서 high-water/기동 fencing을 유지하고 참조 종료·결과 관측 이전 뒤 회수하며 보호 공간 부족은 호출 전 NOT_AVAILABLE이다.

Network 기존 classifier는 Core commit 후 Observer 모델이므로 v0.5의 observer-only Fault 결과에는 억지 APPLIED를 넣어 통과시키지 않는다. raw 반환·callback 호출 여부·확인된 consumer 증거를 M09에 직접 보존하고 Core commit은 해당 없음으로 기록한다. State source 프로파일의 실제 commit 보장도 버전/등록/직렬화 증거와 함께 사용한다. callback OK는 물리 효과나 이후 비동기 처리 완료가 아니다. 후발 실패는 같은 원 보고에 연결 가능한 DOMAIN 관측이 없으면 최종 성공 미확인으로 남는다.

<a id="external"></a>
## 7. 남은 외부 적용 조건

| ID / owner | 반드시 받을 입력 증거 | 확인할 출력·미충족 영향 |
|---|---|---|
| E-S01 VSS runtime/구현 | 실제 entry/직렬화, HEALTH 원기록·2슬롯·stamp 고정 용량, 진단 누락 검출, 공개 원자성/참조 종료 | 완전 current·원 이력·CREATED0/1·counter 원자성. 불충족은 해당 생산/인계 제한 |
| E-S02 VSS/Network 기동 | 실제 VSS Boot 생성·보존·재사용/wrap 방지, HEALTH generation과 CAN BOOT의 대응, Network 단독 reset 순서 | old 보고 격리와 같은 원 세대의 송수신; 자동 재명명 금지 |
| E-S03 Runtime/DOMAIN 사용 owner | clock domain/epoch·구간·elapsed 산술, source 사용 기한과 경계, 포화 하한 소비, VSM meta age 경과 처리 | 신선한 State/진단 투영·기한 후 비현재 처리; 미확인 consumer만 fresh 사용 제한 |
| E-S04 Network/INPUT | 실제 PublishStatus 복사/게시·terminal 관측/sink 필드·drain 경계, 원 key·raw 보존, bounded overflow/참조 종료 | localOK·후발 실패 구분, FLOW 후속. sink 미준비 NOT_READY |
| E-S05 DOMAIN 통합 | 설치 header/source 버전·호출 직렬화, VSM 수용 단계, onVssFault 등록/수명/side effect/반환별 미수용·재시도 보장, 확장 이력/세대 저장 소비, 비동기 최종 실패 상관 경로 | State/Fault 별 수용/관측 증거. service 한정/전체 투영을 구분; 불명 결과 재호출 금지 |

이는 VSS/Network 내부 선택을 다시 미결로 돌리는 조건이 아니다. A1 E01~E05·WARNING60·Event L·Hold 확정/UNCERTAIN·Rear Fence/BOOT·원본 복구 계약은 그대로다. 제품/Host/Build/Flash/실보드·Commit/Push/PR은 수행하지 않았고 A3를 시작하지 않는다. 종료는 `A2_SELF_REVIEW_COMPLETED / INDEPENDENT_REVIEW_PENDING`이다.
