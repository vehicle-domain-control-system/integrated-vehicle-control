# VSS / DRIVER / AUDIO_CONTROL

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE. 기능 탐색 안내. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

현재 출력 구성에 대한 Codec·Generator·Reference 각각의 준비 근거와 제어 진행을 유지합니다. 한 장치의 성공을 다른 장치의 readiness로 확대하지 않습니다.

## 상태 owner와 입출력

AUDIO_CONTROL은 장치별 control 문맥과 현재 구성 readiness의 단일 writer입니다. 준비 근거의 수명은 Session과 다를 수 있고, PCM 전송·출력 종료·HEALTH Fault 해제를 소유하지 않습니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | AUDIO_STREAM의 장치/구성 준비·제어·허용 복구 요구, HAL의 해당 제어 요청 사실 |
| 출력 | 장치/구성별 확인·미평가·실패·무효화와 실제 제어 결과를 AUDIO_STREAM에 반환 |
| 의존 | AUDIO_STREAM → AUDIO_CONTROL → HAL 및 필요한 RUNTIME; TX와의 조율은 AUDIO_STREAM |

## 인접 관계와 비동기 경계

reset/reconfig/오류는 영향받는 준비 근거를 무효화합니다. 옛 구성의 늦은 성공이 새 구성을 ready로 만들지 않습니다. 기존 system readiness를 매 Attempt마다 전부 재초기화하거나 새 Asset/provider/A/B 확인 대신 쓰지 않습니다. mute/control 접수는 actual/no-start/전체 종료 증명이 아닙니다.

[AUDIO_STREAM의 조율](../../02_SERVICE/AUDIO_STREAM/00_MODULE_OVERVIEW.md) · [HAL/BSP 경계](../../04_HAL_BSP/00_LAYER_OVERVIEW.md)

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 상세 설명 | 한국어 의사코드 |
|---|---|---|---|
| CORE-15 | `AudioControl_Service` — 현재 구성 준비·제어/무효화의 동기 진행·후속 결과 | [상세 설명](functions/AudioControl_Service.md) | [한국어 의사코드](pseudocode/AudioControl_Service.md) |

위 표에서 CORE-15의 설명·의사코드로 이동합니다. 원형과 writer·비동기 계약은 동결 정본을 따르며, 실제 C/HW 확인은 별도입니다.

## 후속 진행 후보와 미정

구성 identity·readiness 무효화·옛 결과 격리·수행/검증 구분은 독립 SW/Host 후보입니다. 실제 장치 제어 순서·source/ratio·register·API·효과 검증은 BINDING_TBD/HW_UNVERIFIED입니다. 기존 Codec/Clock 경계를 재사용 후보로 유지하고 새 장치별 wrapper를 선결정하지 않습니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. Q·R6 미합의와 외부 IF/HW·제품 결합 제한은 유지합니다.

## 현재 상세 근거

- [기존 AUDIO_CONTROL Module의 책임·lifecycle](../../../design/20_MODULES/31_AUDIO_CONTROL_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/60_AUDIO_TX_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
