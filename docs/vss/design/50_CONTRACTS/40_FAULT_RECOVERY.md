# Fault / Recovery — 현재 제한, 허용, 수행, 검증과 해제

> R4 · 2026-10-08 · 기존 Fault 5종과 owner 유지 · 복구 효과/실측 충분조건은 TBD

[Contract Overview](00_CONTRACT_OVERVIEW.md#fault)

HEALTH의 복구 허용, FLOW의 안전 연결, Backend의 실제 수행, 별도 효과 검증, HEALTH의 현재 제한 해제는 다른 단계다. 이 계약은 기존 typed 자료를 연결하며 새로운 Fault 분류·retry matrix·복구 manager를 만들지 않는다.

<a id="taxonomy"></a>
## 1. 기존 Fault 다섯 종류를 유지한다

| 기존 Fault | 기존 적용 영역 |
| --- | --- |
| `SOUND_ASSET_UNAVAILABLE` | 필요한 저장 음원 사용 불가 |
| `PLAYBACK_START_FAILURE` | 재생 시작 실패 |
| `AUDIO_OUTPUT_FAILURE` | 공통 Audio Output Path 이상 |
| `PLAYBACK_STATE_FAILURE` | 출력 owner/상태 정합성 이상·시작 결과 불명확 최종 포함 |
| `INITIALIZATION_FAILURE` | 확인된 초기화 실패. startup 미평가와 구별 |

각 원본 실패 fact의 producer와 code·target·범위는 보존한다. HEALTH가 기존 정책으로 현재 Fault·가용성·보고를 파생하며 exact raw cause→Fault/복구 action 매핑은 기존 TBD다. 하나의 enum 값이 모든 원인을 동일한 복구로 바꾸는 근거는 아니다.

입력 invalid/stale/누락·아직 평가되지 않은 초기 입력·WINDOW `[구현 보류 — 설계 유지]` 자체를 신규 Fault나 DEGRADED로 분류하지 않는다. 기존 시작 결과 불명확 최종의 `PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE` 경로는 [Async §4](20_ASYNC_RESULT_EVIDENCE.md#partial-and-uncertain)와 PLAYBACK 로컬 규칙을 유지한다.

<a id="current-target"></a>
## 2. 복구 허용은 현재 instance와 대상에만 유효하다

`VssRecoveryPermission`은 HEALTH가 만든 불변 `permission`·`faultInstance`·`faultRevision`·`target`·`configuration`·`action` 의미다. 같은 Fault 종류라는 이유만으로 옛 허용을 새 instance/구성에 적용하지 않는다. 허용 철회·새 대상/Fault·구성 변화가 있으면 옛 허용을 무효화하고 수행 전에 현재 조건을 재확인한다.

복구는 Session 없이도 성립한다. HEALTH→FLOW→`AudioStream_RequestControl` 경로의 identity는 위 Fault/permission 문맥이며 PLAYBACK START/STOP의 Session/Attempt를 빌리거나 가짜 값을 만들지 않는다. 두 typed 경로의 독립 국소 PASS는 [함수 연결](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol)에 기록한다. 실제 C entry point·인자 optional/표현은 R6 TBD다.

수행 결과와 효과 검증도 같은 허용/원래 대상·구성·수행 operation에 연결한다. 옛 성공·다른 target·단순 현재 ready는 현재 Fault의 복구 성공이 아니다. 원본/보고의 writer와 수명은 [Ownership §1~2](10_OWNERSHIP_LIFETIME.md#identity-lifetimes)를 따른다.

<a id="recovery-flow"></a>
## 3. 허용 → 안전 연결 → 수행 → 별도 검증 → 제한 해제

| 단계 / 기존 owner | 필요한 근거와 로컬 책임 | 이 단계가 확정하지 않는 것 |
| --- | --- | --- |
| 허용 — HEALTH | 현재 instance/revision/target/config/action의 제한된 복구 허용 | HW 수행·효과 확인·Fault clear |
| 안전 연결 — FLOW | 옛 전체 출력 retirement와 provider/PCM·관련 장치 참조의 안전 종료, 현재 허용 확인 | 자료 보호가 남은 옛 callback 기록의 일괄 폐기 |
| 요구 적용 — AUDIO STREAM | typed Recovery를 원래 허용으로 보존·현재 대상/안전 조건 재확인. 필요한 기존 Backend/Driver 경계로 연결 | Session 생성·상위 replay 권한 |
| 실제 수행 — Backend/DRIVER | 원래 허용의 기존 action 범위만 수행하고 수행 operation/대상·구성의 결과 보존 | 요청 수용만으로 `performed`·복구 완료 |
| 별도 효과 검증 — 해당 실제 경계 | `VssRecoveryVerificationEvidence`를 수행 operation/동일 문맥에 연결, 실제 효과의 충분 근거 확인 | 수행 성공만으로 `verified`·HEALTH 제한 해제 |
| 제한 해제 — HEALTH | 현재 허용/instance/target/revision/구성과 수행·검증 일치를 확인해 해당 현재 제한만 해제 | 다른 현재 Fault·최근 Fault 기록 삭제·후속 재생 시작 |
| 현재 재평가 — FLOW/원본 owner | 새로 적용 완료된 관측으로 현재 후보·가용성·출력 권한 판단 | 원 발생 age/Hold 갱신·Started One-shot 부활 |

수행/검증이 실패·부족·손실이면 해당 결과·진단과 현재 제한을 보존한다. 자동 재시도 횟수·delay·reset 순서나 모든 Fault의 일괄 clear를 새로 정하지 않는다. HEALTH는 보고/제한/허용 writer이며 직접 HW recovery를 수행하지 않는다. FLOW도 DRIVER 원본을 수정하지 않고 기존 Backend 요구로 연결한다.

출력 retirement와 압축/PCM/HAL 기록 보호 종료는 서로 다른 판단이다. 옛 불변 기록이 안전하게 독립 보호된 채 남을 수 있지만 실제 provider/PCM/device의 위험한 옛 접근/출력이 남은 상태를 복구 수행 허가로 쓰지 않는다. [Ownership §4](10_OWNERSHIP_LIFETIME.md#references-and-release)와 [Buffer §5](50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries)를 함께 확인한다.

<a id="resume-eligibility"></a>
## 4. 복구 뒤 재생은 현재 후보를 새로 판단한다

복구 성공은 원 발생을 다시 만들거나 Hold·UseLimit·first-start age를 리셋하지 않는다. Started One-shot·불명확 최종·replay 금지 발생을 정상 Pending으로 되살리지 않는다. 미시작 One-shot의 재평가는 충분한 NO_START와 기존 replay/최종 조건, 아직 유효한 원본 freshness가 함께 성립할 때만 가능하다.

현재 유효·Hold 내 Stateful은 옛 출력 retirement 뒤 새 Session에서 전체 정책 처음부터 재선택할 수 있다. 옛 PCM offset 재개나 복구 성공만으로 Rear의 옛 위험 후보 부활은 없다. 입력 품질과 실제 Fault 가용성은 별도 원본이며 [Timing §3~4](30_TIMING_FRESHNESS.md#first-start), STORE/PLAYBACK의 local 조건을 유지한다.

<a id="report-and-tbd"></a>
## 5. 보고와 남은 결정

HEALTH 보고는 관측한 STORE·PLAYBACK·장치/복구 사실에서 파생한다. 현재 Fault·최근 Fault·현재 제한/가용성은 구별하며 보고 읽기는 원본을 새로 반영하거나 해제하지 않는다. instance 보존/회수·key alias 방지·진단 축소 정책은 미정이다.

[Fault/복구 자료](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [HEALTH 로컬](../20_MODULES/26_HEALTH_MODULE.md#recovery) · [FLOW 연결](../20_MODULES/10_FLOW_MODULE.md) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

exact cause 매핑·대상별 허용/수행/검증 충분조건·reset/reconfig와 출력/자료 안전의 물리 보장·HEALTH 현재 제한 해제의 상세 판정은 B2-R/정책 TBD다. 실제 API·Header/ABI·storage·직렬화/보존 수단은 R6/Binding TBD로 남긴다.
