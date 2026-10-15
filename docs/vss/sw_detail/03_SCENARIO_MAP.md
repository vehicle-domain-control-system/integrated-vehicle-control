# VSS 시나리오 지도 — 기존 6 Flow와 원 사실의 연결

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[처음 읽기](00_START_HERE.md) · [VSS 지도](01_VSS_MAP.md) · [양단 인터페이스·18 Core 색인](02_INTERFACE_MAP.md#core-call-return)

이 지도는 기존 R5-C1의 6 실행 Flow를 입력 조건·호출/결과·상태 writer·예외·종료와 연결한다. **GUIDE / LOGICAL_APPROVED**인 문서상 연결이며 실행 로그·새 정책·추가 Core·실제 C call graph가 아니다. 기준 `VSS@d8a2220ffc1e9c701bbec505e5b101790d3bfc50`, 기존 5 Layer/10 Module/18 Core·49 Trace·Fault 5종·PCM A/B를 보존한다. 양단 IF는 PROPOSED_IF/BINDING_TBD, 실제 C ABI·RTD/SAI/DMA·STOP/actual·source/image·복구 효과는 HW_UNVERIFIED다.

동기 수용/반환과 이후 actual·소비·반환·복구 사실을 구분한다. 각 OUT·원 fact·참조 수명은 독립이며 현재 now·최신 Attempt·링크 순번으로 옛 사실을 다시 라벨링하지 않는다. 논리 순서는 원 R5·R6를 읽는 안내이고 물리 Task/Queue·용량·반복 횟수를 확정하지 않는다.

<a id="flow10"></a>
## 10_STARTUP_FLOW — 미평가·장치 진행·최초 보고

| 확인 항목 | 원 경계와 연결 |
|---|---|
| 입력·진입 | INITIAL 처리 기회·원 시간, 이미 수용한 초기 제어·원 device/configuration/operation. Session/Attempt 없는 startup은 정상이며 최초 HW 준비 entry·순서는 TBD |
| 기존 Trace | `VSS-TR-RUN-INIT`, `VSS-TR-STATUS-CAPABILITY`, `VSS-TR-STATUS-COMPOSE`, `VSS-TR-DIAG-FAULT` |
| CALL / RETURN | `Flow_Process` → `AudioStream_Advance` → 내부 `AudioControl_Service` 후속. FLOW는 CONTROL을 직접 호출하지 않음. 원 장치/진단 반영 → `Health_Evaluate` → 보고용 관측 재수집 → `Health_BuildStatus`. CREATED만 FLOW→INPUT 통신 연결로 전달 |
| authoritative state / 동기·후속 | CONTROL만 Codec/Generator/Reference 각각 현재 readiness writer. HEALTH만 Fault/permission writer; BuildStatus는 읽기 전용 파생. STREAM/CONTROL void·FLOW SETTLED가 모든 장치 READY/HW 성공을 의미하지 않음 |
| 경합·실패·끝 | 반복 INITIAL로 같은 HW 준비 재발행 금지. 미관측 UNASSESSED는 READY/FAILED/INITIALIZATION_FAILURE가 아님. 옛 구성 성공은 old 정리·진단만. 보고 관측/필수 reduction 부족은 REEVALUATE/written0과 후속 |
| 승인 원문·함수 연결 | [R5 startup](../design/60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup) · [FLOW startup](01_APP/FLOW/functions/Flow_Process.md#startup-recovery) · [CONTROL 장치별 준비](03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#devices) · [HEALTH 초기 상태](02_SERVICE/HEALTH/functions/Health_BuildStatus.md#states) |
| 독립 구현 후보 | 미평가/확인 실패 구분, 같은 초기 operation 중복 방지, old 구성 격리·coherent 보고/보호수명의 Software-only·Host-only fixture. 실제 실행은 별도 승인 |
| 종속 HOLD | BSP/초기 control entry·실제 readiness 충분조건·순서/timeout·source/image/ABI·Status 초기/전체 reduction. 미정만으로 독립 논리 전체를 보류하지 않음 |

<a id="flow20"></a>
## 20_INPUT_TO_SELECTION — 검증·원 사실 반영·현재 판단

| 확인 항목 | 원 경계와 연결 |
|---|---|
| 입력·진입 | RX 또는 RX 없는 INPUT_OR_FOLLOW_UP/TIME_CHECK, 원 Event/State/Quality·source generation·ordering·Meta·age. Network local 전달은 Domain 원 의미·기능 수용의 증거와 별개 |
| 기존 Trace | `VSS-TR-BOUND-NETWORK`, `VSS-TR-IN-RECEIVED`, `VSS-TR-IN-VALID`, `VSS-TR-IN-ACCEPTED`, `VSS-TR-IN-SOURCE`, `VSS-TR-IN-TEST`, `VSS-TR-IN-DUPLICATE`, `VSS-TR-OS-AGE`, `VSS-TR-OS-PENDING`, `VSS-TR-ST-WARNING`, `VSS-TR-ST-CLEAR`, `VSS-TR-ST-DISABLED`, `VSS-TR-ST-HOLD`, `VSS-TR-ST-REAR`, `VSS-TR-ST-WINDOW`, `VSS-TR-SEL-POLICY`, `VSS-TR-SEL-SNAPSHOT`, `VSS-TR-SEL-DECISION`, `VSS-TR-SEL-PREEMPT`, `VSS-TR-DIAG-INPUT`, `VSS-TR-RUN-DEADLINE` |
| CALL / RETURN | FLOW → `Input_Process`(내부 `Store_ApplyInput`) → `AudioStream_Advance` → `Playback_Advance`(원 fact→`Store_ApplyPlaybackFact`) → 최신 단일 PB 관측 → `Store_AdvanceDeadlines` → `Health_Evaluate`/현재 복구 조율 → coherent 관측 → `Select_Choose` → FLOW → `Playback_RequestTransition`. 즉시 원 결과 후속·보고용 별도 basis 재수집은 원 R5를 따름 |
| authoritative state / 동기·후속 | INPUT 문맥 반영과 STORE 개별 수용은 독립. STORE는 Occurrence/Stateful/Hold/Rear writer, POLICY는 read-only 판단, PLAYBACK은 채택/실행 owner, FLOW는 조율만. CHOOSE ≠ ADOPTED ≠ PREPARED ≠ actual |
| 경합·실패·끝 | STORE 거부에도 INPUT contextApplied를 rollback하지 않음. 동일 발생 재전달은 새 발생이 아님. 원 fact/통지 미반영 또는 startKnowledge pending/unknown이면 age만으로 Expired 없음. owner 적용·시간/인과 부족/변경은 재수집. INVALID/STALE/SNA/누락은 CLEAR가 아니며 Rear 활성과 위험 근거를 별도로 검사 |
| 승인 원문·함수 연결 | [R5 정규 기회](../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) · [INPUT 품질](02_SERVICE/INPUT/functions/Input_Process.md#quality) · [STORE 수용](02_SERVICE/STORE/functions/Store_ApplyInput.md#admission) · [STORE 기한](02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md#ownership) · [POLICY 판단](02_SERVICE/POLICY/functions/Select_Choose.md#return) · [양단 입력](02_INTERFACE_MAP.md#ingress) |
| 독립 구현 후보 | 배타 typed 입력·원 identity/age·중복/replay·품질/Hold 기산·Rear 결합·coherence·원 사실/expiry 순서. `<L`은 A1 선택값이고 Host fixture도 원 시간 문맥을 구분 |
| 종속 HOLD | 실제 producer/변환/복사·Wake/기동/wrap·양단 Codec/Meta·후방 활성 format·입력별 시간 실제 바인딩·직렬화/원 근거·제품 TEST/WINDOW 연결. Q01~07/Q11~14의 A1 결정과 외부 조건은 92_NETWORK_BINDING에 구분 |

<a id="flow30"></a>
## 30_PLAYBACK_FLOW — 채택·최초 actual·정상 후속 cue

| 확인 항목 | 원 경계와 연결 |
|---|---|
| 입력·진입 | 원 선택 decision 또는 원 Session의 종료 의도, 현재 owner 관측/basis·조건·원 preparation/request/output/cleanup. plan 참조와 원 Session/Attempt/position 보호 |
| 기존 Trace | `VSS-TR-PLAY-IDENTITY`, `VSS-TR-PLAY-OWNER`, `VSS-TR-PLAY-CUES`, `VSS-TR-PLAY-QUARANTINE`, `VSS-TR-BE-PREPARED`, `VSS-TR-BE-ACCEPTED`, `VSS-TR-BE-START`, `VSS-TR-OS-STARTED`, `VSS-TR-OS-FINAL` |
| CALL / RETURN | FLOW → `Playback_RequestTransition` → `AudioStream_Prepare` 또는 원 playback lane `AudioStream_RequestControl`. 원 결과 → `Playback_Advance` → 필요한 STREAM 후속·`Store_ApplyPlaybackFact`. ADOPTED는 PB 의도 반영, requests/outputs/facts/진단은 별도 원 결과 |
| authoritative state / 동기·후속 | PLAYBACK만 단일 전체 Session·Attempt·plan position·start/cue/repeat 권한·actual 지식·최종/통지 의무 writer. STORE는 원 Occurrence 이력. winner ≠ prepared ≠ accepted/enabled ≠ actual. actual과 acceptance가 역순 도착해도 이미 반영한 사실/종료를 되돌리지 않음 |
| 최초 actual 경계 | 원 output 충분성·원 occurredAt 구간·activation 인과·당시 권한을 대조. 구간 전체가 원 발생 이후이고 `A1 age < L` 및 원 권한 안이면 적법 최초 Started. 구간 전체가 밖이면 확정 출력 위반(정확히 L도 미허용). 비교 불가/경계 걸침이면 정상 Started와 확정 위반 모두 보류하며 확인된 물리 fact/coverage는 그대로 보호 |
| 정상 후속·실패·끝 | 이미 적법 Started된 같은 Session의 정상 다음 cue/반복 대기에는 최초 age gate 재적용 없음. cue gap은 owner 해제가 아님. Started One-shot은 선점·복구 뒤 자동 재실행하지 않음. EOF/lastForSegment/SEGMENT end는 whole final이 아니며 전체 종료는 Flow50의 범위/권한/late/자원 조건으로 확인 |
| 승인 원문·함수 연결 | [R5 재생](../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-advance) · [최초 actual 세 갈래](../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#first-actual-gate) · [PB 채택](02_SERVICE/PLAYBACK/functions/Playback_RequestTransition.md#return) · [PB actual](02_SERVICE/PLAYBACK/functions/Playback_Advance.md#actual) · [원 시간](../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) |
| 독립 구현 후보 | 원 identity·plan/permission 재확인·동기/후속 결과 분리·최초 구간 비교·정상 후속·반영 멱등/통지 보호. 잠정 경계 fixture는 제품 시간 승인과 구분 |
| 종속 HOLD | 원 actual 시각/activation·당시 권한 물리 근거·epoch 변환, USE_LIMIT 실제 생산·시간 입증, source/image/음원·실제 ABI·uncertain 최종화 정책/TBD-FINAL. now/capturedAt로 원 발생이나 actual 대체 없음 |

<a id="flow40"></a>
## 40_AUDIO_PREPARE_TX_FLOW — bounded 음원·PCM A/B·원 raw

| 확인 항목 | 원 경계와 연결 |
|---|---|
| 입력·진입 | 원 preparation/session/attempt/plan·asset/image·firstStartMeta, 장치별 현재 readiness, 기존 A/B의 실제 쓰기 가능 범위·원 operation/cycle. 압축 span과 PCM 전송 자료는 별개 |
| 기존 Trace | `VSS-TR-ASSET-LOOKUP`, `VSS-TR-ASSET-BYTES`, `VSS-TR-AUD-DECODE`, `VSS-TR-AUD-PCMOWNER`, `VSS-TR-AUD-REFILL`, `VSS-TR-AUD-TRANSPORT`, `VSS-TR-AUD-CONTROL`, `VSS-TR-BE-PREPARED`, `VSS-TR-BE-ACCEPTED`, `VSS-TR-BE-START`, `VSS-TR-RUN-ISR`, `VSS-TR-BIND-LOWER`, `VSS-TR-PER-MEASURE`(관측 경계 연결만; 실측 아님) |
| CALL / RETURN | PB → `AudioStream_Prepare` → `Asset_Read`·`AudioControl_Service`·필요 `AudioTx_Request` PCM lane. 원 START는 `AudioStream_RequestControl` → 주소 없는 TX control lane. HAL `AudioHAL_Callback` raw는 보호 경계에 포착; `AudioTx_Advance`/CONTROL 후속 → `AudioStream_Advance` → PB 원 출력 근거로 전달. FillBuffer/decoder는 STREAM 내부 local step이며 Core19/새 API가 아님 |
| authoritative state / 동기·후속 | ASSET은 bounded 압축 bytes/span 참조, STREAM은 provider·PCM A/B 내용/usage·생산권한, TX는 원 operation/장치 retain, CONTROL은 device/config readiness, HAL은 불변 원등록/raw writer. PB만 전체 actual/Session 판단. 각 즉시 disposition/OUT/후속은 독립 |
| PCM 사용권·수명 | 정확히 A/B 두 전송 버퍼. handoff 전에 HANDOFF_PENDING으로 samples/key/범위 보호. source consumed ≠ safe return ≠ 현재 CPU write permission ≠ actual end. safe return 뒤에도 차기 DMA 방문 전 bounded write·cache/barrier·원 회차/현재 안전 조건 재확인 |
| 유효 sound / zero-fill | validFrameCount=0은 새 sound 인계 없음이며 지속 DMA 정지를 의미하지 않음. 실제 decode된 0 sample은 유효 sound. tail/zero-fill은 sound 길이/반복을 늘리지 않고 실제 안전이 확인된 기존 A/B 범위에서만 처리. STOP 뒤 생산 차단 유지 |
| 원등록·ISR·손실 | 효력 전에 보호한 원 등록과 raw의 occurredAt/capturedAt/coverage를 보존. 지속 vendor 등록과 개별 operation/cycle 수명은 1:1 아님. 부족은 INSUFFICIENT, 실제 손실은 LOST. current Attempt/버퍼주소/IRQ phase로 원키 합성 없음. ISR은 짧은 포착/acknowledge/알림뿐이며 decode/refill/정책·복구·Core 역호출 없음 |
| 실패·끝 | 새 무효력 거부와 operation 생성 뒤 부분 효력/EFFECT_UNCERTAIN·기존 접근을 구분. retained refs/후속은 유지. HALF/MAJOR/WSF/EOF·abort·mute·zero-fill은 actual/no-start/whole 종료 충분조건 아님. 압축 refs 종료·PCM 반환·출력 retirement는 별도 |
| 승인 원문·함수 연결 | [R5 준비/인계](../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare) · [내부 fill/유효 PCM](../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) · [HAL 포착](04_HAL_BSP/AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#capture) · [PCM 원 회차 적용](02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md#pcm-return) · [R4 현재 쓰기 안전](../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [원 producer 측정 Boundary](../design/10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md#measurement) |
| 독립 구현 후보 | bounded read·배타 PCM/control·원 operation/cycle·독립 OUT bounds·consumed/return/현재 쓰기 조건 분리·STOP 선차단·손실/late 귀속 fixture. 실제 DMA 동작을 mock 성공으로 표시하지 않음 |
| 종속 HOLD | 실제 image/decoder format·등록/vendor ABI·지속 raw↔operation/PCM 상관·RTD·DMA 방문/WCET/cache/barrier·SAI/Codec 출력·STOP 충분성·실물 측정과 원 시간 확보. R6-03~07 OPEN 유지 |

<a id="flow50"></a>
## 50_STOP_LATE_UNCERTAIN_FLOW — 전체 차단·원 범위 정리·late

| 확인 항목 | 원 경계와 연결 |
|---|---|
| 입력·진입 | 원 Session/Attempt의 종료·선점·고장·현재 조건 상실·정책 끝, 기존 원 operation/SEGMENT/ATTEMPT 범위·request/output/cleanup·원 fact. current key로 원 target 합성 금지 |
| 기존 Trace | `VSS-TR-BE-NOSTART`, `VSS-TR-BE-END`, `VSS-TR-BE-LATE`, `VSS-TR-OS-FINAL`, `VSS-TR-PLAY-QUARANTINE`, `VSS-TR-RUN-RESOURCES` |
| CALL / RETURN | PB 원 종료 의도 → `AudioStream_RequestControl` STOP → `AudioTx_Request` 원 target별 제어. `AudioTx_Advance` → `AudioStream_Advance` → `Playback_Advance`. PB 원 fact는 `Store_ApplyPlaybackFact` APPLIED/ALREADY_APPLIED까지 또는 독립 보호 추적으로 유지; `Store_AdvanceDeadlines`는 최신 사실 반영/통지 확인 뒤 진행 |
| authoritative state / 동기·후속 | STREAM은 하위 STOP 호출보다 먼저 원 Session 관련 미래 decode/refill/PCM 제출 차단. TX만 자기 원 operation/장치 보호 writer. PB는 전체 start/cue/repeat 차단·전체 retirement writer. STOP accepted/void·한 target 성공은 전체 출력 종료가 아님 |
| NO_START / 종료 충분성 | NO_START는 원 범위의 과거 무출력 **및** 옛 미래 출력 불가 + STREAM 미래 PCM 차단이 필요. 실제 termination은 실제/잔류 출력 종료 **및** 옛 미래 출력 불가 + PCM 차단. termination/quiescent/현재 무음은 과거 무출력 증명이 아님 |
| whole / 독립 회수 | 전체 관련 Attempt/구간을 빠짐없이 덮는 Backend 충분 근거·future PCM 차단·PB 전체 후속 권한 차단·late 원 귀속 및 새 owner의 실제 자원 안전이 모두 필요. SEGMENT 하나·control operation 완료·PCM safe return·압축 참조 종료·원등록 회수는 서로 다른 조건 |
| late / uncertain / 끝 | 최종 전 늦은 적법 actual은 최초 Started 사실/보고만 반영하고 STOPPING/ABORTING 유지. uncertain final/retired 뒤 late는 old 정리/진단만: 새 Session/PCM/ready/Fault·정상 이력 변경 없음. TBD-FINAL 조건을 임의 충족시켜 QUARANTINED 만들지 않음; 확정 uncertain final은 PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE·replay 금지 유지 |
| 승인 원문·함수 연결 | [R5 STOP 원 scope](../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) · [whole/late/retirement](../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late) · [PB 통지](../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification) · [STORE fact](02_SERVICE/STORE/functions/Store_ApplyPlaybackFact.md#late-final) · [R4 충분조건](../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) |
| 독립 구현 후보 | 원 target/identity/scope 대조·STOP 선차단·부분 결과 격리·late/final 멱등·통지 보호·부분 목록≠whole의 문서/Software-only fixture |
| 종속 HOLD | 실제 STOP target↔descriptor/FIFO/frame 상관·abort/drain 효과·past coverage·future fence·전체 scope 완전성·현재 DMA 안전·C ABI/원기록 보호/물리 자원 회수·최종화 조건. timeout/mute/zero-fill/IRQ 하나로 확정 종료 없음 |

<a id="flow60"></a>
## 60_FAULT_RECOVERY_FLOW — 현재 허용·수행·별도 효과 검증

| 확인 항목 | 원 경계와 연결 |
|---|---|
| 입력·진입 | 원 producer 진단·장치/PB/cleanup의 반영 완료 관측·현재 Fault instance/revision·대상/configuration/action·허용과 원 수행 operation. Session 없는 Recovery는 정상 typed 경로 |
| 기존 Trace | `VSS-TR-DIAG-FAULT`, `VSS-TR-DIAG-RECOVERY`, `VSS-TR-STATUS-CAPABILITY`, `VSS-TR-STATUS-COMPOSE`, `VSS-TR-RUN-RESOURCES`, `VSS-TR-PLAY-QUARANTINE` |
| CALL / RETURN | FLOW → `Health_Evaluate`(현재 제한/허용) → FLOW old retirement/실제 위험 참조 정리 → `AudioStream_RequestControl` Recovery lane → 기존 Backend/필요 `AudioControl_Service`. `AudioStream_Advance`의 performed·verified·diagnostics → FLOW → `Health_Evaluate`. 해제 뒤 최신 관측 → POLICY → PB 현재 재판단; 복구 성공으로 직접 START 없음 |
| authoritative state / 동기·후속 | HEALTH 허용 → FLOW 안전 연결 → Backend/Driver 실제 performed → **별도 verified** → HEALTH 현재 해당 제한 해제. 6필드 permission/faultInstance/faultRevision/target/configuration/action과 수행 operation을 모두 원 문맥에 연결. 요청 수용·ready·performed 성공 하나는 효과 검증/Fault clear가 아님 |
| Fault 기존 5종 | `SOUND_ASSET_UNAVAILABLE`, `PLAYBACK_START_FAILURE`, `AUDIO_OUTPUT_FAILURE`, `PLAYBACK_STATE_FAILURE`, `INITIALIZATION_FAILURE`. 미평가·입력 품질 상실·WINDOW 보류를 새 Fault로 만들지 않음. exact raw cause→종류/action/효과 충분조건은 기존 TBD |
| 경합·실패·끝 | 허용 철회/새 instance/구성/target 변경은 새 수행 전 재검사. 같은 원 수행 후속을 새 요청으로 재발행하지 않음. old 성공·검증은 old 정리/진단만. performed 또는 verified 한쪽 없음/불일치·손실은 현재 제한 유지. 현재 해당 한 대상 해제는 다른 Fault/최근 이력 삭제가 아님 |
| 복구 뒤 입력·재생 | 원 age/Hold/replay 보존. Started One-shot·uncertain final 부활 없음. 미시작 One-shot은 충분 NO_START·안전 retirement·기존 replay 조건과 원 freshness에서만 재평가 가능. Stateful은 현재 유효 또는 원 출처·시각·고정 Hold 내·Rear gate/fence 조건을 충족한 경고를 old retirement 뒤 재선택하여 새 Session 정책 시작점. 입력/통신 복구의 pre-Pending 폐기 제안과 출력 Fault 복구를 같은 scope로 합치지 않음 |
| 보고 | 보고용 basis 재수집 → `Health_BuildStatus` CREATED → FLOW→INPUT→Node Communication. REEVALUATE·Status 변환/송신/Domain 소비는 [양단 Status 경계](02_INTERFACE_MAP.md#status-path)와 같으며 Recovery 허용·수행 결과와 합치지 않음 |
| 승인 원문·함수 연결 | [R5 안전 연결](../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) · [Session 없는 STREAM 복구](../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#stream-recovery) · [수행/검증](../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#device-recovery) · [HEALTH 한 대상 해제](02_SERVICE/HEALTH/functions/Health_Evaluate.md#pair) · [기존 Fault 계약](../design/50_CONTRACTS/40_FAULT_RECOVERY.md#recovery-flow) |
| 독립 구현 후보 | 허용 현재성/철회·same operation 멱등·performed/verified pair·한 대상 제한/최근 기록 분리·coherent Status와 원 수명 보호의 Software-only/Host-only fixture |
| 종속 HOLD | 실제 action→Driver map·target/config effect 관측·검증 충분조건·물리 자원 안전·정책/timeout/retry·Status reduction/원 목록·실제 실패 sink/consumer. 새 retry matrix·Recovery manager·auto clear 없음; P2 HOLD 유지 |

<a id="trace49"></a>
## 기존 49 Trace의 문서 연결 색인

아래는 [기존 Trace Map](../design/00_OVERVIEW/02_VSS_TRACEABILITY_MAP.md)의 13개 묶음 표기를 기존 49 ID로 펼친 탐색 색인이다. Flow 연결은 각 원 계약의 확인 위치 안내이며 새 Trace 명세·개별 R7 검사 결과나 GD7 PASS가 아니다. 원 R7 개별 Summary는 미확보인 외부 참조15에 남고, `PER-MEASURE`도 측정할 Boundary의 위치만 연결한다.

| 기존 Trace ID | 원 계약을 읽을 Flow |
|---|---|
| `VSS-TR-BOUND-NETWORK`, `VSS-TR-IN-RECEIVED`, `VSS-TR-IN-VALID`, `VSS-TR-IN-ACCEPTED`, `VSS-TR-IN-SOURCE`, `VSS-TR-IN-TEST`, `VSS-TR-IN-DUPLICATE` | [Flow20](#flow20) |
| `VSS-TR-OS-AGE`, `VSS-TR-OS-PENDING`, `VSS-TR-OS-STARTED`, `VSS-TR-OS-FINAL` | [Flow20](#flow20) · [Flow30](#flow30) · [Flow50](#flow50) |
| `VSS-TR-ST-WARNING`, `VSS-TR-ST-CLEAR`, `VSS-TR-ST-DISABLED`, `VSS-TR-ST-HOLD`, `VSS-TR-ST-REAR`, `VSS-TR-ST-WINDOW` | [Flow20](#flow20) |
| `VSS-TR-SEL-POLICY`, `VSS-TR-SEL-SNAPSHOT`, `VSS-TR-SEL-DECISION`, `VSS-TR-SEL-PREEMPT` | [Flow20](#flow20) · [Flow30](#flow30) |
| `VSS-TR-PLAY-IDENTITY`, `VSS-TR-PLAY-OWNER`, `VSS-TR-PLAY-CUES`, `VSS-TR-PLAY-QUARANTINE` | [Flow30](#flow30) · [Flow50](#flow50) |
| `VSS-TR-BE-PREPARED`, `VSS-TR-BE-ACCEPTED`, `VSS-TR-BE-START`, `VSS-TR-BE-NOSTART`, `VSS-TR-BE-END`, `VSS-TR-BE-LATE` | [Flow30](#flow30) · [Flow40](#flow40) · [Flow50](#flow50) |
| `VSS-TR-ASSET-LOOKUP`, `VSS-TR-ASSET-BYTES` | [Flow40](#flow40) |
| `VSS-TR-AUD-DECODE`, `VSS-TR-AUD-PCMOWNER`, `VSS-TR-AUD-REFILL`, `VSS-TR-AUD-TRANSPORT`, `VSS-TR-AUD-CONTROL` | [Flow40](#flow40) · [Flow50](#flow50) |
| `VSS-TR-DIAG-INPUT`, `VSS-TR-DIAG-FAULT`, `VSS-TR-DIAG-RECOVERY` | [Flow20](#flow20) · [Flow60](#flow60) |
| `VSS-TR-STATUS-CAPABILITY`, `VSS-TR-STATUS-COMPOSE` | [Flow10](#flow10) · [Flow60](#flow60) |
| `VSS-TR-RUN-INIT`, `VSS-TR-RUN-DEADLINE`, `VSS-TR-RUN-ISR`, `VSS-TR-RUN-RESOURCES` | [Flow10](#flow10) · [Flow20](#flow20) · [Flow40](#flow40) · [Flow50](#flow50) · [Flow60](#flow60) |
| `VSS-TR-BIND-LOWER` | [Flow40](#flow40) |
| `VSS-TR-PER-MEASURE` | [Flow40](#flow40) |

<a id="limits"></a>
## 미결·확인 수준과 독립 가능한 범위

Q01~Q14·R6-01~08과 [인터페이스 미결](02_INTERFACE_MAP.md#external-open)은 OPEN이며 외부 누락 참조15건/7대상·원 G1-B 최종 결과 SOURCE_NOT_RECOVERED를 보존한다. 문서 추적 보정 PASS는 양단 채택·제품 source/ABI·물리 충분성의 해소가 아니다. WINDOW/Anti-Pinch **[구현 보류 — 설계 유지]**, TEST 제품 격리, P2 HOLD를 유지한다.

각 Flow의 독립 후보는 원 자료·owner·단계·원 시간/귀속·보호 실패 조건을 사용하는 정적/Software-only/Host-only 작업 후보다. 실제 코드·Host/Firmware/Build/Flash/보드 시험을 수행한 결과가 아니다. 실제 양단 결합·출력 성공·복구 효과·실측 성능의 확인은 해당 owner·source/채택 기록과 별도 시험이 필요하다. 미결의 영향을 받지 않는 논리까지 일괄 HOLD하거나 근거 없는 기본 정상값·새 정책으로 빈칸을 채우지 않는다.

## A1 현재 입력 기준과 이력

현재 입력 연결은 [92_NETWORK_BINDING](../design/90_BINDING/92_NETWORK_BINDING.md)과 A1으로 개정한 Data/계약/의사코드다. Network 고정 원문은 `70028972ec713a3ec6b5f0a419e0dc89fd963844`이다. 과거 SHA·Q01~Q14·PASS·P1B HOLD 기록은 당시 검토 이력이며 현재 미결 전체가 아니다. D01 WARNING60, D04 L, D05 RX Age, D06 Hold/활성 후보, D07 owner/용량, D08 복구 분리는 설계 반영했다. Q 관련 DOMAIN 판단 순서/생산·계보·Boot·실제 시간/ABI/HW는 E01~E05로 구체화했다. Status 관련 Q/D09~D11은 A2 미착수다.

## A2 Status 정상·오류·부분 실패 추적

[Status Binding](../design/90_BINDING/93_NETWORK_STATUS_BINDING.md)의 §2 STARTUP/READY/PLAYING/FAULT, §1 current 완전성·동일 kind 여러 instance·clear 후 이력, §3 REEVALUATE 번호/시각 불변, §5 submit 거부/후발 실패, §6 State 수용 뒤 Fault 실패를 ST01~ST12로 대조했다. A1-C1 WARNING60/Event L/Hold/Rear/복구는 ST13에서 보존 확인한다. 문서 분기 검수이며 Host/Board PASS가 아니다.
