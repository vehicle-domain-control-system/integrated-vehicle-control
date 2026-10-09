# AUDIO CONTROL Module — 장치별 구성 준비

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/30_DRIVER_LAYER.md)

## 1. 목적

현재 출력 구성의 Codec·Generator·Reference별 준비 근거를 유지하고 유효한 제어 결과를 반환한다.

## 2. 역할

AUDIOCTRL은 세 장치의 control 문맥과 readiness를 각각 관리한다. 한 장치의 성공을 다른 장치의 준비 사실로 확장하지 않는다.

## 3. 책임

- 현재 구성과 요구에 맞는 장치별 준비를 확인하고 필요한 control을 등록한다.
- 해당 장치·구성 결과와 reset/reconfig/오류의 무효화 범위를 적용한다.
- 허용된 범위의 실제 장치 제어/복구 결과를 AUDIO STREAM에 반환한다.

## 4. 책임이 아닌 것

Asset/provider/A/B 준비, PCM 전송/출력의 종료·차단 확인 조건(fence), PLAYBACK Session과 전체 종료, HEALTH Fault 해제를 맡지 않는다. mute/control 수락을 출력 시작/미시작/전체 종료의 근거로 판정하지 않는다.

## 5. 소유 상태 / 데이터

장치별 해당 제어 요청의 문맥·현재 구성 readiness의 단일 writer는 AUDIO CONTROL이다.

| 장치 책임 | 현재 구성에서 확인할 사실 |
| --- | --- |
| SGTL5000 Codec | Codec 설정/준비 |
| CS2100 Clock Generator | Generator 구성/상태 |
| Clock Reference | Reference source 준비 |

## 6. 입력

AUDIO STREAM의 해당 장치·구성 준비·제어·허용된 복구 요구와 HAL의 해당 제어 요청에 연결된 사실을 받는다.

## 7. 출력

장치/구성별 확인·미평가·실패·무효화 사실과 실제 제어 결과를 AUDIO STREAM에 반환한다. startup 미평가와 확인된 실패는 구분한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)

현재 Core는 [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)다. 요청·후속 진행은 같은 현재 장치 구성의 준비/무효화 수명 안에서 다룬다. 장치별 wrapper나 새 Attempt마다의 재초기화를 추가하지 않는다.

## 9. 의존 Module

AUDIO STREAM → AUDIO CONTROL → HAL / 필요한 RUNTIME이다. AUDIO TX와의 준비/전송 조율은 AUDIO STREAM이 담당한다.

## 10. 상태 전이 또는 주요 lifecycle

현재 요구에 대응하는 유효 준비를 확인하고 필요한 장치 control을 등록한다. 실제 확인 결과만 해당 장치·구성에 적용한다. 한 장치의 성공으로 다른 readiness를 추정하지 않는다. reset/reconfig/오류는 영향을 받는 준비 근거를 무효화한다.

system readiness는 Session보다 오래 유지될 수 있어 매 Attempt마다 전체 재초기화하지 않는다. 유지된 ready는 시도 준비의 참조이며 새 Asset/provider/A/B 확인을 대신하지 않는다. 옛 control 성공이 새 구성을 ready로 만들면 안 된다. control context 저장 회수와 유효한 readiness lifetime도 별개다.

## 11. Module-local Contract

현재 요구에 대응하는 해당 장치·구성에만 결과를 적용한다. 한 장치의 성공으로 다른 readiness를 추정하지 않고, 옛 control 성공은 새 구성을 ready로 만들 수 없다. 유지된 system readiness는 새 Asset/provider/A/B 확인을 대체하지 않는다.

## 12. 대표 오류 / 예외 처리 원칙

제어 실패/미확인은 해당 장치·구성 범위로 보고한다. mute/control 접수 성공은 실제 시작·미시작·전체 종료 증명이 아니다. 늦은 옛 제어 결과는 새 readiness를 복원하지 않는다.

## 13. 관련 Function 문서

[AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Codec / Clock Readiness](../40_DATA/60_AUDIO_TX_DATA.md#control) · [HAL이 요청 전에 저장한 귀속 정보의 수명](../40_DATA/60_AUDIO_TX_DATA.md#tx)

## 15. 관련 공통 Contract

[Ownership](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [Async](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

공통 원문의 위치를 연결한다. 여러 Module에 동일한 규칙의 통합·중복 제거는 R4에서 수행한다.

## 16. TBD / Deferred

실제 순서·source/ratio·register·API는 [HW TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware)를 확인한다. callback 등록/저장 표현과 실제 C 파일/prototype·Header는 후속 단계로 남긴다.
