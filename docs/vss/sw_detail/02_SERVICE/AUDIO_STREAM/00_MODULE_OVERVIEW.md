# VSS / SERVICE / AUDIO_STREAM

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

원래 출력 시도의 MP3 준비·CPU decode·PCM 생산과 장치 근거 결합을 담당합니다. ASSET bytes를 provider로 소비하고 TX/CONTROL 결과를 Backend 범위로 PLAYBACK에 반환합니다.

## 상태 owner와 입출력

AUDIO_STREAM은 provider 진행·소비 위치, PCM A/B의 내용/usage와 미래 decode/refill/제출 권한을 씁니다. TX의 장치 retain과 PLAYBACK의 Session 최종 판단은 별도 원본입니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | PLAYBACK 준비/제어 요구·원래 문맥/기한, ASSET bytes·읽기 결과, TX/CONTROL 귀속 근거 |
| 출력 | 준비·Backend 결과, TX PCM 인계/제어와 CONTROL 구성 요구; 허용된 복구 수행/검증 근거 |
| 의존 | PLAYBACK·FLOW → AUDIO_STREAM → ASSET/AUDIO_TX/AUDIO_CONTROL 및 필요한 RUNTIME |

## 인접 관계와 비동기 경계

PCM 전송 버퍼는 A/B 두 개입니다. prepared는 출력 허가나 actual이 아닙니다. 같은 회차의 안전 반환과 현재 CPU 쓰기 조건을 확인하며, STOP 뒤 반환은 old 정리일 뿐 decode/refill/새 sound handoff 권한을 복원하지 않습니다. compressed MP3는 CPU decode 입력이며 PCM 장치 전송과 다릅니다.

[ASSET 읽기](../ASSET/00_MODULE_OVERVIEW.md) · [AUDIO_TX 장치 보호](../../03_DRIVER/AUDIO_TX/00_MODULE_OVERVIEW.md) · [AUDIO_CONTROL 준비](../../03_DRIVER/AUDIO_CONTROL/00_MODULE_OVERVIEW.md)

## 담당 Core의 예정 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-10 | `AudioStream_Prepare` — 준비 수용·Asset/provider·초기 PCM, 후속 준비 | `02_SERVICE/AUDIO_STREAM/functions/AudioStream_Prepare.md` | `02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_Prepare.md` | PLANNED_NOT_WRITTEN |
| CORE-11 | `AudioStream_RequestControl` — start/stop/허용 복구 수용·미래 생산 차단 | `02_SERVICE/AUDIO_STREAM/functions/AudioStream_RequestControl.md` | `02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_RequestControl.md` | PLANNED_NOT_WRITTEN |
| CORE-12 | `AudioStream_Advance` — 하위 사실·PCM 회차·Backend/복구 결과 진행 | `02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md` | `02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_Advance.md` | PLANNED_NOT_WRITTEN |

경로는 `docs/vss/sw_detail/` 기준입니다. 설명·의사코드 본문은 아직 없으므로 위 경로는 클릭 링크가 아닙니다. 실제 동작은 아래 동결 Function 원문에서 읽고, 새 함수쌍은 각 단계 검수 뒤 작성합니다.

## 후속 진행 후보와 미정

bounded/no-progress 처리·short tail·빈 B 구분·PCM 회차/late·STOP 후 생산 차단은 독립 SW/Host 후보입니다. format/크기·실제 safe-write·DMA/STOP 효과·제품 source/image는 미정입니다. 과거 Provider/Player는 dated 참고 재사용 후보이며 현재 제품 완성본이 아닙니다. 함수쌍은 D4 예정입니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 AUDIO_STREAM Module의 책임·lifecycle](../../../design/20_MODULES/24_AUDIO_STREAM_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/50_AUDIO_STREAM_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
