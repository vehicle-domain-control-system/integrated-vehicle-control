# VSS / SERVICE / HEALTH

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

현재 출력 제한과 대상별 복구 판단을 유지하고 반영 완료된 owner 관측에서 일관된 VSS_STATUS를 파생합니다. 입력 품질 상실과 출력 Fault, 초기 미평가와 확인 실패를 구분합니다.

## 상태 owner와 입출력

Health_Evaluate는 현재/최근 Fault·허용·제한의 writer입니다. Health_BuildStatus는 원본을 읽어 파생 보고만 만들며 Fault/STORE ledger/PLAYBACK Session의 두 번째 writer가 아닙니다. 기존 Fault 5종을 유지합니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | FLOW가 연결한 원발생 주체·원인·대상/구성·사실 시각·반영 완료 관측, 복구 수행과 별도 효과 검증 결과 |
| 출력 | 현재 제한·대상별 복구 허용/해제 판단과 VSS_STATUS 묶음을 FLOW에 반환 |
| 의존 | FLOW → HEALTH → 필요한 RUNTIME; HW 복구/Network 송신 직접 호출 없음 |

## 인접 관계와 비동기 경계

VSS_STATUS_CREATED는 일관된 관측의 새 보고 생성입니다. VSS_STATUS_REEVALUATE에서는 부족/혼합·미정 reduction을 정상값으로 보완하지 않습니다. 보고 생성 → FLOW → INPUT → Node Communication → Domain 소비는 다른 책임이며 송신·수용 완료를 추정하지 않습니다. 복구 허용·수행·효과 검증·HEALTH 제한 해제도 구분합니다.

[FLOW의 보고 조율](../../01_APP/FLOW/00_MODULE_OVERVIEW.md) · [INPUT의 통신 연결](../INPUT/00_MODULE_OVERVIEW.md)

<a id="startup-fault-recovery-status"></a>
## Startup에서 Fault·Recovery·Status까지 읽기

아래는 기존 Core를 잇는 읽기 순서입니다. 장치 제어·진단·보고를 하나의 실행 함수로 합치거나 초기화 순서를 새로 정하지 않습니다.

| 흐름 | 판단 주체·정상 경로 | 부족·실패 시 남기는 것 |
|---|---|---|
| 초기 처리 기회 → STREAM → CONTROL | [FLOW startup](../../01_APP/FLOW/functions/Flow_Process.md#startup-recovery)이 Session 없이 이미 수용한 장치 제어의 진행을 연결합니다. [CONTROL](../../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#devices)이 Codec/Generator/Reference 각각의 현재 configuration readiness를 반영합니다. | UNASSESSED/NO_DATA는 확인된 INITIALIZATION_FAILURE가 아닙니다. 초기 제어 entry·순서와 전체 초기 Availability는 미정이며 반복 INITIAL 알림으로 HW 준비를 재발행하지 않습니다. |
| 원 실패·품질 관측 → FLOW → HEALTH 평가 | 실제 발견 producer/stage/cause/target/configuration/operation/occurredAt을 보존해 [Health_Evaluate](functions/Health_Evaluate.md#diagnostics)에 전달합니다. 확인된 실패만 기존 Fault 5종의 승인된 분류 근거로 판단합니다. | INVALID/STALE/NO_DATA는 정상 CLEAR·출력 Fault·실제 종료로 변환하지 않습니다. exact cause 매핑이 없으면 원진단과 제한 판단 미결을 보존합니다. 반영된 불명확 최종의 PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE은 별도로 유지합니다. |
| 현재 HEALTH 허용 → FLOW 안전 연결 | [현재 permission](functions/Health_Evaluate.md#permission)의 permission/faultInstance/faultRevision/target/configuration/action을 확인하고 old 전체 retirement·provider/PCM/device 위험 종료·독립 late 귀속 보호를 확인합니다. | old 위험이 남거나 허용이 철회·변경되면 기존 정리와 후속을 유지합니다. Session 없는 복구에 가짜 Session/Attempt를 만들지 않습니다. |
| STREAM Recovery → Backend/CONTROL 실제 수행 → 별도 효과 검증 | [RequestControl의 Recovery lane](../AUDIO_STREAM/functions/AudioStream_RequestControl.md#recovery)으로 연결하고 [Advance](../AUDIO_STREAM/functions/AudioStream_Advance.md#recovery)가 원 performed와 verified를 각각 전달합니다. 효력 직전 현재 허용·대상·안전 조건을 다시 확인합니다. | 수용/receipt/ACK는 수행 성공이 아니며 performed 성공도 verified 성공이 아닙니다. action 매핑·물리 효과 기준이 미정이면 해당 수행/성공 판정만 보류합니다. |
| 원 pair → HEALTH 현재 한 대상의 제한 해제 → FLOW 재평가 | [pair 대조](functions/Health_Evaluate.md#pair)에서 현재 instance/revision·permission·target/configuration/action, 실제 operation과 수행 이후 별도 검증의 원 시간·효과 충분성을 확인합니다. HEALTH만 해당 제한을 해제하고 다른 current/recent를 보존합니다. | 한쪽 fact만 있거나 old/다른 대상·operation·시간 인과가 불일치하면 clear하지 않습니다. 복구 뒤 원 age/Hold/replay를 갱신하거나 Started One-shot을 부활시키지 않고 현재 후보를 별도로 재평가합니다. |
| 새 보고 basis → BuildStatus → FLOW → INPUT → Node Communication | 변경 반영 뒤 보고용 관측을 새로 수집하고 [BuildStatus](functions/Health_BuildStatus.md#coherence)가 read-only로 파생합니다. CREATED의 불변 Snapshot·Fault 의미 목록·basis 참조를 [실제 소비 수명](functions/Health_BuildStatus.md#snapshot)까지 보호합니다. | 부족/혼합·미정 reduction이면 REEVALUATE로 새 Snapshot을 공개하지 않습니다. 논리 Snapshot producer는 HEALTH이나 실제 VSS_STATUS 송신 writer·직렬화/DTO 변환 위치·Domain consumer는 TBD입니다. Getter/CREATED가 송신·Domain 수용 완료를 뜻하지 않습니다. |

정상 idle·cue 무음·반복 대기·선택할 유효 사건이 없는 상태만으로 통신 오류나 출력 Fault를 만들지 않습니다. 적법 actual이 확인된 전체 Session이 retirement 전이면 무음 구간·시작 후 종료 진행도 기존 PLAYING 의미에 포함되며, state 외 필수 보고 근거가 부족하면 전체 보고는 REEVALUATE입니다. [기존 Status 분기](functions/Health_BuildStatus.md#states)를 따릅니다.

[HAL의 원등록·raw·손실과 CONTROL 후속](../../04_HAL_BSP/01_HAL_BSP_BOUNDARIES.md#raw-time-loss) · [공통 시간과 의미 owner](../../05_INFRA/00_LAYER_OVERVIEW.md#startup-recovery-time) · [동결 Startup](../../../design/60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup) · [동결 Fault/Recovery](../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#recovery-flow)

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-17 | `Health_Evaluate` — 동기 진단·현재 대상의 복구 허용/해제 적용 | [상세 설명](functions/Health_Evaluate.md) | [한국어 의사코드](pseudocode/Health_Evaluate.md) | D6-1 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING |
| CORE-18 | `Health_BuildStatus` — 동기 원본 읽기 전용 파생 보고 | [상세 설명](functions/Health_BuildStatus.md) | [한국어 의사코드](pseudocode/Health_BuildStatus.md) | D6-1 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING |

CORE-17/18 설명·의사코드는 위 링크로 왕복합니다. D6-1 자체검수·독립 검수 대기이며 실제 동작의 정본은 아래 동결 Function 원문입니다.

## 후속 진행 후보와 미정

current/recent 구분·현재 대상/permission 검사·coherent CREATED/REEVALUATE는 독립 SW/Host 후보입니다. Q08~10의 실제 DTO·Domain 소비·초기 전체 reduction, exact cause/action와 물리 효과 검증은 미정입니다. HEALTH가 새 CAN 송신 API를 만들지 않습니다. 두 함수쌍은 D6-1 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING입니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 HEALTH Module의 책임·lifecycle](../../../design/20_MODULES/26_HEALTH_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/70_DIAGNOSTIC_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
