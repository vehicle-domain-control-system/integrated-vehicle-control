# VSS / HAL/BSP

[상위 VSS 지도](../01_VSS_MAP.md) · [DRIVER](../03_DRIVER/00_LAYER_OVERVIEW.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

HAL은 vendor 요청·IRQ의 raw capture 경계이고 BSP는 보드/generated·이미지/메모리 배치 근거입니다. 같은 Layer에서 읽지만 raw 등록과 정적 구성의 owner·수명을 합치지 않습니다. **독립 Module을 추가하지 않습니다.**

## 내부 Boundary

| Boundary | 소유와 연결 |
|---|---|
| AUDIOHAL / AUDIOHAL_BOUNDARY | 요청 전에 저장한 불변 귀속 정보, vendor 문맥과 원 관측·시각/coverage·손실. DRIVER에 후속 처리 근거 전달 |
| AUDIOPLATFORM / BSP | 현 보드·source/generated·Clock/PinMux/IRQ·linker/MPU/cache의 build 근거. HAL binding과 ASSET build-time 참조에 제공 |

DRIVER → HAL → 현 vendor/HW 및 BSP/RUNTIME이 허용 방향입니다. ISR은 원등록에 연결된 사실을 짧게 보존하며 decode/refill·정책 판단·상위 상태 쓰기를 수행하지 않습니다. 현재 Attempt를 나중에 조회해 옛 callback에 붙이지 않습니다.

## CORE-16의 문서쌍

`AudioHAL_Callback`은 R2 Core이지만 10 Module 밖 **HAL/BSP 내부 AUDIOHAL_BOUNDARY**가 논리 owner입니다. D5-2에서 아래 두 함수 문서만 작성하며 별도 Boundary 개요나 Module을 추가하지 않습니다.

| Core | 함수 | 상세 설명 | 한국어 의사코드 | 작성 상태 |
|---|---|---|---|---|
| CORE-16 | `AudioHAL_Callback` | [상세 설명](AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md) | [한국어 의사코드](AUDIOHAL_BOUNDARY/pseudocode/AudioHAL_Callback.md) | D5-2 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING |

위 링크는 HAL/BSP 내부 Boundary의 CORE-16 문서쌍입니다. callback 기록의 수명은 Session retirement·PCM 반환과 같다고 가정하지 않습니다. 원귀속·late/손실의 독립 논리 후보는 유지하되 실제 callback ABI·지속 DMA와 개별 회차의 상관·출력/STOP 충분성은 `BINDING_TBD / HW_UNVERIFIED`입니다. 과거 bring-up 설치 PASS가 현 제품/보드 PASS를 뜻하지 않습니다.

## 현재 상세 근거

[R1 HAL/BSP](../../design/10_LAYERS/40_HAL_BSP_LAYER.md) · [등록·ISR·자원 수명](../../design/10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) · [R2 DRIVER/HAL 함수](../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md) · [R4 PCM 회차·지속 스트림](../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md)

실제 보드/build 대응과 raw 사실의 물리 충분조건은 source/generated 및 보드 계측으로 확인해야 합니다. 신규 Task·Queue·wrapper·HAL Module을 선결정하지 않습니다.


## HAL/BSP·RTD 연결 근거 읽기

[원등록·PCM 회차·물리 충분조건의 source-basis 연결](01_HAL_BSP_BOUNDARIES.md)에서 기존 Core/R2~R6와 실제 참고 출처를 함께 확인합니다. 현재 제품 source/image·C ABI·DMA/출력 충분성은 미검증이며, 필요한 효과의 Gate와 독립 논리 후보를 구분합니다.

Startup/Recovery의 장치별 readiness와 실제 수행·별도 검증은 [CONTROL·원 대상의 연결](01_HAL_BSP_BOUNDARIES.md#health-connection)을 거쳐 [HEALTH의 전체 읽기 흐름](../02_SERVICE/HEALTH/00_MODULE_OVERVIEW.md#startup-fault-recovery-status)으로 이어집니다. HAL/BSP는 원 관측과 정적 구성 근거를 제공하며 Fault·복구 허용·제한 해제의 writer가 아닙니다. [INFRA](../05_INFRA/00_LAYER_OVERVIEW.md#startup-recovery-time)의 시간·후속 알림도 의미 판단을 대신하지 않습니다.
