# VSS / APP

[상위 VSS 지도](../01_VSS_MAP.md) · [처음 읽기](../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 PASS 인수. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

APP는 각 Service의 진행과 선택·실행·보고·복구 순서를 연결합니다. 상태를 변경하는 책임은 해당 owner에 남기고, FLOW는 반영 완료 관측의 일관성을 확인해 POLICY 판단과 PLAYBACK 요구를 조율합니다.

## 포함 Module

| Module | 책임과 상태 원본 |
|---|---|
| [FLOW](FLOW/00_MODULE_OVERVIEW.md) | 조율 단계·재평가 필요성·임시 읽기 묶음. 다른 Module 상태의 두 번째 writer가 아님 |

## 입력·출력과 의존

startup·제품/TEST 입력·시간/후속 처리 기회와 Service 결과를 받아 필요한 공개 의미 경계를 연결합니다. APP → INPUT/STORE/POLICY/PLAYBACK/AUDIO_STREAM/ASSET/HEALTH 및 필요한 RUNTIME이 허용 방향입니다. Driver/HAL/RTD 직접 호출이나 PCM 주소 처리는 APP 책임이 아닙니다.

입력이 없어도 기한·후속 결과·정리·재평가 기회는 진행됩니다. 실제 Task·호출 주기·직렬화·예산은 미정입니다. 함께 판단 가능한 관측의 조율은 독립 SW 후보이며 하드웨어 미정으로 삭제하지 않습니다.

## 연결해서 읽기

[D3 의미 흐름](../01_VSS_MAP.md#d3-semantic-flow)은 [Flow_Process](FLOW/functions/Flow_Process.md#order)의 입력·원 사실 적용 → 최신 STORE 기한 → coherent POLICY 판단·PLAYBACK 의도 → 별도 basis의 HEALTH 보고 연결 순서입니다. FLOW는 owner의 반환·후속을 보호하고 원본 writer를 대신하지 않으며 실제 Task/IRQ 경계를 확정하지 않습니다.

## 지금 읽을 원문과 다음 위치

[R1 APP 책임](../../design/10_LAYERS/10_APP_LAYER.md) · [R4 Ownership](../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md) · [R4 복구 순서](../../design/50_CONTRACTS/40_FAULT_RECOVERY.md)

`Flow_Process`의 기존 설명/의사코드는 D3-1 부분 독립 PASS 인수 완료입니다. [FLOW 함수 표](FLOW/00_MODULE_OVERVIEW.md)에서 정확한 두 경로를 찾을 수 있습니다.
