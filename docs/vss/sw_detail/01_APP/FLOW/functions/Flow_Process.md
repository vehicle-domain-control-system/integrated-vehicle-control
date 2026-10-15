# Flow_Process — 처리 기회와 owner 반영 순서의 조율

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[VSS 지도](../../../01_VSS_MAP.md) · [FLOW](../00_MODULE_OVERVIEW.md) · CORE-01 · [한국어 의사코드](../pseudocode/Flow_Process.md)

> GUIDE · 동결 `0641b2c2268cf47450aeb50302638a45a7a3277f`. TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

기존 app 처리 기회 안에서 INPUT 검증, 비동기 원사실 적용, STORE 기한, HEALTH/복구, POLICY 선택, PLAYBACK 요구와 보고를 연결한다. FLOW는 각 owner의 공개 의미 경계를 조율하며 재생 성공·새 authority·상태 원본을 생성하지 않는다.

<a id="contract"></a>
## R6 원형과 반환

```text
VssFlowProgress Flow_Process(
    VALUE VssProcessingOpportunity opportunity,
    IN const VssTimeEvidence *time);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-flow-process)의 opportunity는 작은 enum **값**이다. [R2](../../../../design/30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)의 포인터 후보를 새 payload pointer로 복원하지 않는다. VALUE/IN은 논리 주석이며 실제 C ABI·enum 숫자·Header는 미정이다. 도착 입력은 기존 INPUT, 하위 사실은 STREAM 경계에서 읽는다. 새 RX/typed backend 묶음을 public 인자로 추가하지 않는다.

| 인자·반환 | 필요한 의미·수명·사후조건 |
|---|---|
| [VssProcessingOpportunity](../../../../design/40_DATA/10_INPUT_DATA.md#vssprocessingopportunity) | INITIAL / INPUT_OR_FOLLOW_UP / TIME_CHECK. 알림 자체는 actual/완료 사실이 아님 |
| [VssTimeEvidence](../../../../design/40_DATA/10_INPUT_DATA.md#vsstimeevidence) | 호출 동안 유효한 불변 borrow. NULL/무효 공간은 다른 Core에 전달하지 않고 재수집. 저장공간은 유효하나 원 시간 비교가 부족한 경우 독립 owner 사실 적용을 이어가되 expiry/START/정상 보고 확정 금지 |
| `VSS_FLOW_RECOLLECT_OBSERVATIONS` | 미반영·변경·시간 부족으로 선택/보고에 함께 적용 가능한 근거가 필요. stale 판단을 START/새 정상 보고로 채택하지 않음 |
| `VSS_FLOW_FOLLOW_UP_REQUIRED` | 적용/정리/미인계 진단·보고/재평가 또는 처리 예산 밖 일이 남음. 기존 후속 처리 기회로 연결 |
| `VSS_FLOW_SETTLED` | 이번 기회에 필요한 반영/연결을 완료. 전체 물리 출력 성공·미래 기회 중단·모든 reference 회수 아님 |

반환 타입은 [VssFlowProgress](../../../../design/40_DATA/10_INPUT_DATA.md#vssflowprogress)다. INIT accepted·winner·prepared·device enabled와 actual은 각 owner에서 별도로 관측한다.

<a id="ownership"></a>
## 호출·writer·자료 수명

Caller는 기존 app main/task 진입의 논리 경계다. Callee는 INPUT, STORE, POLICY, PLAYBACK, AUDIO STREAM, HEALTH 및 필요한 기존 ASSET/Runtime 읽기 경계다. DRIVER/HAL 직접 호출이나 IRQ 안의 상위 Core 실행은 없다. FLOW가 쓰는 것은 조율 단계·후속 필요성과 평가기간 [VssReadBasis](../../../../design/40_DATA/30_SELECTION_DATA.md#vssreadbasis)뿐이다. 다른 owner 원본과 PCM A/B usage는 read-only다.

typed 사실·진단·적용 결과가 동시에 존재하면 각각 보호/반영한다. 마지막 progress 하나로 STARTED/final/diagnostics를 덮지 않는다. plan/Meta/stamps/Status 내부 참조는 실제 소비자 수명까지 보호하며 선택 평가용 borrow를 비동기 채택 자료의 보호로 간주하지 않는다. 후속 필요성과 예산은 기존 실행 문맥에 남긴다. 새 Task/Queue/Manager/무제한 loop를 만들지 않는다.

<a id="order"></a>
## 정규 기회의 순서와 RX 없음

1. 기존 owner의 Network callback에서 Event1개/Warning최대4개를 동기 소비한 뒤, no-RX/기한 점검의 optional evidence를 읽어 [Input_Process](../../../02_SERVICE/INPUT/functions/Input_Process.md)를 연결한다. TIME_CHECK/RX 없음도 NULL로 품질 점검한다.
2. AUDIO STREAM의 보호된 typed 하위 사실을 진행하고 PLAYBACK이 최신 조건/원 시간으로 적용한다. 기존 통지 내부 단계의 [Store_ApplyPlaybackFact](../../../02_SERVICE/STORE/functions/Store_ApplyPlaybackFact.md)가 원 최초/최종 사실을 반영한다. 미통지 사실은 owner가 보호한다.
3. 이미 반영된 최신 singular PLAYBACK 관측을 수집해 [Store_AdvanceDeadlines](../../../02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md)를 호출한다. notificationPending/부족이면 사실 적용·재확인이 먼저이며 age만으로 만료하지 않는다.
4. INPUT/STREAM/PB가 실제 발견한 진단과 원 수행/별도 검증을 HEALTH에 적용한다. 현재 HEALTH 허용과 old 안전 종료를 [R5 복구 slice](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow)로 연결한다.
5. STORE 후보, PB, HEALTH 제한, ASSET/장치 가용·cleanup의 반영 완료 관측과 시간으로 선택 basis를 수집한다. 그 basis가 유효할 때만 POLICY 판단을 PB 요구로 연결한다.
6. 요구에서 즉시 발생한 결과도 같은 owner 적용 경계로 연결하고 **별도 보고 basis를 새로 수집**한다. HEALTH가 CREATED한 파생 Status만 기존 INPUT/Node Communication 경계에 보호·전달한다. 실제 CAN 송신/Domain 수용은 별도다.

<a id="coherence"></a>
## 관측 변경·원사실 부족·출력 경합

각 owner stamp의 owner/revision/appliedThrough와 필요한 인과 선행 사실을 대조한다. 전체 revision 숫자가 같을 필요는 없다. 수집 후 관련 원본이 변하면 READ_CHANGED → RECOLLECT, 미반영 사실·부족한 stamp/시간이면 READ_INCOMPLETE → RECOLLECT다. POLICY 평가와 PB 채택 전에도 최신 조건을 확인한다. 단순 getter에 숨은 적용을 추가하지 않는다.

old PB 통지가 pending이면 expiry나 old 이력 종료를 먼저 확정하지 않는다. 최신 PB 관측·STORE 후보가 함께 적용 가능하기 전 stale winner로 새 Session/START를 채택하지 않는다. POLICY 선택은 실행 권한이 아니고 PB ADOPTED는 STREAM prepare/actual 성공이 아니다. 실제 actual은 원 귀속·당시 권한·충분 coverage/시간을 PB가 판정한 사실이며 FLOW는 이를 합성하지 않는다. 부족한 선택/보고 때문에 이미 안전하게 반영한 INPUT/STORE 사실을 rollback하지 않는다.

<a id="startup-recovery"></a>
## Startup과 복구 slice

INITIAL 기회는 기존 모듈 초기화/자원 근거에서 중립 ASSET 가용·Session 없는 STREAM 장치 진행을 연결한다. 아직 검사하지 않은 UNASSESSED를 실패/Fault로 만들지 않는다. 초기화/후속 호출을 반복해 이중 prepare/Session을 생성하지 않는다. 공개 INIT API나 Task를 새로 정하지 않고 [동결 startup](../../../../design/60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup)을 따른다.

복구는 현재 Health permission의 instance/revision/target/configuration/action/basis를 확인한다. old 실제 위험 참조가 남으면 PB 종료·STREAM/하위 cleanup을 기존 경계로 계속 진행하고 후속을 남긴다. 독립 보호된 old HAL 불변 기록이 남아 있다는 이유만으로 안전한 독립 논리 전체를 HOLD하지 않는다. 실제 대상/접근 위험의 종료와 record 저장 수명은 다르다.

새 복구 효력 직전 permission/최신 조건을 재검사하여 STREAM의 기존 Recovery lane으로 연결한다. 실제 Performed와 별도 Verification을 HEALTH에 전달하고 HEALTH만 해당 현재 제한을 해제한다. FLOW가 수행 수용·복구 요청·현재 READY 하나로 Fault를 지우지 않는다. 실제 HW/IF 미정은 그 제품 호출/물리 검증 Gate에만 남긴다.

<a id="feasibility"></a>
## 구현 가능 slice와 Gate

| 상태·근거 | 후속 독립 Host 입력/기대 또는 정확한 종속성 |
|---|---|
| F1_LOCKED_SEMANTIC / [R5 정규 순서](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) | RX 없음·old 통지 pending·revision 수집 도중 변경·선택/보고 basis 변경 fixture. 호출 순서·RECOLLECT·새 START/정상 보고 금지·원 반영 보호 검사. TEST_NOT_RUN |
| F2_PROVISIONAL_TESTABLE / [시간](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) | 명시 가정 처리 예산·시간 구간 경계 fixture. 실제 처리 주기/WCET/시간 ABI 확정 아님 |
| F3_PARTIAL_INDEPENDENT / [복구](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) | permission 변경·old 위험 참조/독립 old 기록·진단/보고 공간 부족·동시 facts 검사. 실행 물리 증거 대신 owner 충분/부족 관측을 명시 주입 |
| F4_EXTERNAL_GATE / [실행 Binding](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#execution) | 기존 app entry/직렬화/처리 예산 및 Node Communication Status 소비자 실제 연결만 미확정 |
| F5_PHYSICAL_SAFETY_GATE / [HW TBD](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#hardware) | old device/provider/PCM 접근 종료, 실제 복구 수행/검증·actual/whole 종료 충분성의 물리 확인만 미확정. FLOW의 순서/재수집 논리는 작성 가능 |
| F6_DEFERRED_SCOPE / [Mapping](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md) | 실제 구현/Host/Build/Flash/보드·WINDOW 제품 연결은 별도 범위 |

보관 코드의 VSS_DEMO에서 Session_Process 후 Arbitrator_Process를 호출하는 순서와 single-pending slot/overwrite counter에 대한 정적 관측만 SOURCE_CONFIRMED이며 최종 FLOW 순서·통지/Status 계약을 구현했다는 의미는 아니다. 논리 `Flow_Process` 심볼은 보관 C/H에서 미확인, BINDING_TBD/HW_UNVERIFIED다. 동결 의미 근거는 LOGICAL_APPROVED다. 실제 채택 시 기존 entry/owner 읽기 adapter만 최소 대응하고 부적합 후보는 그 adapter·예산 가정만 재검토한다. 동결 R2~R6/D2 의미 변경은 별도 CR이다.

[R2](../../../../design/30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [R3 basis](../../../../design/40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [R4 Ownership](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R5 순서](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-flow-process)

## A1 owner 적용 순서

기존 FLOW owner에서 Network dispatcher callback을 동기 소비하여 Event1 또는 Warning최대4 입력을 처리한다. INPUT 변환→각 Store_ApplyInput→bundle 끝→하위 fact→Store_AdvanceDeadlines→HEALTH→한 번의 POLICY/PB 순서다. 별도 Queue/Task를 추가하지 않는다. TIME_CHECK는 10ms 이하 설계 예산(실측 미검증)이며 Input_Process(NULL)이 최대18 문맥을 종류별로 점검한다. 주기 Warning은 원 TTL/정상 증가 RX+150ms, 비주기 Event는 원 L과 별도 승인된 DOMAIN 감독·품질/세대/시간 근거를 사용하며 Event 무발생 timeout을 만들지 않는다. INPUT contextApplied와 STORE admission/진단은 개별 관측이다.

QUALITY Event 상실은 replayAllowed=false; Stateful 최초 loss/Hold는 고정; Rear DISABLED/상실은 riskFence 보존·gate 차단이다. 신규 Event는 원 key/Meta/L/추적을 함께 예약하며 ledger128 full이면 REJECTED다. 현재 유효 또는 원 출처·시각·고정 Hold·Rear gate/fence가 입증된 Stateful은 old retirement 뒤 새 Session 재선택이 가능하다. Hold 경계 걸침/비교 부족은 UNCERTAIN·새 시작 차단·보수적 종료 요청이며 확정 만료가 아니다. 세부 분기와 Data는 [A1 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md)을 따른다.

<a id="a2-status-handoff"></a>
## A2 Status 인계·실패 후속

[Status Binding §5](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#handoff)를 적용한다. CREATED만 기존 INPUT 통신 연결 한 곳에서 NetVssStatus로 변환한다. reportGeneration/reportSequence/evaluatedAt과 last Fault 원근거를 보존하고 Network 동기 전체 복사 반환까지 불변 참조를 보호한다. INVALID는 원 진단/재수집, NOT_READY는 준비 변화, NO_CAPACITY는 다음 기존 처리 기회에서 최대1회 재검토다. 아직 미접수 1건만 더 새 coherent 평가로 교체할 수 있으며 accepted/Driver 사본은 수정하지 않는다.

localOK 뒤 terminal 실패는 원 key·BOOT/TX·raw·참조 종료 여부를 Network 기존 관측 → INPUT 진단16/loss latch → FLOW로 bounded drain한다. 이 처리는 정규 F3에서 RX 유무·새 Snapshot CREATED와 독립적으로 진행한다. 뒤의 선택/보고 basis 부족으로 재수집 반환하더라도 이미 보호한 실패 관측을 잃지 않으며 공간 부족·미방문은 원 owner 보호와 후속 의무로 남긴다. sink 미준비는 접수 NOT_READY다. 실패를 CAN 수신/DOMAIN 성공이나 음향 Fault로 바꾸지 않는다. 미인계/실패 관측/재수집이 남으면 FOLLOW_UP_REQUIRED/RECOLLECT이며 보고 문제로 독립 입력·정리 처리를 중단하지 않는다. 실제 함수/게시/sink 바인딩은 E-S01~04이고 신규 Core/제품 코드는 없다.
