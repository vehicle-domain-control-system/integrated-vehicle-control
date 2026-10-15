# Network–VSS 입력 연결 — A1, 2026-10-15

> 설계 개정 A1 / 구현·Host·보드 시험 NOT_RUN. 독립 검수 대기.
> Network 원문 `70028972ec713a3ec6b5f0a419e0dc89fd963844`, VSS 수정 전 `73edd8e22d4d2b4b4a99b2835348cb2350a69ba0`.

[INPUT Data](../40_DATA/10_INPUT_DATA.md) · [시간](../50_CONTRACTS/30_TIMING_FRESHNESS.md) · [복구](../50_CONTRACTS/40_FAULT_RECOVERY.md) · [Interface 지도](../../sw_detail/02_INTERFACE_MAP.md)

## 1. 경계와 개정 범위

```c
void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);
void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
```

앞 두 callback의 본체는 VSS 기존 INPUT 통신 연결이다. Network는 구조·링크 순서·전달을 소유하고, DOMAIN은 사건/위험/활성 판단, INPUT은 의미 검증, STORE는 사건/현재 경고, PLAYBACK은 실제 시작을 소유한다. callback은 R6 `Input_Process`와 다른 경계다. 18 Core, single Session, PCM A/B, actual/NO_START/unknown, STOP 뒤 하위 참조 수명을 유지한다. 새 public API·Task·범용 Queue를 추가하지 않는다.

이 문서의 D01~D08/D12는 이번 사용자가 허용한 Network/VSS 설계 결정이다. 과거 R2~R6/D2/P1B의 PASS와 Q01~Q14·Owner35 기록은 당시 범위의 이력이다. 현재 입력 설계에서는 아래 규칙과 함께 개정한 Data/계약/의사코드를 적용한다. 제품 ABI·양단 실제 구현·시간/물리 충분성은 승인한 것으로 쓰지 않는다. Status D09~D11은 후속 [A2 선택](93_NETWORK_STATUS_BINDING.md)으로 보완했으며 이 문서의 A1 입력 계약은 유지한다.

<a id="mapping"></a>
## 2. 전체 입력 필드 대응

`meta.source=DOMAIN`, `meta.generation=수용한 DOMAIN BOOT`가 의미 권한이다. 별도 `evidenceSource/evidenceGeneration/evidenceOrdering`은 원 관측 계보다. `deliveryOrder`는 검증된 CAN TX이며 원 평가 번호가 아니다. `ordering`은 의미 원 순서가 입증된 경우만 사용하고, Event의 occurrence나 세 경고의 원 센서 순번에서 DOMAIN 판단 순서를 만들지 않는다. 이 논리 alias의 미입증 상태는 실제 C 숫자0으로 확정하지 않는다.

| Net 입력 전체 필드 | VSS 자료·의미 | 생산→변환→소비 |
|---|---|---|
| RX `source.boot_id`, `source.tx_sequence` | Meta `generation`, `deliveryOrder`; 원 센서 Boot/seq와 별도 | Network 검증→INPUT→순서/손실 문맥 |
| RX `received_at_ms` | 아래 RX 시간 기준점. INPUT 시간 투영 후 원본/오차를 보호 | Driver 포착→Network→INPUT |
| Event `event_kind` 1/2/3/4/5 | `signal` WELCOME/GOODBYE/LOCK_COMPLETE/UNLOCK_COMPLETE/LOCK_ERROR, `value=EVENT` | DOMAIN→INPUT→STORE |
| Event `occurrence_id` | Evidence `occurrence` → OneShotEvent `occurrence`; key=(PRODUCT 또는 TEST, DOMAIN, DOMAIN Boot, occurrence_id). 종류는 key에 넣지 않아 같은 ID의 종류 충돌을 검출 | DOMAIN 발급→INPUT 보존→STORE ledger |
| Event `original_age_ms`, `use_limit_ms` | Meta `originalAt/originalAtLatest`, `useLimit`, `validUntil`; §4의 원 기한 | DOMAIN 발생→Network 누적→INPUT→STORE/POLICY/PB/STREAM/TX |
| Event `value_quality`, `input_availability` | Evidence `quality`, Meta `availability`; Event에는 reason이 없어 `reason=NOT_PROVIDED` 의미 태그 | DOMAIN→INPUT §3→STORE/진단 |
| Event `related_command_id` | Meta `relatedCommand` 선택 참조; 0은 없음. occurrence/순번으로 쓰지 않음 | DOMAIN→INPUT 보호→원 거부 진단 |
| `antipinch_source_boot/sequence/original_age_ms` | evidenceSource=WINDOW, evidenceGeneration/Ordering, 원 시간; PRODUCT 지원 제외, 격리 TEST만 가능 | WINDOW 근거→DOMAIN→INPUT |
| `antipinch_state/quality/reason/availability` | ANTI_PINCH, 0/1→CLEAR/ACTIVE는 VALID일 때만; §3의 품질/사유/가용성 | DOMAIN→INPUT→독립 STORE 경고 |
| `occupant_hazard_source_boot/sequence/original_age_ms` | evidenceSource=PI_VIA_CIS, evidenceGeneration/Ordering, 원 시간. CIS 재포장 Boot로 Pi Boot 대체 금지 | Pi→CIS→DOMAIN→INPUT |
| `occupant_hazard_state/quality/reason/availability` | OCCUPANT_HAZARD, 0/1→CLEAR/ACTIVE는 VALID일 때만; §3 | DOMAIN→INPUT→독립 STORE 경고 |
| `rear_obstacle_source_boot/sequence/original_age_ms` | evidenceSource=CIS_REAR, evidenceGeneration/Ordering, 원 시간 | CIS→DOMAIN→INPUT |
| `rear_obstacle_state/quality/reason/availability` | REAR_RISK, 0/1/2→CLEAR/CAUTION/EMERGENCY는 VALID일 때만; §3 | DOMAIN→INPUT→STORE 위험 |
| `rear_detection_activation` | REAR_ACTIVATION, 0/1→DISABLED/ENABLED는 유효 meta일 때만 | DOMAIN 활성 정책→INPUT→STORE gate |
| `activation_meta_format` | Codec marker. 1은 독립 근거, 0은 부재. VSS 기능 enum이 아님 | 양단 Codec→INPUT 부재 처리 |
| `rear_activation_quality/reason/sequence/age_ms` | quality/reason, Meta ordering 및 evidenceOrdering, 원 시간. evidenceSource=DOMAIN_ACTIVATION, evidenceGeneration=DOMAIN Boot. availability는 필드가 없어 NOT_PROVIDED | DOMAIN 실제 활성 평가→INPUT→STORE gate |

Meta의 reason/availability는 이름으로 검증한 의미 태그이며 Net enum cast가 아니다. Event/Activation의 없는 필드를 NONE/AVAILABLE로 위조하지 않는다. Signal별 지원 구성과 원 raw 묶음은 기존 INPUT 연결 문맥에 보호한다. Evidence/QualityChange/StateUpdate는 동기 borrow이며 STORE가 필요한 Meta·기한·최초 상실을 자기 수명으로 보호한 뒤 반환한다. callback 반환 뒤 원 포인터를 참조하지 않는다.

<a id="quality"></a>
## 3. WARNING60·품질·순서

WARNING60 used60/frame64/DLC15를 D01 단일 설계로 선택한다. WARNING51은 이력 비교용이다. 같은 DLC나 B51 값으로 구형을 자동 판별하지 않는다. format1은 B52..59의 독립 활성 meta를 검증한다. format0은 **B50=0, B52..59=0**인 정규 부재 표현으로 선택한다. 이때 바이트0을 품질 OK/활성 DISABLED의 새 판단으로 소비하지 않고 미평가 또는 기존 활성 상실로 처리한다. 그 외 marker, 정규 부재 위반, 예약 raw, padding·ID·DLC·flags 오류는 전체 프레임 거부다. format0의 다른 세 경고는 정상 검증한다.

| 구조 검증 뒤 조합 | INPUT/STORE 결과 |
|---|---|
| OK, reason NONE, availability AVAILABLE 또는 LIMITED, 유효값·출처·시간 | VALID. LIMITED는 원 producer가 이 값의 유효성을 보증한 범위에서만 사용하며 VSS 출력 가용성으로 복사하지 않음 |
| INVALID 또는 사용불가 availability | 해당 항목 INVALID. 원 reason/availability 보호. 값으로 사건/해제 생성 없음 |
| STALE 또는 원 기한 도달 | 해당 항목 STALE. 새 정상 의미 생성 없음. Event 새 최초 시작 차단; 마지막 유효 Stateful 경고는 §5 제한 Hold/재선택 |
| 최초 NO_DATA/format0 | UNASSESSED. 원 시각/정상값 생성 없음 |
| 유효 이력 이후 NO_DATA/format0 | INVALID 상실. 기존 마지막 유효값을 보존하되 정상 품질 유지 금지 |
| 품질 OK이나 NONE 아닌 reason 등 의미 모순 | 해당 항목 INVALID와 원 모순 진단. 다른 독립 경고는 유지 |
| SOURCE Boot0 또는 필요한 순서/시간 미입증 | 정상 적용 불가. 이미 동기화된 DOMAIN의 더 최신 전달에 의한 제한은 별도 검증 후 적용; 정상 승격 없음 |

지원하지 않는 WINDOW PRODUCT는 양단 고정 구성으로 제외한다. Network가 C2 UNSUPPORTED를 일반 UNAVAILABLE와 동등하다고 선언하지 않는다. **VSS 전용 고정 구성 축약**으로 anti 부분만 state0/quality NO_DATA(3)/reason NO_DATA(6)/availability UNAVAILABLE(2)/sourceBoot0/sequence0/age65535를 송신한다. receiver는 고정 unsupported 구성과 함께 해석하므로 정상 CLEAR를 만들지 않는다. 이 조합은 기존 일반 품질 표의 VSS 한정 예외이며 다른 노드에 확대하지 않는다. 실제 DOMAIN의 해당 입력 생성/구성 일치는 외부 조건 E02다.

C2 UNCONFIRMED+비정상 quality는 state0 placeholder, 원 비정상 quality/reason/availability/계보를 보존하고 값을 무시한다. UNCONFIRMED+OK 또는 지원 대상의 UNSUPPORTED는 해당 항목을 INVALID(2)/DATA_INVALID(9)/UNAVAILABLE(2)/state0으로 **명시적 안전 축약**한다. 원 C2 자료·축약 사유는 Network 원 항목 진단에 보존한다. 이는 의미 동등 변환이나 위험 CLEAR가 아니며 다른 유효 항목의 전송을 막지 않는다. 원 구별을 상대에 완전 전달해야 하면 E02의 VSS 전용 상태 확장 후보가 필요하다. 공유 enum/예약 bit는 변경하지 않는다.

순서는 먼저 승인된 DOMAIN Boot와 증가한 non-wrap TX를 확인한다. 같은 TX/같은 bytes는 duplicate, 같은 TX/다른 bytes 및 과거 TX는 거부한다. 최신 TX는 전달 순서만 증명한다. Warning은 네 항목으로 나눈 뒤 다음을 독립 적용한다.

1. 같은 원 근거 순번의 **제한 변경**(OK→INVALID/STALE/NO_DATA, availability 사용불가, reason 제한)은 새 TX에서 품질 상실로 전달한다. 같은 상실 반복은 firstLossAt/Hold를 변경하지 않는다. 과거 TX의 INVALID는 현재를 덮지 않는다.
2. 동일 근거·동일 의미의 새 TX는 liveness만 갱신한다. 원 Age를 줄이거나 평가 번호/validUntil을 연장하지 않는다. INPUT은 기존 Network monitor의 last advancing valid RX를 owner 문맥에서 읽는다. callback 재발행 없이도 no-RX 판단에 반영한다.
3. 같은 원 근거에서 DOMAIN 정상 값이 변경되면 duplicate로 버리지 않고 **그 항목의 판단 순서 미입증**을 기록하고 INVALID 제한을 적용한다. 값 변경/해제는 적용하지 않는다. 최신 TX를 DOMAIN 평가로 간주하지 않는다. E01이 해결되기 전 정상 회복은 새 원 근거 순번 및 fresh 정상 판단을 요구한다.
4. 같은 근거에서 정상 품질로 승격하는 것도 새 실제 평가 증거 없이 허용하지 않는다. 원 근거 새 순번/승인된 새 세대와 정상 조합이 있어야 회복한다. activation은 독립 DOMAIN 평가 순번으로 비교한다.

<a id="time"></a>
## 4. RX 기준 Age와 최초 실제 시작

Network가 callback에 넘기는 payload Age는 **`meta.received_at_ms`의 RX 기준 Age**다. 원 생산→송신 대기→검증된 전송 공백까지 포함한다. callback 직전 나이는 별도 계산값으로 admission에만 사용하고 RX 기준 payload에 덮어쓰지 않는다. INPUT이 RX→owner 소비 경과를 한 번 더하며 이후 각 owner는 보호된 동일 원 시각을 사용한다. 원인 없는 0 초기화/구간 중복 가산을 금지한다.

RX 시각 구간 `[rLo,rHi]`, RX 기준 Age 구간 `[aLo,aHi]`가 입증되면 로컬 원 시각 구간은 `[rLo-aHi,rHi-aLo]`다. `originalAt`은 이 구간 하한, `originalAtLatest`는 상한이다. ECU 절대시각 동기화는 요구하지 않는다. 송신 직후→RX 공백은 5ms를 시연 예산 후보로 두되 측정/입증 전 fresh 승인 근거로 사용하지 않는다. 양끝 timestamp 오차·tick 양자화도 Age 구간에 포함한다. 포화65535, 미측정 공백, epoch/reset/wrap 비교 실패는 불확실이며 정확한 원 시각/정상 fresh를 만들지 않는다.

Event의 `L = min(수신한 양수 use_limit_ms, 500ms, 2000ms)`를 `meta.useLimit`에 보존한다. use_limit=0은 사용불가다. 500ms는 이번 기본 설계 선택, 2000ms는 기존 절대 상한이다. 더 긴 Wire 값으로 500ms를 늘리지 않으며 더 짧은 값은 존중한다. `validUntil=originalAt+L`은 보수적 새 시작 마감이다. 재전달에서는 동일 key의 기존 원 시각·기한을 젊게 하지 않는다.

```text
INPUT: occurrence와 원 Meta/L 검증 → STORE 수용/거부 (실제 시작 아님)
STORE/POLICY/PB/STREAM/TX 최초 START 직전:
  연속성·출처·replayAllowed·권한이 유효하고 now.latestNow-originalAt < L일 때만 가능
PB 최초 actual 적용:
  충분한 물리 actual, 당시 권한, 동일 domain/epoch가 선행
  actual.earliest >= originalAtLatest && actual.latest-originalAt < L → 적법 Started
  actual.earliest-originalAtLatest >= L 또는 구간 전체 권한 밖 → 확정 위반·정리
  나머지(경계 걸침/비교 부족) → 적법성 불확실, 원 actual/coverage 보존·차단/정리
```

499ms는 시간조건만 통과, 500/501ms는 미허용이다. 300ms 도착 후 700ms actual은 적법 최초 시작이 아니다. 이미 적법 Started된 동일 Session의 후속 cue700ms에는 최초 gate를 재적용하지 않는다. START_IN_FLIGHT/unknown 중 만료되면 미래 시작 권한을 차단하고 종료를 요구하지만 NO_START/Expired를 합성하지 않는다. 충분한 원 NO_START와 retirement 후에만 Expired 확정이 가능하다. 시간 부족/위반을 기존 물리 실제 출력 fact의 삭제로 처리하지 않는다.

<a id="warning-time"></a>
## 5. Warning freshness·수신 중단·Hold

| 입력 | 원 기산·수명 후보 | 전달/수신중단 | 제한 Hold 후보·종료 |
|---|---|---|---|
| Anti-Pinch | WINDOW 원 평가 250ms; PRODUCT 미지원, TEST만 | Warning50ms/150ms, TEST는 격리 시간원 | 250ms; 확정 만료/경계 걸침은 아래 3분기 |
| Occupant | Pi 원 관측 1000ms; CIS/DOMAIN 재포장으로 갱신 금지 | Warning50ms/150ms | 500ms; 확정 만료/경계 걸침은 아래 3분기 |
| Rear risk | CIS 후방 원 측정 150ms | Warning50ms/150ms | 150ms; 확정 만료/경계 걸침은 아래 3분기 |
| Rear activation | DOMAIN 원 Power 평가 약200ms/500ms를 **동일 원 판단으로 연결하는 설계 조건 E03** | Warning50ms/150ms | 0ms; 상실 즉시 gate 닫음 |

수치는 실측 보장이 아니다. 마지막 유효 Stateful 경고는 [원 출처·시각·고정 Hold·Rear gate/fence 조건](../50_CONTRACTS/30_TIMING_FRESHNESS.md#hold-reselection)을 만족하면 old 전체 retirement 뒤 새 Session 정책 시작점에서 재선택할 수 있다. 원 source TTL·품질·anchor를 갱신하거나 INVALID 값을 새 정상 의미로 소비하지 않는다. 같은 단위/domain/epoch·reset/wrap 연속성이 입증되면 `latestNow < anchor+Hold`는 Hold 내, `earliestNow >= anchor+Hold`는 확정 만료다. 경계 걸침 또는 시간/출처 미입증은 UNCERTAIN/newSessionAllowed=false로 보수적 종료·재확인을 요구하며 확정 만료와 구별한다. CLEAR·위험 정상화·물리 STOP 완료·이력 삭제를 합성하지 않는다. [3분기와 후속](../50_CONTRACTS/30_TIMING_FRESHNESS.md#hold-certainty)을 따른다.

`firstLossAt`은 최초 상실 근거의 보수적 경계다. `anchor=min(firstLossAt,lastValidMeta.validUntil)`를 한 번 고정한다. no-RX의 상실 경계는 마지막 정상 구조/증가 TX 수신 시각+150ms, 원 TTL 상실은 원 validUntil이다. 늦게 실행된 점검 now를 lossAt으로 써서 연장하지 않는다. 반복 INVALID, 같은 관측 재송신, 선점, 복구는 최초 상실/기한을 바꾸지 않는다. malformed/같은 TX duplicate는 정상 수신시각을 갱신하지 않는다.

`Input_Process(NULL,time,…)`는 9 signal×PRODUCT/TEST의 최대18 문맥을 bounded 점검하고 필요한 각 quality lane을 STORE에 전달한다. FLOW의 기존 시간 기회는 설계 예산10ms 이하로 요청한다. 실제 주기/WCET/stop 지연은 E04 실측 조건이다. no-RX에도 하위 fact 적용→STORE 기한→현재 후보 재중재를 진행한다.

NULL 점검의 단일 VssInputApplication은 전체 ACK가 아니다. 고정 순서의 첫 거부 결과 전체, 없으면 첫 변화 결과 전체, 없으면 NO_CHANGE/false/NOT_SUBMITTED를 반환한다. 문맥별 실제 STORE 반환/반영 revision·거부 진단은 따로 보호하며 FLOW가 최신 owner 관측과 손실/후속 의무를 읽는다. 서로 다른 문맥의 세 필드를 섞어 성공을 합성하지 않는다.

Rear DISABLED/activation 상실은 gate와 riskApplicable/Hold를 제거하고 당시 위험 근거 key를 fence로 보호한다. 재활성화는 단독으로 위험을 부활시키지 않는다. fence보다 엄격히 새로운 위험 근거와 현재 fresh 활성을 결합해야 한다. 초기 ENABLED+fresh 위험의 같은 bundle은 허용한다. CAUTION→EMERGENCY→CLEAR는 새 적용 가능한 DOMAIN 위험 근거로 해당 후보만 변경한다. 거리를 VSS가 재판정하지 않는다.

<a id="owner"></a>
## 6. owner 문맥·보호·완료점·유한 용량

기본 연결은 기존 FLOW owner의 직렬화된 Network dispatcher 기회에서 callback을 동기 소비한다. ISR에서는 stable frame 포착만 한다. 같은 owner가 Event1개 또는 Warning 최대4개를 입력 처리하고 callback이 반환한 뒤 한 번 재중재한다. reentrant callback/다른 Task에서 직접 호출은 허용하지 않으며 실제 scheduler binding은 E04다. 비동기 이관을 채택하려면 반환 전 깊은 복사·유한 용량·실패 계약을 별도 검수해야 한다. 이번 기본안에 추가 전달 Queue는 없다.

```text
callback(message,rx): // 동일 INPUT owner, message/rx는 동기 borrow
  Network 구조/DOMAIN 전달 순서 승인 pair를 확인
  Event1개 또는 Warning4개를 §2~5로 변환
  Warning은 activation → rear risk → occupant → anti 순서로 Input_Process 호출
  각 결과의 validation/contextApplied/storeAdmission과 원 거부 진단을 보호
  후항목 실패로 이미 적용한 독립 앞항목을 rollback하지 않음
  전체 bundle 뒤에만 FLOW 재중재; callback 반환은 전달 완료만 의미
```

| 완료점 | 관측·실패 |
|---|---|
| Network decode/admission/void callback 종료 | 형식/전달 완료. STORE/actual 성공을 의미하지 않음. Network는 반환 뒤 자동 재호출하지 않음 |
| INPUT contextApplied=true | 최신 출처/품질 문맥 반영. STORE 거부여도 rollback 없음 |
| STORE ACCEPTED/UPDATED/DUPLICATE/REJECTED | 실제 자료 수용/현재 변경/기존 발생/거부. 원 identity·원 기한·실패 단계 보존 |
| PLAYBACK FIRST_ACTUAL_START / whole final | 원 시간·출력 충분성까지 확인한 개별 사실. 콜백/driver accepted로 생성 금지 |

설계 용량 후보는 입력 문맥18, 동시 borrow bundle1(최대4 입력), STORE occurrence ledger128, INPUT 기존 진단 보관16이다. ledger128은 시연의 승인된 DOMAIN 한 세대당 최대128 distinct Event(예: 1분당2개×64분)를 수용/거부 tombstone까지 보존하는 예산이며 제품 입력률 보장이 아니다. 하나의 accepted 항목은 최초/최종 사실과 replay 추적 보호까지 예약한다. 세대 안에서 임의 eviction하지 않고 full이면 이후 새 Event 수용을 제한한다. 상태/품질/기한/기존 사실 적용에는 Event 슬롯을 요구하지 않는다.

진단16은 한 bundle 최대4개 실패의 네 처리 기회분 후보다. 가득 차면 기존16개를 덮지 않고 기존 INPUT owner의 `diagnosticLossCount`를 포화 증가시키고 `diagnosticLossLatched`를 세운다. 첫/최근 손실 원 key·단계는 문맥당 고정 두 요약 슬롯(최대36)에 보존한다. 모두 전달했다고 주장하지 않으며 손실 해당 범위의 검수 충분성을 제한한다. 다른 유효 경고 전체를 차단하지 않는다. 용량·입력률·HEALTH 소비 지연의 제품 확인은 E04다.

<a id="recovery"></a>
## 7. 세대·입력 복구·출력 복구

`inputLossAt`은 최초 신뢰 상실의 보호된 VssFactTime이며 반복으로 갱신하지 않는다. 시각이 부족하면 상실 이전 actual임을 추정하지 않는다. `replayAllowed`는 STORE occurrence별 단조 false 전이 latch다. 수용 때 true이며 입력 신뢰 상실/DOMAIN 세대 종료/시간 연속성 상실이면 false로 바꾼다. 충분한 미시도/NO_START인 Pending은 Expired로 종결하고 이력을 보존한다. in-flight/unknown은 상태를 억지로 Expired로 바꾸지 않고 차단/정리·원 결과 확인을 계속한다. NO_START_REEVALUATE가 와도 false를 true로 복원하지 않는다. 늦은 적법 actual이 상실 이전에 발생했다는 근거가 충분하면 원 occurrence에만 반영하고 종료를 취소하지 않는다.

STORE는 후보 관측 `newSessionAllowed`에 Event의 replayAllowed·원 L·신뢰 또는 Stateful의 현재 유효 또는 §5의 Hold 내 재선택 조건을 파생한다. POLICY/PB는 이 읽기 필드를 소비하고 STORE private record를 직접 읽지 않는다. Hold 경계 걸침/미입증은 UNCERTAIN으로 노출하고 현재 Session 유지 허가로도 사용하지 않는다. PB 새 채택·최초 START 직전에도 최신 후보 revision과 newSessionAllowed를 재확인한다.

DOMAIN Boot 변경은 모든 해당 DOMAIN 입력/미시작 Event를 격리한다. 센서 Boot 변경은 해당 Warning 계보와 Rear 결합만 무효화하고 다른 센서/DOMAIN Event를 일괄 재초기화하지 않는다. 승인되지 않은 새 Boot·wrap은 새 정상 세대로 수용하지 않는다. source Boot0의 상실은 해당 기존 문맥의 제한에만 쓸 수 있다.

VSS만 재기동하여 replay 이력이 사라졌는데 DOMAIN Boot가 유지되면 옛 Event를 새 것으로 받을 수 없다. 이번 기본안은 Event admission을 닫고, 재기동 전 ledger의 신뢰 가능한 복원 또는 **VSS 동기화 이후 DOMAIN의 실제 새 승인 세대**가 확인될 때 열도록 선택한다. 양단 재기동/지속저장 정책은 E05다. 수신한 첫 Boot를 새 세대라고 추정하지 않는다. Warning은 현재 세대·계보·시간의 새 유효 관측으로 독립 재구성한다.

통신/입력 상실 복구는 옛 Event Pending의 부활 사유가 아니다. Event 무수신 자체는 비주기 Event의 loss가 아니며 DOMAIN 감독 상실(Warning/Permission의 승인된 모니터), 명시 품질 상실, 세대/시간 연속성 상실을 사용한다. 출력 장치 Fault만 발생했고 입력 신뢰와 replayAllowed가 유지되었다면, 충분한 전체 NO_START·retirement·실제 HEALTH 제한 해제·원 L 내 freshness를 모두 확인하여 동일 occurrence만 재평가한다. Started/Interrupted/uncertain final은 자동 resume하지 않는다.

`VssNetPriv_OnDomainPermission`은 Network 감독으로 유지한다. 상실은 입력 신뢰 제한의 근거가 될 수 있으나 ALLOW는 `VssRecoveryPermission`, 새 Event, 원 기한 갱신 또는 실제 출력 복구 완료가 아니다. HEALTH 허용→수행→별도 검증→현재 제한 해제 계약을 유지한다.

<a id="external"></a>
## 8. 외부 조건·A2 인계

| ID (A1 로컬 추적 ID) | 현재 안전 처리와 정확한 다음 근거 |
|---|---|
| E01 판단 순서 | 같은 Pi/CIS 근거에서 DOMAIN 값 변경은 해당 항목 제한. 최소 후보: C2 각 위험 판단에 DOMAIN judgment generation/sequence/age를 추가하고 VSS 전용 versioned payload에 전달. 현60B 예약4B에 3개 평가를 임의 압축하지 않음. DOMAIN owner 생성 계약·새 Wire 합의 필요 |
| E02 표현·계보 | §3 VSS 전용 제한 축약/고정 WINDOW 구성을 적용. DOMAIN 원 UNCONFIRMED/UNSUPPORTED 의미의 완전 구별은 전달하지 못함. 이를 요구하면 VSS 전용 지원/판단확인 태그 확장 검토. Pi→CIS→DOMAIN source generation/seq 보존 및 C2 v0.6/Vehicle v0.3 실제 채택 확인 |
| E03 activation | 동일 Power 원 평가 약200ms/500ms의 meta를 Warning에도 전달하도록 선택. 합성 Permission update가 아닌 **원** DOMAIN 평가 사용. 다른 활성 정책이라면 별도 생성 주기≤200ms·원500ms 근거 계약 필요. 생산 확인 전 해당 activation 정상 승격 제한; occupant는 독립 |
| E04 시간·실행·자원 | RX Age 기준, 전송공백/오차, owner 직렬화10ms 예산, ledger128/진단16, 실제 actual/NO_START/STOP/PCM 안전성 검증. 문서의 수치가 성능 측정은 아님 |
| E05 lifecycle | DOMAIN Boot 재사용/wrap 금지·승인, VSS 재기동시 ledger 복원 또는 실제 새 DOMAIN 세대, source 세대 승인 근거 필요 |
| A2 D09~D11 | NetVssStatus 전체 필드 producer/최근 fault occurrence·평가순서·부분 상태·DOMAIN Rx 부분성공은 [A2 Status Binding](93_NETWORK_STATUS_BINDING.md)에서 설계 선택 완료. A1의 거부/손실 관측은 원 owner에 남기며 Status 숫자로 임의 축약하지 않음 |

독립 검수는 D01~D08/D12의 before/after와 T01~T18/T27 정적 추적부터 수행한다. 실제 실행이나 A2는 별도 지시 없이 시작하지 않는다.
