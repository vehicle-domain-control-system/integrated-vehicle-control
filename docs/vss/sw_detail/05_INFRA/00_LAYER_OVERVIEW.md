# VSS / INFRA

[상위 VSS 지도](../01_VSS_MAP.md) · [APP](../01_APP/00_LAYER_OVERVIEW.md) · [HAL/BSP](../04_HAL_BSP/00_LAYER_OVERVIEW.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

INFRA의 RUNTIME은 비교 가능한 시간·연속성과 후속 처리/손실 근거를 제공합니다. 시간 도구의 유효성과 전달 손실을 소유하고, age·Hold·출력·복구의 의미 판단은 각 owner에 남깁니다. **공통 Boundary이며 별도 Module·Core가 없습니다.**

## RUNTIME 공통 경계

| 제공 근거 | 소비 책임 |
|---|---|
| 원본과 비교 가능한 시간·연속성 | STORE age/Hold, PLAYBACK 시작/정리, Backend·HEALTH 기한 |
| 후속 처리 기회와 coalesce/overflow·귀속 손실 | FLOW 조율과 각 owner의 원래 사실 반영 |
| 발생 주체가 남긴 marker 연결 | 관련 operation의 지연 비교. 처리 현재 시각으로 원사실을 재생산하지 않음 |

필요한 APP/SERVICE/DRIVER/HAL이 기존 공개 도구를 사용합니다. RX 없는 진행은 FLOW, 요청 전 귀속 정보에 따른 raw capture는 HAL 책임입니다. 알림만으로 actual/no-start/종료를 생성하거나 연속성 상실을 정상 성공·새 age로 보정하지 않습니다.

원 시간 비교·경계 걸침·손실 전달은 후속 독립 SW/Host 후보입니다. 실제 timebase·wrap/reset·포착 정밀도·직렬화/처리 예산과 보호 용량은 미정이고 성능 PASS도 미확정입니다. 이를 이유로 시간 의미 전체를 HOLD하거나 새 Timer/Queue/Mutex/Task를 추가하지 않습니다.

<a id="startup-recovery-time"></a>
## Startup·Recovery·보고에서 시간 근거를 소비하기

| 상황·공통 근거 | 판단 owner와 후속 |
|---|---|
| 초기 장치 준비·후속 기회 | FLOW → STREAM → CONTROL이 이미 수용한 원 operation/device/configuration을 진행합니다. Runtime 알림은 READY·초기화 실패·HW 완료가 아니며 [HEALTH 흐름](../02_SERVICE/HEALTH/00_MODULE_OVERVIEW.md#startup-fault-recovery-status)으로 원 관측을 연결합니다. |
| wrap/reset/epoch·시간 연속성 변화 | 비교가 입증된 문맥에서만 원 시각/가능 구간을 대조합니다. 비교 근거 부족은 부족 그대로, 실제 연속성 손실은 손실 범위 그대로 전달합니다. now/0/새 epoch로 원 발생·허용을 갱신하거나 임의 wrap 보정·공통 timeout을 만들지 않습니다. |
| 늦거나 순서가 뒤바뀐 수행·검증 | 원 occurredAt과 verifiedAt 및 domain/epoch·연속성·인과를 보존합니다. HEALTH가 수행 이후 별도 효과 검증과 현재 대상 일치를 확인하며 도착 순서·현재 처리 시각으로 verified 성공/clear를 합성하지 않습니다. |
| coalesce/overflow·원귀속/저장 손실 | 알림 병합은 fact 건수·consumer 반영 ACK가 아닙니다. 실제 부족/손실을 원 interval/binding에 한정해 HAL/Driver·FLOW/HEALTH 소비자에게 전달하고 해당 confirmed 판정을 막습니다. 다른 충분 범위와 미반영 사실은 보존하며 항상 성공하는 loss 저장소를 가정하지 않습니다. |
| HEALTH 변경 뒤 보고 | FLOW가 보고용 새 owner stamps/ReadBasis를 수집하고 BuildStatus가 coherence를 확인합니다. revision 숫자가 모두 같아야 하는 조건은 없으며 미반영/변경·필수 시간 부족이면 REEVALUATE입니다. Runtime이 숨은 Fault 적용·Recovery/Status 정책을 수행하지 않습니다. |

[HAL 원등록·raw 시간/손실](../04_HAL_BSP/01_HAL_BSP_BOUNDARIES.md#raw-time-loss) · [FLOW startup/recovery](../01_APP/FLOW/functions/Flow_Process.md#startup-recovery) · [HEALTH pair의 원 시간](../02_SERVICE/HEALTH/functions/Health_Evaluate.md#pair) · [Status 보고 basis](../02_SERVICE/HEALTH/functions/Health_BuildStatus.md#coherence)

## 현재 상세 근거와 다음 연결

[R1 INFRA](../../design/10_LAYERS/50_INFRA_LAYER.md) · [RUNTIME 시간·알림·marker](../../design/10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) · [R4 Timing/Freshness](../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md)

D6 공통 Boundary 연결에서 이 역할을 재사용합니다. 함수쌍 예정 목록에 INFRA 함수를 추가하지 않고 [FLOW](../01_APP/FLOW/00_MODULE_OVERVIEW.md)와 각 소비 Module에서 필요한 시간 근거를 따라갑니다.
