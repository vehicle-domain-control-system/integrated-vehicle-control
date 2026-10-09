# HEALTH Module — 진단·복구 판단·파생 보고

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

현재 출력 제한과 복구 판단을 유지하고 반영이 완료된 원본 사실에서 일관된 VSS_STATUS를 파생한다.

## 2. 역할

HEALTH의 진단·복구 판단 책임(C11)은 입력 진단·현재/최근 주요 Fault·recoverable 판단과 허용 범위·대상 제한 해제를 맡는다. HEALTH의 보고 책임(C12)은 진단·복구 판단 결과(C11)와 STORE/PLAYBACK/Backend 관측에서 외부 보고를 만든다.

## 3. 책임

- 사실을 생성한 주체·원인·단계·대상/구성·사실 시각을 보존해 진단과 현재 제한을 평가한다.
- 현재 대상에 한정된 복구 허용과 수행 뒤 효과 검증에 따른 제한 해제를 판단한다.
- 반영된 관측과 제한으로 서비스 수준·Availability·accepting 보고를 파생한다.

## 4. 책임이 아닌 것

HW/Backend 복구 수행, Network 송신 실행, STORE ledger·PLAYBACK Session 변경을 맡지 않는다. HEALTH의 보고 책임(C12)은 Fault/ledger/Session의 두 번째 writer가 아니다. 입력 STALE/INVALID/미수신을 출력 Fault로 자동 전환하지 않는다.

## 5. 소유 상태 / 데이터

현재/최근 Fault·허용 범위·제한의 authoritative writer는 HEALTH의 진단·복구 판단 책임(C11)이다. HEALTH의 보고 책임(C12)이 소유하는 보고는 현재 원본 사실에서 파생되며 원본 상태가 아니다. Asset metadata/read와 AUDIO STREAM(C09)의 decode의 발견 단계를 보존한다.

| 기존 Fault | 의미 |
| --- | --- |
| `SOUND_ASSET_UNAVAILABLE` | 필요한 저장 음원 사용 불가 |
| `PLAYBACK_START_FAILURE` | 재생 시작 실패 |
| `AUDIO_OUTPUT_FAILURE` | 공통 Audio Output Path 이상 |
| `PLAYBACK_STATE_FAILURE` | 상태/출력 owner 정합성 이상, 시작 결과 불명확 최종 포함 |
| `INITIALIZATION_FAILURE` | 확인된 초기화 실패 |

## 6. 입력

FLOW가 연결한 진단 자료의 발생 주체·원인·대상/구성·사실 시각과 현재 owner의 반영 완료 관측을 함께 판단 가능한 일관된 자료로 받는다. Backend가 복구를 실제 수행한 뒤 효과를 검증한 결과도 받는다.

## 7. 출력

FLOW에 진단/제한·현재 대상의 복구 허용/해제 판단과 VSS_STATUS 묶음을 반환한다. 보고 전달은 HEALTH → FLOW → INPUT의 통신 연결 책임(C02) → 기존 Node Communication이며 입력/송신 Module이 보고값을 독자 계산하지 않는다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

현재 Core는 진단·복구 판단 책임(C11)의 [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)와 원본 읽기 전용 보고 책임(C12)의 [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)다. 판단 writer와 파생 보고의 Writes를 분리하고 복구 단계별 wrapper는 만들지 않는다.

## 9. 의존 Module

FLOW → HEALTH → 필요한 RUNTIME이다. STORE/PLAYBACK/Backend의 반영된 관측은 FLOW가 연결한다. HEALTH가 HW 복구/Backend·Network를 직접 호출하는 의존은 없다.

<a id="recovery"></a>
## 10. 상태 전이 또는 주요 lifecycle

복구 단계·현재 permission/instance/target·수행/별도 검증·재평가 조건은 [Fault §2~4](../50_CONTRACTS/40_FAULT_RECOVERY.md#recovery-flow)를 따른다. HEALTH의 진단 책임(C11)은 허용과 해당 현재 제한 해제의 writer이며 FLOW/Backend가 실제 수행을 맡는다.

복구 요구사항(PER-009)은 recoverable 출력 Fault에 한정하며 PLAYBACK_STATE_FAILURE도 원인에 따라 포함될 수 있다. 다른 현재 Fault·최근 기록은 유지하고 복귀 뒤 재생 자격은 [현재 후보 재평가](../50_CONTRACTS/40_FAULT_RECOVERY.md#resume-eligibility)에서 확인한다.

현재 제한은 해당 대상의 해제 조건까지, 최근 기록은 기존 보존 정책까지 유지한다. 파생 보고는 해당 관측의 일관성과 전달/참조 기간을 따른다.

## 11. Module-local Contract

<a id="status"></a>
### 파생 상태 보고의 로컬 기준

| 보고 의미 | 판단 기준 |
| --- | --- |
| STARTUP / READY | 정상 초기화·미평가 / 정상 준비이나 실제 시작된 활성 재생 미확인. 미평가를 초기화 실패로 바꾸지 않음 |
| PLAYING | 적법한 실제 시작이 확인된 Session. cue 무음·반복 대기·시작 후 종료 진행 포함 |
| FAULT / UNAVAILABLE | 정상 출력 보장 불가와 새 start 차단. QUARANTINED의 고정 연결은 [PLAYBACK](23_PLAYBACK_MODULE.md#outcomes) 기준 |
| FULL / DEGRADED | 지원 서비스 수준. 음원 일부 제한과 공통 경로 불가를 구분하며 개별 start 허가는 아님 |
| accepting | 입력·검증·저장 자원·시간/출처·replay 준비의 종합. true는 개별 수용 보장이 아님 |

accepting=false여도 추적 중 duplicate·유효 CLEAR·품질/기한의 안전 경로는 진행한다. Audio Fault와 입력 수용 가능성은 별도로 평가한다. WINDOW 보류는 [INPUT](20_INPUT_MODULE.md)의 기준을 따른다. 초기 Availability와 전체 보고 조합/reduction의 미정 정책은 TBD다.

## 12. 대표 오류 / 예외 처리 원칙

startup 미평가와 확인 실패, 입력 품질 상실과 출력 Fault, 옛 복구 결과와 현재 대상을 구분한다. 대표 출력 실패는 기존 5종에 연결하며 새 retry/fallback matrix를 추가하지 않는다. WINDOW 보류는 출력 Fault/DEGRADED의 자동 근거가 아니다.

## 13. 관련 Function 문서

[Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Fault / Recovery](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [VSS Status Snapshot](../40_DATA/70_DIAGNOSTIC_DATA.md#status)

## 15. 관련 공통 Contract

[Fault / Recovery](../50_CONTRACTS/40_FAULT_RECOVERY.md) · [Ownership](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md)

공통 규칙은 위 R4 계약을 따르며 이 문서의 owner·상태 적용·실패/예외는 로컬 조건으로 유지한다.

## 16. TBD / Deferred

exact cause→Fault/DIA-017/Recovery Action·해제/Availability 정책은 [정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)를 확인한다. 초기 Availability·전체 보고 조합/reduction은 미정이다. 실제 보고 표현·필드/타입·Header는 후속 단계다.
