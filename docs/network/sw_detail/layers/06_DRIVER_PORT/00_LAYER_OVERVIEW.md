# Driver / 내부 Port 경계

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Function](../../03_FUNCTION_INDEX.md)

기존 [Driver 책임](../../../network_sw_architecture.md#4-layer별-책임)은 HW/SDK callback, TX/RX primitive, local HW error를 상위에 제공하는 것이다. parsing·기능 정책을 Driver에 이동하지 않는다. LinkPort는 Driver/HAL 교체를 위한 최소 내부 의미 경계 후보이며 U1에서 함수명·인자·구조체를 확정하지 않는다.

| 현재 자료 | 지위 |
|---|---|
| [실제 callback·자원 미정](../../../network_sw_architecture.md#11-현재-확정하지-않는-항목) | SDK/RTD/MB-FIFO/IRQ/DMA·Owner 문맥의 타깃 증거 필요 |
| [팀 SDK·Port·핀·전압 OPEN](../../../interface/06_OPEN_FOR_TEAM.md) | `OPEN-SDK / OPEN-HW / OPEN-TIME` 유지 |
| [Transport](../05_TRANSPORT/00_LAYER_OVERVIEW.md) → [HAL/BSP](../07_HAL_BSP/00_LAYER_OVERVIEW.md) | 상위 의미와 하위 vendor binding 구별 |

현재는 `SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT / TARGET_BINDING_OPEN`이다. 실제 Driver Module·가짜 SDK 함수·핀/IRQ 수치를 생성하지 않는다. 초기 최소 Port 의미 검토는 U2/U4D, 실제 구현 결합은 U10의 별도 승인·B3/SDK 근거 대상이다.

별도 B2의 144↔344 UART 실험은 제품 Pi↔CIS UART-C 또는 ESP32↔DOMAIN UART-M의 완성 증거가 아니다. U2의 [최소 CAN LinkPort 의미 후보](10_CAN_LINKPORT_SEMANTICS.md)를 추가했다. 실제 Driver 상세·SDK 결합은 계속 `PLANNED_ONLY / TARGET_BINDING_OPEN`이며 G2 독립 판정은 PENDING이다.
