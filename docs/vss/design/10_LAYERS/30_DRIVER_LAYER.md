# Audio DRIVER Layer

> R1 Layer 설계 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Layer Overview](00_LAYER_OVERVIEW.md)

## 1. 목적

Audio Service의 중립 요청을 장치 사용과 원래 요청에 귀속된 근거로 연결한다.

## 2. 역할

AUDIO TX는 PCM 전송/장치 접근을, AUDIO CONTROL은 장치별 구성 준비/제어를 맡는다. vendor 세부는 HAL 경계에 격리한다.

## 3. 책임

- 장치가 PCM 버퍼에 접근할 수 있는 동안의 사용권 보호(retain)·실제 전송/출력을 위한 장치 활성화(arm)·소비·안전 반환·정리 근거를 관리한다.
- Codec/Generator/Reference의 해당 장치 구성 readiness를 관리한다.
- HAL raw 사실을 요청 범위에 적용해 AUDIO STREAM에 중립 결과를 반환한다.

## 4. 책임이 아닌 것

후보 우선순위·occurrence 최종·Session retirement·Fault 해제는 상위 책임이다. DRIVER는 Service 원본 상태를 직접 변경하지 않는다.

## 5. 포함 Module / 실제 내부 책임

[AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md)와 [AUDIO CONTROL](../20_MODULES/31_AUDIO_CONTROL_MODULE.md)의 2개 Module이다. 전송/제어의 원본 상태는 구분하고 AUDIO STREAM이 두 책임을 조율한다.

## 6. 입력 / 출력

| 구분 | 의미 |
| --- | --- |
| 입력 | AUDIO STREAM의 PCM 인계/start/정리 및 현재 구성 준비/제어 요구, HAL이 요청 전에 저장한 귀속 정보에 연결된 사실 |
| 출력 | 원래 출력 시도·버퍼 사용 회차·구성에 귀속된 장치 사용권 보호/전송·출력 후보/반환/정리와 readiness 근거 |

## 7. 허용 의존 방향

AUDIO STREAM → DRIVER → HAL / 필요한 RUNTIME이다. HAL은 보드/build 근거에 연결한다. 결과 반환은 같은 요청 경계이며 새로운 역방향 정책 의존이 아니다.

## 8. 금지 의존 방향

DRIVER → SERVICE의 정책 상태 직접 수정, Network/선택 정책 의존, AUDIO TX ↔ AUDIO CONTROL의 새 정책 호출을 금지한다. vendor 세부를 Service/APP에 노출하지 않는다.

## 9. Layer 수준 Contract

요청 수락과 실제 장치 효력을 구별하고 확인한 범위·해당 요청·출력 시도와의 연결·관측 부족을 보존한다. 장치 사실만으로 전체 Session/Fault 판단을 대신하지 않는다. 세부 충분조건은 AUDIO TX, 구성 유효성은 AUDIO CONTROL 상세를 따른다.

## 10. 대표 처리 흐름

AUDIO STREAM 중립 요청 → 각 Driver의 원래 요청 보호/등록 → HAL 요청·raw capture → 해당 버퍼 사용 회차/구성 결과 적용 → AUDIO STREAM 반환이다. 전송과 준비/제어는 같은 AUDIO STREAM이 조율하는 별도 책임이다.

## 11. Module / 하위 상세 링크

[AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md) · [AUDIO CONTROL](../20_MODULES/31_AUDIO_CONTROL_MODULE.md) · [HAL/BSP Layer](40_HAL_BSP_LAYER.md) · [등록/자원 상세](details/40_HAL_BSP_BOUNDARY.md)

## 12. 관련 공통 Contract

[Async / Evidence](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [Buffer / Transport](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [Ownership](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

## 13. TBD / Deferred

실제 API/wrapper 배치·transport·callback 충분조건은 [HW TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware) 및 B2-R에서 현 source/generated 근거로 확인한다. 논리 책임을 실제 파일 수로 복제하지 않는다.
