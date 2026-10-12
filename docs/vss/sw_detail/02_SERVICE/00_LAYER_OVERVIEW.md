# VSS / SERVICE

[상위 VSS 지도](../01_VSS_MAP.md) · [처음 읽기](../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 PASS 인수. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

SERVICE는 차량 의미·상태·전체 재생 정책과 MP3/PCM 준비를 장치 구현에서 분리합니다. 각 Module이 자기 원본을 쓰고, FLOW가 공개 경계의 반환·관측을 연결합니다. vendor handle·RTD 설정과 물리 통신 framing은 각 하위/외부 책임입니다.

## 포함 Module과 writer

| Module | 소유 원본 / 핵심 역할 |
|---|---|
| [INPUT](INPUT/00_MODULE_OVERVIEW.md) | 출처·세대·순서·검증 문맥; 검증 입력과 보고의 기존 통신 연결 |
| [STORE](STORE/00_MODULE_OVERVIEW.md) | One-shot occurrence 이력과 Stateful 품질/Hold·Rear 후보 |
| [POLICY](POLICY/00_MODULE_OVERVIEW.md) | 읽기 전용 정책과 한 평가의 선택 판단 |
| [PLAYBACK](PLAYBACK/00_MODULE_OVERVIEW.md) | 단일 Session/Attempt·종료 의도·후속 권한 |
| [AUDIO_STREAM](AUDIO_STREAM/00_MODULE_OVERVIEW.md) | provider 진행·PCM A/B 내용/usage·미래 생산 권한 |
| [ASSET](ASSET/00_MODULE_OVERVIEW.md) | read-only 음원 metadata·범위 읽기·consumer 참조 관계 |
| [HEALTH](HEALTH/00_MODULE_OVERVIEW.md) | 현재/최근 Fault·복구 허용/해제; 원본 읽기 전용 파생 보고 |

## 관계와 비동기 경계

INPUT → STORE, PLAYBACK → AUDIO_STREAM/STORE, AUDIO_STREAM → ASSET/AUDIO_TX/AUDIO_CONTROL이 주요 호출 방향입니다. POLICY·HEALTH의 관측은 FLOW가 연결하고 두 Module은 실행·Network 송신을 직접 호출하지 않습니다. ASSET의 BSP 참조는 build-time 근거이며 runtime Flash 쓰기 경로가 아닙니다.

수용·선택·준비·요청 수락과 실제 출력은 다른 경계입니다. STORE 이력, PLAYBACK Session, STREAM의 PCM usage와 TX 장치 보호도 별도 원본입니다. 정의된 중복·품질·bounded read·late/Unknown·보고 판단은 후속 SW/Host 후보로 유지하며, 외부 ABI·물리 출력의 증거는 미정입니다.

## 연결해서 읽기

[D3 의미 흐름](../01_VSS_MAP.md#d3-semantic-flow)에서 INPUT은 검증 문맥, STORE는 발생·품질·기한, POLICY는 읽기 판단, PLAYBACK은 Session/Attempt·원 actual·통지 의무를 각각 씁니다. [원 fact 적용](STORE/functions/Store_ApplyPlaybackFact.md)·최신 PB 관측은 만료보다 먼저이고, D4 STREAM·D6 HEALTH는 [동결 R5/R6 경계](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#core-map)만 연결하며 해당 함수 상세는 아직 미작성입니다.

## 지금 읽을 원문과 다음 위치

[R1 SERVICE](../../design/10_LAYERS/20_SERVICE_LAYER.md) · [R3 자료와 writer](../../design/40_DATA/00_DATA_OVERVIEW.md) · [R4 공통 계약](../../design/50_CONTRACTS/00_CONTRACT_OVERVIEW.md)

D2 STORE 시범과 D3-1/2 의미 경로의 함수쌍은 승인 인수 완료이며, D4 ASSET/STREAM·D6 HEALTH 상세는 아직 미작성입니다. 각 Module의 표에 정확한 예정 경로와 미작성 상태가 있습니다. WINDOW 제품 보류와 TEST 격리는 INPUT 페이지에서 확인합니다.
