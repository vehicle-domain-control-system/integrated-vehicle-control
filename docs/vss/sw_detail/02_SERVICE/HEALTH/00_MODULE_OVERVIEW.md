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

## 담당 Core의 예정 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-17 | `Health_Evaluate` — 동기 진단·현재 대상의 복구 허용/해제 적용 | `02_SERVICE/HEALTH/functions/Health_Evaluate.md` | `02_SERVICE/HEALTH/pseudocode/Health_Evaluate.md` | PLANNED_NOT_WRITTEN |
| CORE-18 | `Health_BuildStatus` — 동기 원본 읽기 전용 파생 보고 | `02_SERVICE/HEALTH/functions/Health_BuildStatus.md` | `02_SERVICE/HEALTH/pseudocode/Health_BuildStatus.md` | PLANNED_NOT_WRITTEN |

경로는 `docs/vss/sw_detail/` 기준입니다. 설명·의사코드 본문은 아직 없으므로 위 경로는 클릭 링크가 아닙니다. 실제 동작은 아래 동결 Function 원문에서 읽고, 새 함수쌍은 각 단계 검수 뒤 작성합니다.

## 후속 진행 후보와 미정

current/recent 구분·현재 대상/permission 검사·coherent CREATED/REEVALUATE는 독립 SW/Host 후보입니다. Q08~10의 실제 DTO·Domain 소비·초기 전체 reduction, exact cause/action와 물리 효과 검증은 미정입니다. HEALTH가 새 CAN 송신 API를 만들지 않습니다. 함수쌍은 D6 예정입니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 HEALTH Module의 책임·lifecycle](../../../design/20_MODULES/26_HEALTH_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/70_DIAGNOSTIC_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
