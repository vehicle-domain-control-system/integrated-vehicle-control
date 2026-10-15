# Store_ApplyPlaybackFact — 원 발생 재생 사실의 멱등 반영

[VSS 지도](../../../01_VSS_MAP.md) · [STORE](../00_MODULE_OVERVIEW.md) · CORE-04 · [한국어 의사코드](../pseudocode/Store_ApplyPlaybackFact.md)

> GUIDE · 동결 `0641b2c2268cf47450aeb50302638a45a7a3277f`. TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

PLAYBACK이 정규화한 원 One-shot 발생의 최초 actual/전체 최종 사실 하나를 STORE 이력에 적용한다. raw IRQ·HAL 수락·cue 종료를 STORE가 actual 또는 전체 성공으로 재판정하지 않는다. 적용 완료 증명이 없는 통지는 완료로 응답하지 않는다.

<a id="contract"></a>
## R6 원형과 적용 결과

```text
VssFactApplication Store_ApplyPlaybackFact(
    IN const VssPlaybackFact *fact);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyplaybackfact)는 [R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)의 단일 fact → 적용 결과 의미를 유지한다. `IN`은 동기 불변 borrow 주석이고 C 키워드가 아니다. 실제 Header·key 폭·저장 방식·ABI는 미정이다.

| 전달/반환 | 의미·유효 조건·사후조건 |
|---|---|
| [VssPlaybackFact](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) | `fact/occurrence/session/attempt/kind/occurredAt/basisRevision`의 원귀속과 PLAYBACK의 반영 완료 판정 근거. NULL·미추적·불일치·불충분하면 적용 못 함 |
| `VSS_FACT_APPLIED` | 필요한 정규화 fact·참조가 STORE의 이력 수명으로 보호되고 해당 반영/revision 공개까지 완료 |
| `VSS_FACT_ALREADY_APPLIED` | 동일 원 fact가 이미 **같은 귀속·내용으로 반영 완료됨을 보호 기록에서 증명**. 이번 재전달로 이력/age 재변경 없음 |
| `VSS_FACT_NOT_APPLIED` | 귀속/근거/보호 부족·충돌·부적합 kind. PLAYBACK의 원 fact·통지 의무가 계속 남음 |

반환 타입의 정본은 [VssFactApplication](../../../../design/40_DATA/20_STORE_DATA.md#vssfactapplication)이다. APPLIED/ALREADY_APPLIED는 해당 통지의 STORE 적용 완료이고 Session retirement·자원 회수·실제 보드 성공을 뜻하지 않는다.

<a id="ownership"></a>
## 호출·상태 변경·수명

Caller는 기존 [Playback_Advance/통지 내부 단계](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification)다. 현재 [Playback_Advance 상세 설명](../../PLAYBACK/functions/Playback_Advance.md#pairing)과 [의사코드](../../PLAYBACK/pseudocode/Playback_Advance.md)에서 통지 문맥을 함께 읽는다. STORE는 [VssOccurrenceRecord](../../../../design/40_DATA/20_STORE_DATA.md#vssoccurrencerecord)의 firstStartFact/finalFact/state/tracking/revision과 필요한 적용 흔적만 쓴다. PLAYBACK Session/Attempt/phase·INPUT authority·Stateful ACTIVE/CLEAR/quality/Hold·HEALTH Fault·PCM 회차는 변경하지 않는다. Callee는 필요한 기존 Runtime/보호 경계이며 PLAYBACK 역호출이 아니다.

정규화 fact는 ledger 수명으로 보호한다. PLAYBACK의 임시 반환 포인터를 얕게 보관하고 APPLIED라 할 수 없다. raw HAL record 전체를 replay 수명까지 강제로 보관하지도 않는다. accepted 때 확보한 추적에 필요한 불변 사실/적용 증명이 맞는지 확인하며 표현·용량·직렬화 수단은 TBD다. 기존 accepted/final eviction으로 공간을 만들지 않는다.

<a id="identity"></a>
## 먼저 귀속과 동일 적용 증명

입력 공간이 유효한지 확인한 뒤 원 fact 키·occurrence·Session·Attempt 및 보호된 accepted 연결을 대조한다. 현재 active 포인터·signal·requestId만 같다는 이유로 원귀속을 채우지 않는다. basisRevision은 해당 PLAYBACK 반영 경계와의 인과 연결이고 STORE revision과 숫자가 같아야 한다는 조건이 아니다.

동일 fact 키라도 kind/원 시각/귀속/판정 근거와 보호된 적용 흔적이 충돌하면 NOT_APPLIED다. 같은 occurrence가 이미 Started이거나 final이라는 상태만으로 ALREADY_APPLIED를 반환하지 않는다. **이미 반영된 동일 fact의 재전달**만 멱등 완료로 응답한다. 최초·최종이 동시에 미통지여도 각 사실의 의무는 독립적이다.

<a id="branches"></a>
## 최초 시작과 전체 최종 분기

| 기존 kind | 필요한 분기와 STORE 반영 |
|---|---|
| FIRST_ACTUAL_START | PLAYBACK의 원 최초 적법 actual 근거가 충분하고 원 Pending의 최초 fact가 없을 때 firstStartFact를 보호해 Started. 원 발생 시간/age는 그대로. 충분한 늦은 actual은 final 전 옛 occurrence에만 반영하고 PLAYBACK 종료 phase를 취소하지 않음 |
| WHOLE_COMPLETED / WHOLE_INTERRUPTED | 보호된 firstStartFact와 전체 계획 범위 종료·관련 하위/상위 후속 권한 차단이 충분할 때 finalFact를 보호해 전체 Completed/Interrupted. cue·segment·backend 일부 종료만으로 이 kind를 만들거나 적용하지 않음 |
| START_OUTCOME_UNCERTAIN | 원 PLAYBACK 불명확 **최종** 판정 근거가 확인됐을 때 finalFact와 replay 금지를 보호해 UncertainFinal. 기존 firstStartFact가 있다면 삭제하지 않음. 이 최종은 물리 cleanup 완료와 별개 |
| NO_START_REEVALUATE | 최초 actual/uncertain final이 없고 전체 과거 NO_START·미래 후속 차단·retirement가 충분할 때 원 Pending/Expiry의 **재확인만** 허용. 이 호출은 현재 now/새 age로 만료 또는 새 Pending을 만들지 않음 |

최초 actual의 시각 비교는 원 발생→원 actual의 가능한 구간 전체가 `[잠정] <2초`라는 PLAYBACK 적법성 근거를 소비한다. exactly 2초는 적법 최초 시작으로 적용하지 않는다. Network `500ms` 제안과 min/AND로 결합하지 않는다. Started Session의 후속 cue마다 최초 gate를 다시 적용하지 않는다. R6에는 `time` 인자가 없으므로 현재 now를 새 인자로 덧붙이지 않는다.

<a id="late-final"></a>
## 늦은 결과·최종 후 충돌·보호 실패

UncertainFinal 또는 기존 whole final 뒤 새로운 정상 actual/final이 충돌하면 기존 final/replay를 유지하고 NOT_APPLIED다. 이미 반영된 동일 final의 재전달은 위 적용 증명이 있을 때만 ALREADY_APPLIED다. Expired 등 보호된 종결 이력을 새 Pending/Started로 되돌리지 않는다. 최종 후 old 결과는 원 old 진단/정리 문맥에 남고 새 Session/Attempt로 재귀속되지 않는다.

필요한 fact 보호 또는 적용 증명을 확보하지 못하면 상태 일부를 성공으로 공개하지 않는다. 기존 원본을 유지한 NOT_APPLIED와 부족 관측을 남긴다. PLAYBACK은 [APPLIED/ALREADY_APPLIED까지 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)하고 후속 기회에 적용/진단을 이어간다. timeout·최신 idle·mute·zero-fill·IRQ disable은 과거 무출력 증거가 아니다.

<a id="feasibility"></a>
## 구현 가능 slice와 제품 Gate

| 상태·근거 | 독립 Host 후보 / 정확한 제품 종속성 |
|---|---|
| F1_LOCKED_SEMANTIC / [R5 fact 적용](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#store-applyplaybackfact) | 같은 fact 재전달·키 동일 내용 충돌·미추적 occurrence·Started/final 독립 사실·final 후 late fixture. 기대 ALREADY는 증명 있는 경우만, 나머지 NOT_APPLIED/원 이력 보호. TEST_NOT_RUN |
| F2_PROVISIONAL_TESTABLE / [최초 시작 Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) | 원 actual 2초 미만/동일/경계 걸침과 원 발생 재전달 fixture. 시간 표현만 잠정, 물리 시각 충분성 승인 아님 |
| F3_PARTIAL_INDEPENDENT / [R4 async](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#late-results) | 추적/참조 보호 실패·cue scope·no-start 후 재확인 분기 검사. 실제 lower 증거 대신 **명시적으로 충분/부족인 typed fixture**를 사용하며 실제 출력 근거를 생성했다고 주장하지 않음 |
| F4_EXTERNAL_GATE / [Identity TBD](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#identity) | 제품 identity·reboot/wrap·bounded 추적/복사·실제 API 연결만 미확정. 내부 멱등/거부 논리 전체 HOLD 아님 |
| F5_PHYSICAL_SAFETY_GATE / [HW TBD](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#hardware) | PLAYBACK이 소비하는 실제 fresh actual·과거 no-output coverage·전체 종료/차단 충분성의 물리 증명만 미확정. STORE가 raw 사실을 대체 판정하지 않음 |
| F6_DEFERRED_SCOPE / [Mapping](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md) | 펌웨어/Host/보드 실행은 별도 범위. 이 Core는 WINDOW 제품 연결을 추가하지 않음 |

보관 C/H 범위에 `Store_ApplyPlaybackFact` 심볼은 미확인이며, 보관 Session의 단일 pending request 경계에 대한 정적 관측만 SOURCE_CONFIRMED다. 이 구현이 최종 fact/ledger 계약을 충족한다는 증거는 아니다. BINDING_TBD/HW_UNVERIFIED를 유지한다. 실제 채택 때 기존 STORE owner의 identity/보호 adapter만 최소 대응하고, 후보 부적합은 그 대응부를 재검토한다. 동결 의미 근거는 LOGICAL_APPROVED이며 그 의미 변경은 별도 CR이다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [R3 fact](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) · [R3 record](../../../../design/40_DATA/20_STORE_DATA.md#vssoccurrencerecord) · [R4 async](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md) · [R5 통지](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyplaybackfact)
