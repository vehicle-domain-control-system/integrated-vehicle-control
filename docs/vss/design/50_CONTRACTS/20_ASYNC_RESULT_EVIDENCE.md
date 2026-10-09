# Async Result / Evidence — 수용과 실제 출력 근거

> R4 · 2026-10-08 · 논리 충분조건 정리 · 물리 관측의 충분성은 B2-R/보드 검증 TBD

[Contract Overview](00_CONTRACT_OVERVIEW.md#async)

원래 요구의 단계와 실제 장치 사실, Backend 범위 결과, 전체 Session 판정을 구별한다. 아래는 기존 자료·owner의 공통 불변조건이며 새로운 Result union이나 결과별 Core를 정의하지 않는다.

<a id="stages"></a>
## 1. 준비·제출·수용·실제 시작은 서로 다른 사실이다

| 단계 | 확인하는 사실 | 이 단계만으로 확인할 수 없는 것 |
| --- | --- | --- |
| POLICY winner | 현재 평가에서 선택한 후보/전체 계획 | PLAYBACK 채택·출력 권한 |
| 준비 요구 수용 / pending | 원래 준비 문맥 등록 또는 후속 준비 진행 | prepared·실제 start |
| prepared | 현재 Asset/provider·장치 구성·필요 초기 PCM 준비 충족 | 새 start 허가·actual |
| START submitted / queued | 하위 요구 제출·대기 접수 | 실제 장치 활성화 |
| START accepted and enabled | 원래 요구 수락과 해당 출력이 가능하도록 실제 활성화한 근거 | 실제 음향 출력 시작 |
| 실제 출력 관측 | 원래 요구/활성화 이후 새로 관측된 해당 시도의 출력 경계 사실 | 첫 시작의 상위 적법성·전체 완료 |

**winner ≠ prepared ≠ accepted/enabled ≠ actual sound start**다. PCM 인계 수용 자체는 START 활성화가 아니며 장치 ready도 새 Asset/provider/A/B 확인을 대신하지 않는다. 지속 스트림에서 개별 음향 활성화를 어떻게 입증하는지는 [Buffer §6](50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream)의 B2-R TBD다.

즉시 callback이나 결과 전달 순서 역전이 가능해도 원래 사실의 인과 관계를 보존한다. actual이 acceptance보다 먼저 반영되거나 종료 뒤 늦은 acceptance/prepared가 와도 이미 반영한 actual·종료 상태를 되돌리지 않는다.

<a id="evidence-basis"></a>
## 2. 판정에는 원래 귀속·범위·시각·관측 근거가 필요하다

HAL은 요청 전에 보호한 문맥과 raw 사실·발생/포착 시각·관측 범위/손실을 짧게 보존한다. DRIVER는 원래 operation 또는 device/config에 사실을 적용하고, AUDIO STREAM은 Backend 범위로 결합하며, PLAYBACK만 전체 Session 판정을 한다. ISR은 decode/refill·대량 복사·동적 할당·상위 정책/복구를 실행하지 않는다.

관측 근거 범위(coverage)는 실제 boundary·기간·당시 binding/source/build 및 충분/부족/손실을 포함한다. 원래 요청/활성화 이후의 **새 관측**과 기존 장치 상태를 구별한다. 알림·현재 flag·처리 현재 시각은 새 실제 출력 사실이 아니다. 정확한 시각이 없으면 가능한 발생 구간과 불확실성을 남긴다. [Timing §1~3](30_TIMING_FRESHNESS.md#time-context)을 함께 적용한다.

`VSS_COVERAGE_INSUFFICIENT`는 필요한 관측/물리 상관 근거 부족, `VSS_COVERAGE_LOST`는 귀속·시간·저장 누락/overflow 등의 손실이다. 둘을 정상 완료로 숨기지 않는다. 충분성이 필요한 판정에 영향을 주면 confirmed 근거를 만들지 않으며 확인 범위와 진단을 보존한다. 보존/전달 수단과 용량은 TBD다.

<a id="confirmed-outcomes"></a>
## 3. 출력 결과별 충분조건과 책임 범위

아래 열은 서로를 대체하지 않는다. 하위 사실의 identity·scope·시각·관측 충분성부터 검증하고 상위 조건을 추가한다.

| 결과 | AUDIO TX가 제공할 실제 장치 근거 | AUDIO STREAM의 Backend 결합 | PLAYBACK의 전체 적용 |
| --- | --- | --- | --- |
| `OUTPUT_START_CONFIRMED` | 원래 요청 및 실제 활성화 이후 새 출력 경계 관측, 원래 사실 시각, 충분한 coverage. 지속 raw와 개별 작업의 대응 검증 필요 | 원래 Session/Attempt와 해당 출력 scope의 새 물리 근거를 보호해 전달. actual만인 경우 미래 PCM 차단은 필수 아님 | 첫 적법한 actual만 원래 발생에 Started 통지. 사실 시각·기한/권한을 대조. 기한/권한 밖 실제 출력은 위반 사실로 보존·정리 |
| `NO_START_CONFIRMED` | 필요한 **과거 실제 무출력**의 충분한 관측 범위/기간 **및** 해당 옛 request·active work·descriptor·FIFO/frame의 **미래 출력 불가** | 위 두 독립 근거와 해당 scope의 미래 decode/refill/PCM 제출 차단을 결합 | 전체 해당 Backend 범위와 모든 상위 start/cue/반복 권한 차단·late 귀속 보호 확인 뒤 retirement. uncertain 최종이 없는 원래 발생만 미시작 재평가 가능 |
| `OUTPUT_TERMINATION_CONFIRMED` | 해당 scope의 실제 장치/잔류 FIFO/frame 출력 종료 **및** 옛 미래 출력 불가 | 해당 출력 종료/미래 차단과 그 scope의 미래 PCM 차단을 결합 | 전체 관련 출력 범위·상위 후속 권한 차단·late 보호 확인 뒤 retire. Completed/Interrupted는 전체 계획 끝/중단과 Started 여부에 맞춰 판정 |

현재 장치 작업이 멈춘 상태(quiescent)는 과거 실제 무출력의 증명이 아니다. termination도 과거 시작 여부를 소급 확정하지 않는다. 충분한 미래 차단만 있고 과거 관측이 부족하면 `NO_START_CONFIRMED`로 만들지 않는다.

`VSS_SCOPE_SEGMENT`와 `VSS_SCOPE_ATTEMPT`의 확인 범위를 유지한다. 한 cue/EOS·마지막 block·압축 EOF는 전체 Session Completed가 아니다. 정상 다음 cue는 이전 관련 Backend 구간의 안전 끝과 현재 계획을 확인해 같은 Session에서 진행한다. 전체 종료와 PCM 반환·자료 폐기는 [Buffer §5](50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries), [Ownership §4](10_OWNERSHIP_LIFETIME.md#references-and-release)로 구별한다.

<a id="partial-and-uncertain"></a>
## 4. 부분 효력·근거 부족과 불명확 최종을 구별한다

요청 실패 반환에도 장치 사용권 보호(retain)·일부 활성화·미래 접근/출력 가능성이 남을 수 있다. 그때는 일반 신규 거부로 rollback하지 않으며 원래 보호·차단·정리·결과 추적을 유지한다. 명확한 무접근 거부는 실제 무접근/종료 충분조건이 있는 해당 요구에만 적용한다.

정상 `START_IN_FLIGHT` 대기, 부분 효력/근거 부족, `START_OUTCOME_UNCERTAIN`의 상위 불명확 최종은 같지 않다. PLAYBACK이 기존 계약에 따라 시작 결과 불명확 최종을 기록하면 replay 금지·전체 후속 권한 차단·`QUARANTINED`와 기존 **PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE** 연결을 유지한다. 최종 기록은 물리 정리/retirement보다 먼저 가능하며 old 정리는 계속한다. 이 계약으로 새로운 timeout/retry 값이나 최종화 정책을 만들지 않는다.

timeout, 현재 idle, WSF 미관측, 단일 DMA complete/HALF/MAJOR, abort/error return, mute, 순간 무음, zero-fill 완료를 actual/no-start/전체 종료의 충분조건으로 승격하지 않는다. 각 근거의 실제 물리 경계·상관·관측 coverage는 B2-R에서 입증해야 한다.

<a id="late-results"></a>
## 5. 늦은 결과의 적용은 최종 전과 뒤가 다르다

| 늦게 도착한 사실 | 허용되는 적용 | 유지할 차단 / 금지 |
| --- | --- | --- |
| 최종 확정 전, 같은 원래 Attempt의 적법한 actual | 원래 사실 시각·기한/권한을 확인해 최초 Started/보고 사실 반영 | STOPPING/ABORTING 유지. ACTIVE·후속 cue 재개 없음 |
| uncertain 최종 뒤 또는 retired 문맥 | 원래 시도의 정리·진단과 필요한 참조 종료 | 정상 Pending/Started/Completed·새 Session/Buffer/ready/Fault를 부활시키지 않음 |
| 옛 PCM 회차의 소비/반환 | 정확한 원래 operation/Attempt/A/B/회차에만 적용 | 새 A 회차 또는 B 사용권을 해제하지 않음 |
| 옛 device/config 또는 복구 결과 | 원래 대상의 적용/격리·진단 | 새 readiness·새 Fault instance/구성 해제 없음 |

귀속을 확인할 수 없는 결과는 현재 Attempt로 붙이지 않고 귀속 부족/손실을 유지한다. 원래 불변 귀속·당시 binding의 보호 및 기록 회수는 [Ownership §4~5](10_OWNERSHIP_LIFETIME.md#late-attribution)를 따른다. 미반영 시작/전체 최종·진단은 consumer 반영 또는 보호된 추적 종료까지 보존한다.

## 6. 연결 원본과 미정

[PLAYBACK 로컬 적용](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes) · [TX 로컬 적용](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence) · [Backend Evidence](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence) · [TX Evidence/coverage](../40_DATA/60_AUDIO_TX_DATA.md#vsstxoutputevidence) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

실제 SAI/codec/스피커 출력 경계, HALF/MAJOR·TCD/ISR, 지속 raw와 개별 operation의 대응, abort·drain·no-start·termination 충분조건, 자료 보존/손실 범위는 B2-R/보드 검증 TBD다. 정상 브링업 재생 사실은 이 예외 조건의 입증이 아니다. R6의 C 입력/반환·Header/ABI도 확정하지 않는다.
