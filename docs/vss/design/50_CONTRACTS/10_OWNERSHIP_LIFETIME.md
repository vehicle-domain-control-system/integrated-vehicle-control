# Ownership / Lifetime — 원본 소유권과 서로 다른 수명

> R4 · 2026-10-08 · R3-C1 및 잔여 함수 독립 국소 PASS 기준 · 논리 Contract

[Contract Overview](00_CONTRACT_OVERVIEW.md#ownership)

여러 owner가 같은 실행에 참여해도 상태 원본의 writer와 자료의 보호 수명을 합치지 않는다. 이 계약은 기존 소유권을 연결하는 기준이며 새 Context·관리자·저장 구조를 만들지 않는다. Module별 책임·예외는 [Module Overview](../20_MODULES/00_MODULE_OVERVIEW.md)와 각 상세에 유지한다.

<a id="single-writer"></a>
## 1. 원본은 해당 owner 한 곳에서만 변경한다

요청자는 의도를 전달하고 callee가 자기 원본을 변경한다. 결과 producer, 그 결과를 반영하는 owner, 읽기 관측의 소비자는 서로 다른 역할이다. FLOW의 조율·POLICY의 판단·HEALTH의 파생 보고가 STORE/PLAYBACK/DRIVER 원본의 두 번째 writer가 되지 않는다. HAL의 raw 포착과 Runtime의 시간·알림도 상위 상태 변경 권한이 아니다.

같은 자료를 읽는다고 소유권이 이전되지 않는다. 준비/요청 수용·장치 사실·전체 재생 사실은 해당 owner의 공개 경계에서 반영한다. 읽기 함수에서 미반영 결과를 숨겨 적용하지 않으며, 반영 완료 관측을 수집하는 규칙은 [Timing §5](30_TIMING_FRESHNESS.md#coherent-read)를 따른다.

<a id="identity-lifetimes"></a>
## 2. 실행에 연결된 identity와 수명을 구별한다

| 보호 대상 | 기존 writer / 보호 범위 | 끝내거나 회수할 때 필요한 근거 |
| --- | --- | --- |
| One-shot Occurrence | STORE. 원본 identity·Meta·최초 시작·전체 최종·replay | 모든 참조/미반영 통지 종료와 재전달·replay 방어. Session retirement로 ledger 삭제 불가 |
| Stateful 현재 상태 | STORE. 마지막 유효 판단·현재 품질·Hold·Rear 결합 | 해당 상태/품질의 후속 판단·참조 종료. One-shot ledger로 합치지 않음 |
| Session | PLAYBACK. 전체 정책, cue 사이 무음·반복 대기, 후속 권한 | 전체 출력 종료/차단·상위 권한 차단·late 귀속 보호 뒤 출력 owner 해제 |
| Attempt | PLAYBACK. 실제 새 출력 시도·결과 지식·원래 Session/계획 구간 | 해당 출력 정리와 결과·통지 보호 의무. cue와 Attempt 개수의 1:1 고정 없음 |
| Provider / 압축 자료 참조 | 진행은 AUDIO STREAM, 제공 span의 참조 원본은 ASSET | 실제 decoder/read consumer가 더 읽지 않을 근거. 출력 retirement만으로 bytes 회수 불가 |
| PCM A/B 내용·usage | AUDIO STREAM. 정확한 Attempt/Buffer/사용 회차 | 해당 회차 반환과 실제 쓰기 안전 조건은 [Buffer §4](50_AUDIO_BUFFER_TRANSPORT.md#safe-write). TX의 장치 보호와 다른 원본 |
| 개별 TX operation | AUDIO TX. 사전 귀속·장치 접근/출력 가능성·결과 추적 | 해당 옛 작업의 미래 접근/출력 종료와 결과 추적 완료. vendor 지속 등록과 수명 1:1 아님 |
| HAL TX/device 귀속 기록 | HAL/BSP Layer의 HAL Boundary. 불변 요청 귀속·당시 binding | HW·대기·처리 중 소프트웨어 참조와 late 사실 대응 가능성의 독립 종료 |
| 장치 구성 readiness | AUDIO CONTROL. Codec/Generator/Reference 각각의 현재 구성 | 영향받는 reset/reconfig/오류로 무효화. control callback 저장 회수와 별개 |
| 현재 Fault / 복구 허용 | HEALTH. instance·revision·대상/구성·제한 또는 허용 범위 | 허용의 무효화와 현재 제한 해제는 [Fault Contract](40_FAULT_RECOVERY.md). 최근 기록 보존은 별도 |
| 읽기 관측 / 보고 Snapshot | 관측 원본은 각 owner, 임시 연결은 FLOW, 파생 보고는 HEALTH | 관측은 해당 평가 기간. 보고는 복사 완료 또는 실제 전송 consumer 참조 종료까지 |

Occurrence는 원래 발생, Session은 전체 정책 실행, Attempt는 새 실제 출력 시도다. 재전달·재평가·새 Attempt·복구로 Occurrence를 새로 만들거나 그 원본 age를 갱신하지 않는다. Stateful 선점 뒤 재선택과 Started One-shot 재실행의 차이는 [Timing §3~4](30_TIMING_FRESHNESS.md#first-start)에 둔다.

<a id="borrow-and-protect"></a>
## 3. 전달 인자와 실제 참조 자료의 보호 기간은 다르다

`const`/포인터는 읽기 방향을 나타내며 호출이 끝난 임시 구조체 주소를 장기 저장할 허가가 아니다. callee는 후속 처리에 필요한 불변 identity·시각·범위·자료를 원래 문맥에 보호한다. 실제 복사/borrow/참조 보존 수단과 C optional 표현은 R6/Binding TBD다.

Command 인자를 소비한 뒤에도 PCM samples, 압축 bytes, 계획/정책, 결과가 참조하는 근거는 각각 실제 consumer 수명 동안 유지한다. 새 요구 거부나 새 자료 제공은 기존 참조를 종료한 근거가 아니다. mutable 현재 상태 포인터를 불변 결과·보고의 원자료 대신 운반하지 않는다.

시작·전체 최종·소비·반환·진단처럼 함께 존재하는 사실을 마지막 Result/Progress 슬롯 하나로 덮지 않는다. producer는 consumer 반영 완료 또는 보호된 추적 종료까지 각 사실을 보존한다. PLAYBACK→STORE 통지는 `APPLIED`/`ALREADY_APPLIED`까지 의무를 유지하며, 신규 수용 공간 부족을 기존 사실 폐기의 이유로 쓰지 않는다. STORE는 필요한 정규화 fact를 ledger 수명 동안 보호하되 raw HAL 기록 전체를 replay 기간까지 강제로 붙잡지 않는다.

<a id="references-and-release"></a>
## 4. 출력 owner 해제와 자료 폐기는 별도 판단이다

Session retirement, PCM usage 반환, provider/read 종료, HAL 귀속 기록 폐기, Fault 해제, Occurrence 이력 회수는 서로를 자동으로 성립시키지 않는다. IRQ disable·장치 정지·반환 API 호출도 HW·대기·처리 중 참조의 종료를 대신하지 않는다.

옛 출력 종료/차단과 해당 자원의 안전 종료, alias 없는 독립 귀속 보호 공간이 충족되면 옛 불변 callback 기록을 남긴 채 새 provider 또는 같은 A/B 주소의 새 회차를 사용할 수 있다. PCM 주소의 재사용에는 현재 CPU 쓰기 안전 조건도 충족해야 한다. 옛 기록과 새 context를 alias시키지 않으며 당시 binding/build 문맥을 새 구성으로 덮지 않는다. 이 조건은 모든 자원을 Session 끝까지 묶거나, 모든 기록을 동시에 폐기하라는 뜻이 아니다.

보호 공간 부족은 새 수용/진행 제한으로 다룬다. 기존 accepted/final·미반영 사실·옛 callback 기록을 임의 eviction하거나 무제한 pool로 해결하지 않는다. 보호 용량·회수/재부팅·key wrap/alias 방지와 실행 직렬화 수단은 기존 TBD다.

<a id="late-attribution"></a>
## 5. 늦은 결과는 원래 문맥에만 귀속한다

요청이 효력을 가질 수 있는 경계 **전에** 원래 identity와 필요한 참조 보호를 확보한다. callback 도착 때 현재 Session/Attempt/Buffer를 읽어 사후 라벨을 붙이지 않는다. 지속 vendor 등록만으로 개별 Attempt/PCM 회차를 추정하지 않는 규칙은 [Buffer §6](50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream), 최종 전/뒤 적용 범위는 [Async §5](20_ASYNC_RESULT_EVIDENCE.md#late-results)를 따른다.

복구의 원래 identity는 Fault instance/revision/target/config/action·permission이다. Session 없이도 성립하며 가짜 Session/Attempt를 만들지 않는다. 장치 제어는 원래 device/config에만 적용하고 옛 성공으로 새 readiness나 새 Fault를 정상화하지 않는다.

## 6. 연결 원본과 미정

[Occurrence/Stateful](../40_DATA/20_STORE_DATA.md#store) · [읽기 근거](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [Session/Attempt/Fact](../40_DATA/40_PLAYBACK_DATA.md#playback) · [ASSET/provider](../40_DATA/50_AUDIO_STREAM_DATA.md#asset) · [TX/HAL](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [Fault/보고](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [HAL Boundary](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime)

실제 storage·참조/복사·보호 예산·callback 귀속/물리 안전 보장·메모리 조건은 [Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md). 새 Header/ABI·Task/Queue·자원 수치를 이 계약에서 확정하지 않는다.
