# VSS / DRIVER / AUDIO_TX

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

원래 TX operation과 PCM 버퍼 회차의 장치 접근을 보호하고 확인 범위가 명확한 전송·출력·반환·정리 근거를 AUDIO_STREAM에 제공합니다. 실제 전송과 구성 준비는 서로 다른 책임입니다.

## 상태 owner와 입출력

AUDIO_TX는 operation·Attempt/PCM 회차·유효 구간의 장치 retain/arm·descriptor 진행과 반환 근거의 writer입니다. AUDIO_STREAM usage, HAL raw 등록, PLAYBACK Session 최종과 HEALTH Fault는 직접 쓰지 않습니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | AUDIO_STREAM의 PCM 인계/start/정리 요구, HAL의 사전 귀속 raw 사실·시각/scope/coverage·손실 |
| 출력 | 원operation의 수락/활성화·actual 후보, consumed·safe return·장치 범위 no-start/termination과 부족/손실 |
| 의존 | AUDIO_STREAM → AUDIO_TX → HAL 및 필요한 RUNTIME; CONTROL 조율은 AUDIO_STREAM |

## 인접 관계와 비동기 경계

하위 효력이 생기기 전에 원등록·pending/참조 보호가 필요합니다. 실패 반환에도 부분 효력/미래 접근 가능성이 남으면 보호를 유지합니다. consumed ≠ safe return ≠ 현재 CPU 쓰기 안전 ≠ actual end ≠ 전체 retirement입니다. HALF/MAJOR IRQ·abort 반환·mute/zero-fill만으로 물리 STOP·안전 반환을 확인하지 않습니다.

[HAL callback의 원등록 경계](../../04_HAL_BSP/00_LAYER_OVERVIEW.md) · [AUDIO_STREAM의 PCM usage](../../02_SERVICE/AUDIO_STREAM/00_MODULE_OVERVIEW.md)

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-13 | `AudioTx_Request` — 동기 원등록·PCM 보호·요청 / 효과 후속 | [상세 설명](functions/AudioTx_Request.md) | [한국어 의사코드](pseudocode/AudioTx_Request.md) | D5-1 SELF_CHECKED / 독립 부분 검수 대기 |
| CORE-14 | `AudioTx_Advance` — 비동기 raw의 원귀속 적용·반환/출력 근거 | [상세 설명](functions/AudioTx_Advance.md) | [한국어 의사코드](pseudocode/AudioTx_Advance.md) | SELF_CHECKED / GD2_PENDING |

CORE-13 설명·의사코드와 기존 CORE-14 승인 문서로 진입할 수 있습니다. AUDIO_STREAM이 호출하는 AudioTx_Request는 즉시 요청·반환·진단 3 OUT, AudioTx_Advance는 원래 raw의 후속 5 OUT를 담당합니다. 작성 문서와 아래 동결 Function 원문을 함께 읽으며, CORE-14 행의 작성 당시 표시는 보존합니다. 다른 함수쌍은 각 단계의 별도 지시·검수를 따릅니다.

## 후속 진행 후보와 미정

원 key/회차·late·partial·coverage 부족/손실 판정은 독립 SW/Host 후보입니다. 실제 지속 DMA와 개별 operation 상관, 잔류 FIFO/frame·차기 방문/WCET·메모리 조건은 HW_UNVERIFIED/BINDING_TBD입니다. 과거 Transport는 dated 참고 후보입니다. AudioTx_Advance 승인 본문은 보존하고 AudioTx_Request는 D5-1 SELF_CHECKED / 독립 부분 검수 대기입니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 AUDIO_TX Module의 책임·lifecycle](../../../design/20_MODULES/30_AUDIO_TX_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/60_AUDIO_TX_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
