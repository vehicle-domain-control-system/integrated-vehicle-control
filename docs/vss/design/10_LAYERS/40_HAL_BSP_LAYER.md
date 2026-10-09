# HAL / BSP Layer

> R1 Layer 설계 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Layer Overview](00_LAYER_OVERVIEW.md)

## 1. 목적

Driver의 중립 장치 요구를 현 vendor/IRQ와 보드/build 근거에 연결하고 관측한 원본 사실이 해당 요청·출력 시도와 어떻게 연결되는지 보존한다.

## 2. 역할

HAL은 vendor 요청/IRQ·raw capture 경계이며 BSP는 보드/generated 구성·이미지/메모리 배치 근거다. 한 Layer에서 읽되 두 책임의 owner와 수명은 합치지 않는다.

## 3. 책임

- 현 vendor handle/config/API·IRQ 등록과 실제 return/부분 효력을 연결한다.
- 요청 전에 귀속 정보를 저장하고 원본 관측 사실·손실을 보존해 Driver의 후속 처리를 연결한다.
- 보드/source/generated·Clock/PinMux/IRQ·linker/map·MPU/cache 근거를 관리한다.

## 4. 책임이 아닌 것

차량 정책, Session/Fault 최종 판단, PCM usage 변경, decode/refill·대량 복사·상위 복구 실행은 맡지 않는다. 과거 bring-up 값은 현 Binding 확정 근거가 아니다.

## 5. 포함 Module / 실제 내부 책임

| 내부 Boundary | Owner / 입력·출력 / 수명 |
| --- | --- |
| AUDIOHAL | vendor handle/config·요청 전에 저장한 귀속 정보/raw 사실을 소유. Driver 요구를 실제 호출/IRQ 관측으로 연결하며 HW·대기·처리 중 참조 기간을 따른다. |
| AUDIOPLATFORM / BSP | 보드/build/generated 배치 근거를 소유. HAL binding과 ASSET build-time 참조에 제공하며 해당 구성/image 유효 기간을 따른다. |

R1 판정은 **Layer 내부 Boundary**다. 두 책임은 독립 장치 동작과 정적 보드 근거를 섞은 집합이며 하나의 Module lifecycle이 아니다. raw 등록의 owner는 필요하지만, 현 설계에서 이 경계 위에 별도 Module state/API를 추가할 필요는 없다. 상세는 Layer 하위에 보존한다.

## 6. 입력 / 출력

| 구분 | 의미 |
| --- | --- |
| 입력 | Driver의 원래 요청/해당 장치 구성 중립 요구, vendor/IRQ raw 사실, 현 보드/source/generated·build 근거 |
| 출력 | 실제 return/부분 효력·요청 전에 저장한 귀속 정보에 연결된 사실·시각/판정에 필요한 관측 범위·기간의 근거(coverage)/손실과 보드/배치 참조 근거 |

## 7. 허용 의존 방향

DRIVER → HAL → 현 vendor/HW 및 BSP/RUNTIME이다. BSP는 현 보드/build 근거를 HAL에 제공하며 ASSET에는 build-time 이미지 배치 참조를 제공한다. 후속 결과 전달은 기존 등록 경계다.

## 8. 금지 의존 방향

HAL/BSP → APP/SERVICE 정책 호출, 상위 authoritative 상태 직접 변경, 현재 Session을 callback 귀속으로 나중에 붙이는 것을 금지한다. ASSET의 build-time 참조를 새 runtime Flash 호출로 바꾸지 않는다.

## 9. Layer 수준 Contract

HAL은 요청 전에 저장한 귀속 정보와 실제 관측을 보존하고 의미 판정은 Driver/Service owner에 둔다. BSP 구성 근거와 진행 중 작업의 binding 문맥을 혼합하지 않는다. ISR·등록/자원 수명의 세부 조건은 하위 Boundary 상세에서 읽는다.

## 10. 대표 처리 흐름

원래 요청 등록 → 현 구성에 연결된 vendor 요청 → IRQ/raw 사실의 짧은 capture·acknowledge/알림 → 등록된 요청을 처리하는 Driver의 후속 처리다. BSP는 이 연결의 현 보드/build 근거를 제공한다.

## 11. Module / 하위 상세 링크

독립 Module을 추가하지 않았다. [HAL/BSP 요청·ISR·자원 수명](details/40_HAL_BSP_BOUNDARY.md) · [DRIVER / HAL 기존 후보](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#driver-hal) · [TX/HAL 기존 데이터](../40_DATA/60_AUDIO_TX_DATA.md#tx)

## 12. 관련 공통 Contract

[Ownership / Lifetime](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [Async / Evidence](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [Buffer](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

## 13. TBD / Deferred

actual RTD/IRQ/timestamp·Clock/PinMux·물리 값은 [HW TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware), linker/MPU/cache/Flash budget은 [메모리 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory)다. wrapper별 새 Module/API·DMA/TCD Binding을 확정하지 않는다. source/config 대조는 R7 뒤 B2-R이다.
