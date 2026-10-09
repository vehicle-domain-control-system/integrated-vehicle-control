# AUDIO STREAM Module — 준비·decode·PCM

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

원래 출력 시도의 MP3 준비·PCM 생산과 장치 근거 결합을 관리해 PLAYBACK에 Backend 범위 결과를 제공한다.

## 2. 역할

AUDIO STREAM(C09)는 ASSET의 compressed MP3 bytes를 내부 CPU decoder/provider로 decode해 PCM A/B를 만든다. 장치 준비와 PCM 전송 요청을 조율한다.

## 3. 책임

- 해당 출력 시도의 준비·provider 진행·PCM A/B usage와 후속 decode/refill/제출 권한을 관리한다.
- 해당 버퍼 사용 회차의 안전 반환 뒤 bounded PCM 생산과 인계를 진행한다.
- 장치 근거에 해당 범위의 미래 PCM 차단을 결합하고 허용된 Backend 복구 수행/검증 결과를 반환한다.

## 4. 책임이 아닌 것

ASSET의 읽기 근거, DRIVER의 버퍼 사용권 보호·출력 근거, PLAYBACK의 Session 최종 판단, HEALTH의 복구 허용/제한 해제는 소유하지 않는다. DRIVER의 버퍼 보호는 장치가 버퍼에 접근할 수 있는 동안의 사용권 보호(retain)를 뜻한다. 별도 decoder Module이나 compressed MP3의 DMA 전송 경로를 추가하지 않는다.

## 5. 소유 상태 / 데이터

원래 출력 시도의 준비·실제 입력 소비 위치·decoder/provider 진행·PCM 내용/usage와 미래 decode/refill/제출 권한의 단일 writer는 AUDIO STREAM(C09)이다. AUDIO STREAM(C09)의 PCM 사용 상태와 TX의 장치 보호는 서로 다른 원본이다.

## 6. 입력

PLAYBACK의 전체 계획·Session/Attempt·원래 기한·start/stop 요구, ASSET bytes·읽기 근거, TX의 원래 출력 시도·버퍼 사용 회차 장치 사실과 AUDIO CONTROL의 해당 장치 구성 준비/제어 결과, FLOW의 진행/허용된 대상 복구 요구를 받는다.

## 7. 출력

TX에 유효 PCM·해당 버퍼 사용 회차의 인계/start/정리 요구, AUDIO CONTROL에 현재 구성 준비/제어 요구를 전달한다. PLAYBACK에는 준비 완료(prepared), 요청 수락과 실제 전송/출력을 위한 장치 활성화(arm), 실제 출력 관측 및 Backend 범위 결과를, FLOW에는 발생 주체에 연결된 진단 근거·실제 복구 수행/검증 결과를 반환한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

[AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)는 의미 준비 경계로 유지한다. 기존 `AudioStream_Process`는 제어 수용/미래 생산 차단과 하위 사실·PCM/복구 진행으로 분리했다. `AudioStream_FillBuffer`는 Prepare/Advance에서 공유하는 내부 PCM 생산이며 외부 Core가 아니다.

## 9. 의존 Module

PLAYBACK·FLOW → AUDIO STREAM → ASSET / AUDIO TX / AUDIO CONTROL / 필요한 RUNTIME이다. Driver의 해당 요청·출력 시도에 귀속된 결과를 Backend 범위로 결합하되 PLAYBACK·HEALTH·STORE 원본을 직접 수정하지 않는다.

## 10. 상태 전이 또는 주요 lifecycle

| 흐름 | 처리 |
| --- | --- |
| prepare | 원래 출력 시도와의 불변 연결 등록 → Asset/provider → 초기 A/B·유효 길이/format/EOF → 현재 장치/전송 준비를 종합해 prepared 반환. 출력 권한은 아직 없음 |
| start | 최신 준비/차단/구성·원래 기한 확인 → TX start 요청 → 수락+장치 활성화와 구별해 실제 출력 사실 전달 |
| refill | 해당 버퍼 사용 회차 safe return 검증 후 해당 Buffer만 사용권 회수 → bounded decode → 권한 재검사 → 새 회차 인계 |
| 정상 구간 끝 | 유효 tail + 장치 drain + 해당 구간 미래 PCM 불가를 조합. 후속 cue/전체 정책 판단은 PLAYBACK |
| stop/abort | 미래 decode/refill/제출 차단 → old TX 작업 정리 → 실제 근거 추적. 진행 중 provider/read는 안전 종료까지 유지 |

짧은 Asset은 B가 비거나 tail이 짧을 수 있다. 실제 유효 음향만 인계하고 길이/반복을 늘리지 않는다. 물리 무음 zero-fill의 구분·안전 조건은 [Buffer C1-01](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence)을 따른다. 준비 중 취소/구성 무효화는 늦은 prepared의 start 권한이 아니다. actual이 acceptance보다 먼저 와도 해당 요청·출력 시도에 귀속된 결과를 유지한다.

provider/read는 실제 작업·참조가 안전 종료될 때까지 유지한다. Backend 결과의 귀속도 consumer 반영 또는 보호된 추적 완료까지 보존한다.

## 11. Module-local Contract

<a id="pcm"></a>
### PCM A/B usage의 로컬 계약

A/B 수·pending 보호·소비/안전 반환·현재 쓰기 안전성은 [Buffer §1~5](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#access-cycle)를 따른다. AUDIO STREAM(C09)은 자기 PCM usage만 반영하고 TX의 device 보호 원본을 직접 변경하지 않는다.

B가 보호 중이면 A refill 동안 B를 쓰지 않는다. decode 중 stop이 생기면 handoff 직전에도 제출 권한을 재검사한다. STOP 뒤 반환은 옛 usage 정리이며 provider의 decode/refill/재제출 권한을 복원하지 않는다. 같은 key의 충분 반환/명확한 무접근 거부만 적용하고 old A로 새 A/B를 해제하지 않는다.

Device no-start/end에는 AUDIO STREAM(C09)의 미래 PCM 차단을 결합한다. 전체 상위 cue/반복 차단과 final/retirement는 [PLAYBACK](23_PLAYBACK_MODULE.md#outcomes)이 판단한다. 허용된 Backend 복구 수행과 효과 검증은 Process 흐름 안에서 구분하고 해당 대상에 연결된 결과를 FLOW에 반환한다.

## 12. 대표 오류 / 예외 처리 원칙

- Asset/decode 실패: 오류를 처음 발견한 주체·단계·Asset/시도를 보존해 해당 재생 실패/정리로 전달한다. 새 Fault 분류나 대체 음원을 만들지 않는다.
- start/handoff 오류 또는 장치가 일부만 활성화된 경우: 미래 접근 불가 확인 전 보호를 유지하고 정상 rollback을 추정하지 않는다.
- old A/late 결과: 새 usage/provider를 변경하지 않는다. 원래 출력 시도의 정리/진단 또는 최종 전 적법 사실 적용만 허용한다.

## 13. 관련 Function 문서

[AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Provider](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) · [PCM usage](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [TX](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [Readiness](../40_DATA/60_AUDIO_TX_DATA.md#control)

## 15. 관련 공통 Contract

[Buffer / Transport](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md) · [Async](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md) · [Ownership](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md)

공통 규칙은 위 R4 계약을 따르며 이 문서의 owner·상태 적용·실패/예외는 로컬 조건으로 유지한다.

## 16. TBD / Deferred

provider/PCM 자원 반환과 old callback 기록 폐기는 [HAL/BSP lifetime](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime)을 따른다. decode/format은 [Asset TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#asset), 크기/배치는 [메모리 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory)를 확인한다.

실제 PCM size/format/transport와 DMA/TCD 방식은 이번에 확정하지 않는다. Function/타입 상세화와 실제 Binding은 후속 단계다.
