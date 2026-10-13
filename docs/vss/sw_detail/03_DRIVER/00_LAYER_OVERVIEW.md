# VSS / DRIVER

[상위 VSS 지도](../01_VSS_MAP.md) · [처음 읽기](../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

DRIVER는 AUDIO_STREAM의 중립 요청을 장치 사용과 원래 요청에 귀속된 결과로 연결합니다. PCM 전송과 현재 장치 구성 준비는 다른 책임입니다. 장치 근거가 상위 Session·Fault의 최종 판단을 대신하지 않습니다.

## 포함 Module

| Module | 소유 원본 / 반환 근거 |
|---|---|
| [AUDIO_TX](AUDIO_TX/00_MODULE_OVERVIEW.md) | operation·PCM 회차의 장치 보호/활성화와 소비·반환·출력/정리 근거 |
| [AUDIO_CONTROL](AUDIO_CONTROL/00_MODULE_OVERVIEW.md) | 장치별 제어 문맥·현재 구성 readiness |

## 연결과 물리 경계

AUDIO_STREAM → DRIVER → [HAL/BSP](../04_HAL_BSP/00_LAYER_OVERVIEW.md) 및 필요한 RUNTIME이 허용 방향입니다. TX와 CONTROL의 준비/전송 조율은 AUDIO_STREAM이 맡습니다. DRIVER는 STORE·PLAYBACK·HEALTH 원본을 직접 쓰거나 vendor 세부를 Service에 노출하지 않습니다.

요청 반환과 비동기 사실의 원래 operation·회차·구성 귀속을 보존합니다. PCM A/B의 소비, 안전 반환, 현재 CPU 쓰기 가능성, 출력 종료와 전체 retirement는 서로 다릅니다. HALF/MAJOR IRQ·abort 반환만으로 실제 충분성이 입증되지 않았으며 `HW_UNVERIFIED`입니다. 귀속·late·부족 근거의 논리 검증은 후속 독립 후보입니다.

## 지금 읽을 원문과 다음 위치

[R1 DRIVER](../../design/10_LAYERS/30_DRIVER_LAYER.md) · [R4 비동기 결과](../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md) · [R4 PCM/Transport](../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md)

D2는 `AudioTx_Advance` 시범, D5는 나머지 TX/CONTROL과 HAL callback 연결입니다. 두 Module의 함수 표에서 예정 설명·의사코드 위치를 확인합니다.


## HAL/BSP·RTD 연결 근거 읽기

[원등록·PCM 회차·물리 충분조건의 source-basis 연결](../04_HAL_BSP/01_HAL_BSP_BOUNDARIES.md)에서 기존 Core/R2~R6와 실제 참고 출처를 함께 확인합니다. 현재 제품 source/image·C ABI·DMA/출력 충분성은 미검증이며, 필요한 효과의 Gate와 독립 논리 후보를 구분합니다.
