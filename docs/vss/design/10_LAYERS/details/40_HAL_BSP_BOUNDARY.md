# HAL / BSP — Layer 하위 Boundary 상세

> R1 분류: HAL/BSP Layer 내부 Boundary. 요청/callback·자원 수명·미정 근거를 보존하며 실제 RTD Binding 확정은 아니다.

## 1. 요청·callback 흐름

vendor 요청 **전** 원래 출력 시도/operation·환경·원래 기한, PCM이면 Buffer/회차/유효 구간, control이면 장치/구성의 불변 연결을 확보한다. callback 인자에 Session이 없어도 요청 전에 저장한 귀속 정보와 연결할 근거가 필요하다. 도착한 callback에 현재 Attempt/Buffer 포인터를 나중에 붙이는 방식은 충분하지 않다.

ISR은 필요한 raw 사실·해당 요청·출력 시도와의 연결·발생/관측 시각·판정에 필요한 관측 범위·기간의 근거(coverage)를 짧게 보존하고 acknowledge/후속 알림을 연결한다. decode/refill·대량 복사·동적 할당·상위 정책/복구 실행은 후속 Service에서 한다. 정확한 IRQ 순서·timestamp 방식은 actual binding TBD다.

핵심 후보는 [AudioHAL_Callback](../../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#driver-hal)이다. Driver가 HAL/RUNTIME을 사용하고 HAL은 BSP/RUNTIME을 참조한다. ASSET의 BSP 참조는 runtime Flash 호출이 아니다.

<a id="lifetime"></a>
## 2. 등록과 자원의 lifetime

자원별 독립 회수·새 사용/옛 기록 공존·보호 공간 부족은 [Ownership §4](../../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release)를 따른다. HAL Boundary는 자기 불변 귀속 기록·당시 binding/build를 HW·대기·처리 중 참조/late 대응 종료까지 보호한다.

개별 software TX 귀속과 장기 vendor 등록의 차이는 [Buffer C1-02](../../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream)다. PCM 안전 반환/IRQ disable/Session retirement가 HAL 기록 폐기를 대신하지 않으며 새 context와 alias하지 않는다.

## 3. 대표 예외와 TBD

- vendor error/부분 효력: return과 실제 장치 효력을 구분해 Driver에 반환한다.
- late/귀속 부족: 현재 작업으로 재라벨링하지 않고 당시 저장한 기록 또는 관측 손실을 유지한다.
- 보드/build 자료 불일치·미확인: 확인 범위를 제한하고 과거 bring-up 값을 현 확정값으로 쓰지 않는다.

실제 C/RTD/IRQ/물리 값은 [HW TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware), linker/MPU/cache·Flash budget은 [메모리 TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md#memory)를 확인한다. 이 문서는 Layer 내부 binding 책임을 설명한다. 실제 binding 명세는 후속 B2-R 범위다.

## 문서 연결과 후속 범위

[HAL/BSP Layer](../40_HAL_BSP_LAYER.md) · [공통 계약 위치](../../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [C Mapping](../../90_BINDING/90_C_FILE_API_MAPPING.md) · [TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md)

B2-R에서 실제 source/generated config를 대조한다. R1에서는 분류·탐색만 정리했다. API·IRQ·DMA/TCD·물리 값을 추가 확정하지 않았다.
