# Timing / Freshness — 원 발생, 실제 사실 시각, 현재 판단

> R4 · 2026-10-08 · 기존 시간 의미의 공통 Contract · 수치/물리 시간 입증은 기존 TBD 유지

[Contract Overview](00_CONTRACT_OVERVIEW.md#timing)

원 발생의 age, 장치 사실의 발생 시각, 소프트웨어 처리 현재 시각과 읽기 관측의 revision은 다른 근거다. 이 계약은 기존 Meta·Evidence·ReadBasis를 연결하며 새 timestamp 필드나 timeout 정책을 추가하지 않는다.

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

**`[잠정] original age < 2초`는 아직 실제 시작하지 않은 One-shot의 최초 actual gate**다. sender/relay 지연·대기·준비를 포함한 원 발생 기준이며 정확히 2초인 경우는 허용하지 않는다. 숫자의 제품 확정과 실제 관측 입증은 후속 TBD다.

STORE의 후보 유효 판단, PLAYBACK의 채택/START 제출 직전 확인, AUDIO STREAM/TX의 원본 첫 시작 조건 보존, PLAYBACK의 actual 사실 적용은 각 owner의 단계다. winner·prepared·제출·accepted/enabled가 기한 내여도 actual이 기한 내였다는 뜻은 아니다. actual 적용 때 원래 사실 시각·권한을 대조하고 기한 밖 출력은 위반 사실로 보존·정리한다. 단순 age 경과만으로 `START_IN_FLIGHT`를 Expired/NO_START로 만들지 않고 [Async §3~4](20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)의 지식·차단 조건을 함께 본다.

원래 최초 Started가 확인된 정상 같은 Session의 다음 cue·반복 대기는 새 Occurrence의 최초 시작이 아니다. 전체 정책이 유효하면 age가 2초를 넘었다는 이유로 정상 실행을 중단하거나 first-start gate를 다시 적용하지 않는다. `firstStartMeta`는 해당 최초 gate가 필요한 START에만 의미가 있고 정상 후속/STOP에는 없다. Started One-shot은 선점·복구 뒤 자동 재실행하지 않는다.

`USE_LIMIT`은 원 발생에 연결된 별도 기존 제한이다. 최초 `< 2초` 후보 규칙과 관계·경계 판정은 미정이며 임의의 min/AND 우선순위나 새 통합 timeout으로 확정하지 않는다.

<a id="stateful-hold"></a>
## 4. Stateful 상태·현재 품질·Hold·후보를 분리한다

마지막 유효 ACTIVE/CLEAR 판단, 현재 입력 품질, 현재 선택 후보는 STORE 안에서도 서로 다른 의미다. ACTIVE 반복 수신은 새 Session 생성 신호가 아니다. INVALID/STALE/누락/무응답은 CLEAR가 아니며 마지막 유효 판단을 정상으로 덮지 않는다.

Hold의 기준은 **첫 신뢰 상실 시각과 원래 `validUntil` 중 더 이른 경계**다. 원본 기한이 정의된 입력의 `validUntil`을 비교하며 시간 근거 부족을 정밀 기산으로 합성하지 않는다. 반복 invalid, 재평가, 선점·복구로 연장하지 않는다. Hold 만료는 후보 제거이며 입력 품질의 정상화나 CLEAR로 만들지 않는다. Hold 실제 값/입력별 확정은 TBD다.

Rear는 DISABLED 결합으로 위험 후보/Hold를 제거하는 local 규칙을 유지하며 CLEAR와 구별한다. 다시 enabled됐다는 사실만으로 옛 위험 후보를 부활시키지 않는다. 선점 자체는 CLEAR가 아니므로 현재 유효·Hold 내 Stateful은 옛 출력 retirement 뒤 새 Session에서 정책 처음부터 재선택할 수 있다. 옛 PCM offset에서 이어 재생하지 않는다. [STORE 로컬](../20_MODULES/21_STORE_MODULE.md#stateful)과 [PLAYBACK 정책](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes)을 함께 따른다.

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

시간 단위·epoch/wrap/reset·source 변환/신뢰·실제 timestamp 확보, Hold/UseLimit 확정·관계, 준비/정리 timeout과 불명확 최종화 조건, refill/IRQ 지연과 물리 기한은 [Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)에 남긴다. R4에서 새 시간값·재시도 횟수·C 타입을 결정하지 않는다.
