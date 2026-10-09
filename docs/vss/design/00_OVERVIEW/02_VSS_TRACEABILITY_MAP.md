# VSS Traceability Map

기존 49 Trace를 13개 묶음으로 연결한다. 표의 ID에는 모두 `VSS-TR-` 접두어가 붙으며 `{A,B}`는 각각의 기존 ID를 뜻한다. 아래 기존 ID·위치 연결은 유지한다. R7에서 owner/Function/Data/Contract/Flow/Header 후보까지 내부 설계 의미를 전수 대조했으며 [49개 개별 검수 근거](../../../../R7_RESULT_SUMMARY.md#trace-coverage)를 확인한다. 이는 최신 팀 승인 SR/SysRS 전수 충족이나 Network 연계 PASS가 아니다. 최신 외부 Interface 대조는 EXTERNAL-IF-TBD다. Trace별 새 명세·ID·API를 만들지 않는다.

| 기존 Trace 묶음 | 담당 Module → 주요 처리 |
| --- | --- |
| `BOUND-NETWORK`, `IN-{RECEIVED,VALID,ACCEPTED,SOURCE,TEST,DUPLICATE}` | [INPUT](../20_MODULES/20_INPUT_MODULE.md) → 검증 문맥, [STORE](../20_MODULES/21_STORE_MODULE.md) → 수용/동일 발생 |
| `OS-{AGE,PENDING,STARTED,FINAL}` | [STORE](../20_MODULES/21_STORE_MODULE.md#occurrence) → 원본 age·이력, [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes) → 귀속된 재생 사실 |
| `ST-{WARNING,CLEAR,DISABLED,HOLD,REAR,WINDOW}` | [STORE](../20_MODULES/21_STORE_MODULE.md#stateful) → 경고/품질·Hold·Rear, [INPUT](../20_MODULES/20_INPUT_MODULE.md) → WINDOW 보류/TEST |
| `SEL-{POLICY,SNAPSHOT,DECISION,PREEMPT}` | [POLICY](../20_MODULES/22_POLICY_MODULE.md) → 판단·계획, [FLOW](../20_MODULES/10_FLOW_MODULE.md) → 일관된 읽기·재평가 |
| `PLAY-{IDENTITY,OWNER,CUES,QUARANTINE}` | [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) → identity·전체 정책·owner·불명확 결과 |
| `BE-{PREPARED,ACCEPTED,START,NOSTART,END,LATE}` | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) → 시도 준비/Backend 결과, [TX DRIVER](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence) → 실제 출력 근거, [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes) → 최종 적용 |
| `ASSET-{LOOKUP,BYTES}` | [ASSET](../20_MODULES/25_ASSET_MODULE.md) → metadata·bounded MP3 bytes |
| `AUD-{DECODE,PCMOWNER,REFILL,TRANSPORT,CONTROL}` | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md#pcm) → decode·A/B, [TX DRIVER](../20_MODULES/30_AUDIO_TX_MODULE.md) → 전송, [CODEC/CLOCK](../20_MODULES/31_AUDIO_CONTROL_MODULE.md) → 준비 |
| `DIAG-{INPUT,FAULT,RECOVERY}` | [DIAGNOSTIC STATUS](../20_MODULES/26_HEALTH_MODULE.md) → 진단·기존 Fault·복구 판단 |
| `STATUS-{CAPABILITY,COMPOSE}` | [DIAGNOSTIC STATUS](../20_MODULES/26_HEALTH_MODULE.md#status) → 서비스 수준/보고, [INPUT](../20_MODULES/20_INPUT_MODULE.md) → 기존 Network 연결 |
| `RUN-{INIT,DEADLINE,ISR,RESOURCES}` | [FLOW](../20_MODULES/10_FLOW_MODULE.md) → startup/진행, [RUNTIME](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) → 시간/손실, [HAL/BSP](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) → ISR, [TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory) → 자원 확인 |
| `BIND-LOWER` | [DRIVER Layer](../10_LAYERS/30_DRIVER_LAYER.md) → HAL, [HAL/BSP](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) → vendor·보드/build 경계 |
| `PER-MEASURE` | [RUNTIME 측정](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md#measurement) → 원producer marker 연결 |

Module 내 주요 흐름은 [Core Functions](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md)의 owner별 후보와 연결된다. Trace 하나당 함수/API를 만들지 않는다. 이는 설계 의미의 연결이며 구현·성능 검증 완료 표가 아니다.



[Document Map](01_DOCUMENT_MAP.md) · [R7 End-to-End 검수 결과·미정](../../../../R7_RESULT_SUMMARY.md#trace-coverage)
