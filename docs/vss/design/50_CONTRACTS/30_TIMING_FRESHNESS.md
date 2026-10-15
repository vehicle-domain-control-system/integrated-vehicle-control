# Timing / Freshness — 원 발생, 실제 사실 시각, 현재 판단

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

> R4 · 2026-10-08 · 기존 시간 의미의 공통 Contract · 수치/물리 시간 입증은 기존 TBD 유지

[Contract Overview](00_CONTRACT_OVERVIEW.md#timing)

원 발생의 age, 장치 사실의 발생 시각, 소프트웨어 처리 현재 시각과 읽기 관측의 revision은 다른 근거다. 이 계약은 기존 Meta·Evidence·ReadBasis를 연결하며 A1은 INPUT Meta에 원 시각 구간 상한과 useLimit을 보완하고 기존 owner가 같은 원 기한을 소비하도록 연결한다.

<a id="time-context"></a>
## 1. 실제 발생과 포착·처리 시간을 구별한다

| 시간 문맥 | 기존 의미 | 보존할 조건 |
| --- | --- | --- |
| 입력 원 발생 시각/age | sender·relay·수신 전 지연까지 포함한 원본 의미 | 재수신·대기·준비·재시도로 새 now를 원 발생에 대입하지 않음 |
| 실제 장치 사실 시각 | 원래 작업의 실제 출력/소비/종료 경계가 발생한 시간 | callback 처리 시각과 구별. 정확한 시각 대신 구간이면 불확실성을 함께 보존 |
| HAL 포착 시각 | raw 사실을 포착한 시간 | 포착 자체를 actual sound start로 승격하지 않음 |
| 처리 현재 시각 | Runtime이 평가/진행에 제공하는 현재 시간 | 과거 사실의 발생 시각을 덮거나 옛 허용을 갱신하지 않음 |
| 시간 문맥/연속성 | epoch·reset·wrap·변환·source 시간 신뢰 조건 | 다른 문맥을 무조건 빼서 age/기한을 만들지 않음. 불명확하면 그 범위의 판정 근거 부족 유지 |

PRODUCT와 TEST의 의미 경로는 유지하되 TEST 현재 시간으로 PRODUCT 원 발생 의미를 대체하지 않는다. `Runtime_Now`는 시간을 제공하는 INFRA Boundary이고 도메인 freshness·복구·재생 적법성을 판단하지 않는다. callback의 현재 상태/알림만으로 새 관측을 만들지 않는 규칙은 [Async §2](20_ASYNC_RESULT_EVIDENCE.md#evidence-basis)를 따른다.

<a id="source-age"></a>
## 2. 원 발생 age와 재평가의 시간을 분리한다

INPUT은 해석한 원본 Meta·품질·시간 신뢰 근거를 전달하고 STORE는 원본 identity에 연결된 age·현재 품질·후보 유효성을 관리한다. FLOW/POLICY는 해당 owner의 적용 완료 관측으로 현재 평가를 한다. 새 Session/Attempt, 선점·장치 준비·복구는 원 발생을 새로 만들지 않는다.

원본 age가 불명확하거나 시간 연속성이 깨진 경우 확인할 수 없는 fresh 조건을 성공으로 만들지 않는다. 정확한 첫 출력 시각이 아닌 가능한 발생 구간만 있으면 **그 구간 전체가 원래 기한 안이라는 근거**가 필요하다. 일부 시점만 기한 안이거나 처리 now만 아는 상태는 적법한 최초 actual의 충분조건이 아니다.

<a id="first-start"></a>
## 3. One-shot 최초 실제 시작과 정상 후속 cue는 다르다

A1의 원 `L=min(양수 USE_LIMIT,500ms,2000ms)`는 최초 실제 음향 시작까지의 한도다. 500ms는 이번 선택이며 기존 2000ms는 절대 상한이다. use_limit=0은 사용불가, 더 짧은 값은 유지하고 긴 값으로 500ms를 늘리지 않는다. 과거 min/AND 금지·관계 미합의 기록은 이번 D04에서 개정한다.

INPUT은 RX 기준 Age와 시간 오차에서 원 로컬 시각 구간 `[originalAt,originalAtLatest]`를 보호한다. callback payload Age는 received_at_ms까지 포함하고 RX→소비 경과는 한 번만 반영한다. 미측정 전송 공백/포화/reset/wrap이면 fresh로 인정하지 않는다. [계산식·생산 조건](../90_BINDING/92_NETWORK_BINDING.md#time)을 따른다.

STORE/POLICY/PB/STREAM/TX의 최초 START 직전에는 `now.latestNow-originalAt<L`과 원 권한/replayAllowed를 확인한다. 최초 actual은 충분한 물리 fact와 당시 권한이 있고 `actual.earliest>=originalAtLatest && actual.latest-originalAt<L`일 때만 적법하다. `actual.earliest-originalAtLatest>=L`이면 확정 기한 위반, 경계 걸침/비교 부족은 적법성 불확실이다. 499ms는 시간조건만 통과하고 500/501ms는 통과하지 않는다.

in-flight/unknown 중 만료는 미래 권한 차단·종료 요청으로 연결한다. NO_START/Expired/retirement를 합성하지 않고 원 actual/coverage·통지·안전 정리를 보존한다. 충분한 전체 NO_START/미시도와 retirement 후에만 Expired 확정이 가능하다. Started된 동일 Session의 정상 후속 cue에는 L을 다시 적용하지 않는다. 300ms 도착/700ms 최초actual은 미허용이고 정상 후속cue700ms는 최초 gate 대상이 아니다.

<a id="stateful-hold"></a>
## 4. Stateful 상태·현재 품질·Hold·후보를 분리한다

마지막 유효 ACTIVE/CLEAR 판단, 현재 입력 품질, 현재 선택 후보는 STORE 안에서도 서로 다른 의미다. ACTIVE 반복 수신은 새 Session 생성 신호가 아니다. INVALID/STALE/누락/무응답은 CLEAR가 아니며 마지막 유효 판단을 정상으로 덮지 않는다.

Hold의 기준은 **첫 신뢰 상실 시각과 원래 `validUntil` 중 더 이른 경계**다. 원본 기한이 정의된 입력의 `validUntil`을 비교하며 시간 근거 부족을 정밀 기산으로 합성하지 않는다. 반복 invalid, 재평가, 선점·복구로 연장하지 않는다. Hold 만료는 후보 제거이며 입력 품질의 정상화나 CLEAR로 만들지 않는다. A1 시연 후보는 Anti-Pinch TEST250ms/Occupant500ms/Rear150ms/Activation0ms다. Hold는 마지막 유효 Stateful 경고의 제한된 적용 기간이며 원 source TTL·품질을 정상으로 되돌리지 않는다.

<a id="hold-reselection"></a>
현재 fresh 유효 상태 또는 **구간 전체가 Hold 안인 마지막 유효 Stateful 경고**는 새 Session 재선택 후보가 될 수 있다. Hold 경로에서 STORE는 보호된 원 key·출처/승인 세대·원 시각·최초 상실/anchor가 유효하고, 유효 CLEAR·확정 Hold 만료로 제거되지 않았을 때만 `APPLICABLE / newSessionAllowed=true`를 파생한다. Rear는 현재 fresh ENABLED, riskApplicable, riskFence보다 새로운 위험 근거의 결합도 필요하다. DISABLED/activation 상실로 제거한 Hold는 ENABLED만으로 복원하지 않는다. 시계·출처 미입증에서는 허가하지 않는다. 원 품질이 INVALID/STALE여도 그 값을 새 정상 의미로 쓰지 않고 마지막 유효 판단만 제한 사용한다.

POLICY는 이 현재 후보로 새 평가하고 PLAYBACK은 old 전체 retirement·provider/PCM/device 안전 종료·현재 제한/자원과 최신 revision/시간을 채택 및 실제 START 직전에 재검사한다. 새 Session은 정책 시작점에서 시작하며 원 identity·Meta·품질·firstLossAt·anchor를 갱신하지 않는다. `newSessionAllowed`는 이 후보의 허용 조건이지 실제 출력/retirement 증명이 아니다. One-shot의 replayAllowed·L·Started/NO_START/STOP 계약에는 이 Stateful 규칙을 적용하지 않는다.

<a id="hold-certainty"></a>
`deadline=anchor+Hold`와 현재 구간을 같은 시간 단위·domain·epoch에서 비교하고 reset/wrap 연속성과 `earliestNow<=latestNow`를 입증한 뒤 다음을 적용한다.

| 시간 근거 | 후보 관측과 처리 | deadline=150ms 예 |
|---|---|---|
| `latestNow < deadline` | 다른 원본/Rear 조건도 충분하면 APPLICABLE, 새 Session 검토 가능 | [147,149] |
| `earliestNow >= deadline` | 확정 Hold 만료: EXPIRED, newSessionAllowed=false, 후보 제거·종료 요청 | [151,153], [150,150] |
| `earliestNow < deadline <= latestNow` 또는 비교 근거 부족 | UNCERTAIN, newSessionAllowed=false, 재확인·보수적 종료 요청. 확정 만료 아님 | [149,151], [149,150] |

불확실 관측과 원 부족 사유/revision을 보호하며 FLOW는 새 선택 basis가 불충분해도 귀속이 확인된 현재 Session의 기존 종료 lane을 연결한다. 실제 STOP/retirement는 하위 사실로만 확인한다. 보수적 종료 요청을 CLEAR·위험 정상화·물리 STOP 완료·과거 발생 이력 삭제로 바꾸지 않는다. 동일 원 근거로 불확실성이 해소돼도 이미 요청한 old 종료를 취소하지 않으며, Hold 내 재선택은 그 retirement 뒤 새 평가가 필요하다.

Rear는 DISABLED 결합으로 위험 후보/Hold를 제거하는 local 규칙을 유지하며 CLEAR와 구별한다. 다시 enabled됐다는 사실만으로 옛 위험 후보를 부활시키지 않는다. 선점 자체는 CLEAR가 아니므로 현재 유효 또는 위 조건을 충족한 Hold 내 Stateful은 옛 출력 retirement 뒤 새 Session에서 정책 처음부터 재선택할 수 있다. 옛 PCM offset에서 이어 재생하지 않는다. [STORE 로컬](../20_MODULES/21_STORE_MODULE.md#stateful)과 [PLAYBACK 정책](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes)을 함께 따른다.

<a id="coherent-read"></a>
## 5. 읽기 관측은 적용 완료와 인과 관계를 확인한다

FLOW는 같은 처리 순간이라는 이유만으로 STORE·PLAYBACK·HEALTH의 서로 다른 revision을 일치했다고 보지 않는다. 그 평가에 필요한 사실이 각 owner에 반영 완료됐고 서로 인과적으로 연결되는 `VssReadBasis`를 확인한다. 읽기 함수는 숨은 반영을 하지 않는다.

관측 수집 뒤 관련 owner 변화가 있거나 근거가 부족/손실되면 새 START의 현재 판단에 옛 결과를 그대로 쓰지 않고 필요한 관측을 다시 수집한다. 이 재수집이 원본 age나 permission을 갱신하는 것은 아니다. Snapshot/Command는 해당 평가·요구 수명에 필요한 근거만 보존하며 전역 Context나 새 revision manager를 만들지 않는다.

<a id="deadline-owners"></a>
## 6. 기한 판단의 owner와 로컬 경계

| 기존 owner / Boundary | 적용하는 시간 책임 | 별도 유지할 조건 |
| --- | --- | --- |
| INPUT | 원본 시간/품질 해석 | source trust·PRODUCT/TEST·WINDOW 구현 보류 |
| STORE | 원 age·후보 freshness·Stateful Hold·replay | 실제 Started/최종 fact 반영과 분리 |
| PLAYBACK | 전체 최초 시작 적법성·cue/반복 대기·전체 정리 | 실제 사실 시각과 후속 권한, 단일 cue≠전체 완료 |
| AUDIO STREAM | 준비/진행·PCM 생산/제출 권한 | STOP 뒤 생산 차단, provider/PCM 참조 종료 |
| AUDIO TX | 장치 사실/관측 시간·회차 대응 | 차기 DMA 방문까지 실제 쓰기 안전성은 [Buffer §4](50_AUDIO_BUFFER_TRANSPORT.md#safe-write) |
| AUDIO CONTROL | 원래 구성/operation의 readiness 근거 | 옛 성공으로 새 구성을 정상화하지 않음 |
| HEALTH | 현재 Fault instance의 제한·복구 허용/효과 확인 | 허용과 수행·검증·해제는 [Fault §3](40_FAULT_RECOVERY.md#recovery-flow) |
| Runtime Boundary | 현재 시간·연속성 문맥 제공 | 위 owner의 정책 판단을 대신하지 않음 |

## 7. 연결 원본과 미정

[Input Meta](../40_DATA/10_INPUT_DATA.md#vssinputmeta) · [STORE 시간/상태](../40_DATA/20_STORE_DATA.md#store) · [ReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [PLAYBACK](../40_DATA/40_PLAYBACK_DATA.md#playback) · [Runtime 시간 Boundary](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md)

시간 단위·epoch/wrap/reset·source 변환/신뢰·실제 timestamp 확보, Hold/UseLimit의 실제 생산·실측, 준비/정리 timeout과 불명확 최종화 조건, refill/IRQ 지연과 물리 기한은 [Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)에 남긴다. R4에서 새 시간값·재시도 횟수·C 타입을 결정하지 않는다.
