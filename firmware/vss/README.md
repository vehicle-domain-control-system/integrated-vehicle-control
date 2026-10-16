# VSS P1-A/C1 · P1-B · P2/P3 Host 통합 후보

## P2P3-C1 · 2026-10-16

G3-C1-001~003 국소 수정의 **자체검수 완료 / 독립 검수 대기** 상태다.
P2 독립 PASS를 선언하지 않으며 P3 PARTIAL/실구동 HOLD를 유지한다.

- POLICY는 전체 최대 우선순위를 찾은 뒤 그 선택에 필요한 동률만 검증한다.
  잠정 최소 원 시각은 비교 후보 선정용이며, 최종 원 순서 증명 없이 CHOOSE하지 않는다.
  낮은 순위의 겹친 발생 구간은 높은 긴급 경고를 가리지 않는다.
- PB는 보호된 Attempt를 원 계획의 cue/반복 위치에 연결하고 적법 actual이 확인된
  위치만 이행으로 기록한다. NO_START인 필수 위치를 건너뛰거나 전체 정상 완료로
  처리하지 않는다. 안전 종료 후 기존 FIRST는 보존하고 해당 중단 의미를 적용한다.
  위치별 이행과 Attempt 수는 별개이며 새 재시도/무음 완료 정책은 추가하지 않았다.
- 최초 차단 발생 여부와 비교 가능한 차단 시각을 분리한다. 시각 미확인 STOP도
  즉시 차단하되, 뒤의 STOP 시각으로 최초 경계를 바꾸지 않는다. 입증되지 않은
  late actual은 물리 근거/불확실 진단으로 보존하고 legal FIRST를 만들지 않는다.

기존 PB/FLOW 12그룹·705검사를 유지하고 같은 suite에 C1 5그룹·1,461검사를 추가했다.
전체 GCC O0/O2는 각 72그룹·132,111검사 PASS(그중 실제 음원 decode 125,913검사)다.
별도 검수 제공 세 반례는 수정 전 모두 FAIL, 수정 후 모두 PASS다.
Clang/ASan/UBSan은 이번 실행 환경에서 미가용으로 NOT_RUN이며 target/보드 결과가 아니다.
INPUT/Warning, 오디오 reference/Provider/decoder, 제품 구동 gate 및 header는 변경하지 않았다.

`src/`는 기존 One-shot/Warning INPUT·STORE에 POLICY, 단일 Session PLAYBACK,
FLOW 순서, 안전 차단 오디오 어댑터를 연결한 C99 후보다. P1-B 독립 검수 PASS의
소스와 시험을 기준으로 확장했다. `audio_reference/`에는 사용자 제공 오디오 ZIP의
실제 음원·minimp3·provider 분리 사본과 유효 tail 길이 보강이 있다.
물리 경로는 HOLD다. Network, HEALTH/Status(P4), 타깃 main/Flash/보드 시험은 미연결이다.

## 실행

저장소 루트에서 Python 3와 GCC를 사용한다. 결과는 기본적으로 Git에서 제외된
`firmware/vss/build/host/`에 저장된다. 다른 위치는 `--out`으로 지정한다.

```powershell
python firmware/vss/tests/run_host_tests.py
python firmware/vss/audio_reference/run_host_decode.py --out firmware/vss/build/audio_decode
```

Runner는 C99 엄격 경고 빌드, 테스트 매크로 없는 제품 object 빌드, O0/O2 실행,
ASan/UBSan 실제 링크 가능 여부, PATH의 Clang 존재 여부를 기록한다.
sanitizer probe 실패는 시험 PASS가 아닌 NOT_RUN이다.

## P2/P3 구현 경계

- POLICY는 Emergency: Anti-Pinch > Rear Emergency > Occupant, Caution:
  Rear Caution > Lock Error, Feedback: Unlock > Lock > Goodbye > Welcome 순서다.
  같은 의미 Event는 원 발생 시각/고정 key로 비교하고 시간 구간이 겹치면 재수집한다.
  미정 계획이나 상위 후보의 자원 부족을 하위 소리로 대체하지 않는다.
- 제품 cue 수·간격·반복·음량·의미별 asset 연결은 미정이다. 제품 FLOW catalog는
  비어 있고 startsAllowed=false다. Host fixture만 명시적 유한 계획을 제공한다.
  4 cue/8 repeat 타입 상한과 실제 Session 8 Attempt 상한은 구현 자원 한계이며
  제품 패턴값이 아니다. 총 Attempt가 8을 넘는 계획은 효력 전 WAIT다.
- `VssFlow_OnEvent/OnWarning`은 private 동기 owner helper다. Warning 4 lane 전체
  뒤 한 번 중재한다. `Flow_Process(enum,time)`는 no-RX INPUT → 하위 fact/PB 통지
  → STORE 기한 → 새 basis/POLICY → PB 채택 → 즉시 하위 진행 순서다.
  owner revision은 서로 같은 숫자를 요구하지 않는다. P4 보고는 생성하지 않는다.
- PB는 occurrence/session/attempt/operation을 구별하고 원 Meta/계획을 값 복사한다.
  PREPARED/START 제출/activation/actual을 분리한다. 첫 actual은 원 시간 구간 전체
  `<L`; 동일 Started Session의 정상 후속 cue에는 최초 L을 재적용하지 않는다.
  STOP/REPLACE는 후속 권한을 먼저 차단하고 전체 old retirement 뒤 새로 선택한다.
- STORE companion은 원 binding 32개와 정규화 fact 증명 64개를 수명 동안 보존한다.
  정확한 동일 fact만 ALREADY_APPLIED다. capacity 소진 시 새 효력을 차단하며
  과거 증명을 덮지 않는다. PB는 FIRST/final 두 통지와 각각의 실제 STORE 반환을
  보호한다. STORE 적용과 OUT pair 공개는 별개이며 한쪽 공간 부족 시 양쪽을 보류한다.
- actual·NO_START·termination은 각 원 scope의 충분한 증거가 필요하다. Provider,
  PCM 접근 종료, 미래 생산 차단, late 귀속도 모두 확인해야 retire한다. unknown
  과거 시작 + 안전 종료는 정상 완료/NO_START로 바뀌지 않는다. 부족/timeout만으로
  uncertain final을 만들지 않는다. TBD-FINAL 주입은 Host 전용이다.
- 오디오 어댑터의 원 기록 8개는 자동 회수하지 않는다. 실제 late callback 수명/
  세대 재사용 증명이 없는 상태에서 새 기록 공간이 없으면 HOLD한다. 제품 운용을
  위한 회수/재개 조건은 미결이며 8회 재생 보장을 뜻하지 않는다.
- 원본 PCM A/B 두 구간 외 전송 buffer는 없다. 테스트는 `(Attempt,A/B,cycle)`의
  소유권, 소비와 CPU 반환 분리, 다음 DMA 방문 전 WCET+IRQ margin을 검사한다.
  원본 전체 allocation은 32B 정렬이나 A/B는 +16/+4112라 개별 32B 정렬이 아니다.
  HALF/MAJOR/zero-fill/ready/STOP 응답은 물리 충분 증거가 아니다.
- `VSS_S32K344_REFERENCE_BINDING`은 원본 Clock/Codec/Transport header의 준비상태
  읽기만 연결한다. 모두 ready여도 HOLD다. 실제 안전 쓰기/STOP/개별 activation
  대응이 미입증이므로 vendor Start/Service를 호출하지 않는다. 자세한 경계는
  `integration/s32k344/README.md`, 음원 사본은 `audio_reference/PROVENANCE.json`을 따른다.
- 실제 target compiler/외부 SDK가 없어 TARGET_BUILD는 NOT_RUN이다. 생산 객체에는
  `*_Test*` 증거 주입·reset·계획 fixture가 없다. Host typedef shim은 target에 금지한다.
  MCU SRAM/linker/WCET, E01~E05, Rear activation 상실 뒤 과거 actual의 완전한
  권한 이력 재구성, TBD-FINAL, 제품 전체 R6 restrictions/assets/devices binding은 OPEN이다.

아래 INPUT/STORE 설명은 보존된 C1/P1-B 계약이다. 최초 결과 ZIP 및 이번 결과의
`before/` 사본으로 이전 7개 파일을 그대로 복원·비교할 수 있다.

## 내부 경계와 선택

- `vss_input.c`만 입력 세대·전달 순서·Event 10 + Warning 8 = 18문맥과 진단16을 쓴다.
  `vss_store.c`는 ledger128과 별도 Warning 8개·Rear 2개를 쓴다.
  외부 읽기는 값 복사 getter이며 숨은 반영이 없다.
- R6 `Input_Process` 반환 세 필드를 유지한다. OUT은 포인터/`size_t capacity`/
  `size_t *written`으로 구체화했다. STORE의 세 typed 포인터 중 정확히 하나만
  허용한다. P1-B에서 STATE를 구체화했다. WarningMeta는 Event ledger를 불필요하게
  팽창시키지 않는 내부 표현이며 Warning quality에는 별도 warningBasis를 사용한다.
- INPUT/STORE 내부 Header는 보존하고 P2/P3 소유 경계 Header만 추가했다. wire 값 cast,
  동적 할당, 큐, Task/RTOS, 새 상위 관리자는 없다. enum 숫자는 wire 계약이 아니다.
  `uint32_t`는 내부 Boot/occurrence/TX/원 use-limit, `uint64_t`는 로컬 ms·revision·
  예약 fact ID다. 확정 Network 또는 target ABI를 의미하지 않는다.
- 입력은 값 초기화를 마친 의미형 fixture/후속 adapter가 동기 borrow로 제공한다.
  `occurrence`만 입력 포인터이며 반환 전 INPUT·STORE가 값을 보호한다. Meta의
  기한·원 시각·관련 command는 포인터 없는 값이다. `relatedCommand=0`은 없음이다.
  Event에 없는 reason·원 평가 ordering/evidence 계보를 숫자0으로 생성하지 않는다.
- 모든 non-NULL 포인터는 유효 객체, 충분한 배열, 정렬·수명·비중첩을 caller가
  보증해야 한다. 표준 C99로 임의 dangling/forged 주소를 탐지할 수 없다.
  NULL·잘못된 enum/값·배타 인자·capacity/OUT 조합은 검사한다.
  invalid OUT은 written을 포함해 접근하지 않고 INPUT/STORE의 안전한 처리는 계속한다.
- 실행은 하나의 직렬 owner로 한정한다. ISR/reentrant/concurrent 호출은 계약 밖이다.
  공개는 owner 변경 완료 후 revision으로 표시한다. P1-A의 record revision은
  수용/최초 상실만 증가하며, INPUT revision은 non-wrap u32 전달 상한보다 넓은 u64다.

## 세대와 시간

기본 Event admission은 닫혀 있다. `Input_OpenAfterSynchronization`은 origin별
한 번만 열 수 있는 내부 bootstrap 경계다. 호출자는 **VSS 동기화 이후 실제 새
DOMAIN 승인 세대**를 외부에서 입증해야 한다. 수신 첫 Boot만으로 호출하면 안 된다.
Host에서만 그 전제를 fixture로 제공했다. 실제 proof 공급자/영속 ledger 복원은 E05다.
Boot 변경 또는 확정 시계 reset/wrap/backward가 관측되면 해당 origin을 닫고 기존
Event replay를 차단한다. 같은 실행에서 재개/삭제 API는 없다. 재개는 후속 lifecycle
구현까지 HOLD이며, 시험용 reset 함수는 `VSS_HOST_TEST` 빌드에만 존재한다.

C1에서 전달 순서의 반영점을 EVENT 의미 수용과 분리했다. 알려진 타입/출처·origin,
승인 Boot와 열린 gate, 정상 현재/RX 시간의 같은 domain/epoch 및 RX 상한≤현재 하한,
양수·증가 non-wrap TX를 확인한 뒤 전달의 값 사본을 즉시 보호한다. 그 뒤 최초 INVALID에
기존 signal 문맥이 없거나 EVENT Age/use-limit/의미 검증이 실패해도 이전 TX는 거부한다.
같은 TX/동일 입력은 NO_CHANGE, 같은 TX/다른 입력은 REJECTED다. 전달 순서만 보호한
경우 `contextApplied`는 false이며 VALID/Pending/Started를 만들지 않는다. 이후 새 TX의
다른 유효 발생은 정상 검증한다. malformed enum, 미확인 출처/Boot/origin/RX 시각으로
순서를 전진시키지 않으며 기존 세대/시계 상실의 닫힘·replay 제한은 유지한다.

여기서 verified delivery는 **typed Host/adapter 경계**의 조건이다. 원 프레임의
CRC/ID/DLC/예약값·DOMAIN source와 RX meta의 실제 association은 이 타입만으로 증명할 수
없으므로 실제 Network adapter가 먼저 보증해야 한다. C1은 그 raw 검증이나 새 Wire 필드/API를
추가하지 않았다. P1-B는 공통 source TX high-water와 Warning 프레임 내부 4 lane의
원 근거 순서를 분리했다. 실제 raw frame/DTO association 검증은 계속 외부 Gate다.

RX 시각과 Age 구간으로 `[rx.low-age.high, rx.high-age.low]`를 계산한다.
RX→소비 경과는 현재 시각과 이 원본을 비교하는 방식으로 한 번만 반영한다.
오차·전송 공백을 모두 포함했다는 `ageComplete` 근거, 같은 domain/epoch,
정상 구간·비음수 산술이 필요하다. 포화65535·미측정 공백·underflow·overflow는 거부한다.
음수 로컬 원 시각을 지원하지 않는 보수적 선택이며 clock 실바인딩에서 검토한다.
RX 상한≤소비 현재 하한을 요구하므로 시간 오차 구간이 겹치면 수용을 보류한다.

`L=min(양수 use_limit,500,2000)`를 원 수신값과 함께 보존한다. INPUT fresh 적용과
STORE read-only 최초 시작 기회는 전체 구간 `<L` 조건이다. 같은 key 재전달의
원 시각 구간/원 use-limit/종류/관련 command가 달라지면 보수적으로 충돌 거부한다.
겹치는 구간을 합성하거나 더 젊게 보정하지 않는다. 정상 재전달은 원 Meta와
최초/최종 fact/replay/revision을 바꾸지 않는다.

Event 무수신에는 150ms watchdog을 적용하지 않는다. 명시적인 최신 quality loss는
해당 origin/Boot/signal의 모든 보호 Event에 replay=false를 단조 적용한다.
첫 lossAt이 미확인이어도 그대로 보호하고 다음 정상/중복/상실로 교체하지 않는다.
기한 또는 상실만으로 Started/Expired/NO_START/retirement를 만들지 않는다.
P2는 충분한 typed backend 근거를 정규화하여 STORE에 적용한다. 실제 target producer는
미입증이라 차단되어 있으며 현재 actual 시험 근거는 **SYNTHETIC_TEST_EVIDENCE**뿐이다.

## 용량과 후속 경계

ledger128에는 accepted와 의미 검증에서 거부된 신뢰 가능한 key의 tombstone이
함께 들어간다. full이면 새 Event를 거부하고 기존 duplicate/quality는 처리한다.
회수/eviction이 없어 full 뒤 누락 tombstone이 나중에 재수용될 경로도 없다.
`Store_ProtectRejectedEvent`는 INPUT이 확인한 key의 재전달 방어용 내부 helper이며
별도 Core/Queue가 아니다. tombstone의 state/fact 필드는 accepted 상태로 해석하지 않는다.
저장 full·충돌 등 STORE 거부에도 INPUT의 이미 적용한 출처 순서는 rollback하지 않는다.

accepted마다 최초/최종 정규화 fact의 key/occurrence/session/attempt/time/revision
공간을 미리 확보한다. 보존 Header의 `VssReservedPlaybackFact` 표현을 P2 companion의
`VssPlaybackFact` alias로 사용한다. 원본 Header의 P2 NOT_RUN 주석은 P1-B 당시 설명이다.

진단16이 가득 차면 기존 기록을 덮지 않는다. 포화 lossCount/latch와 각 입력
문맥의 첫/최근 손실 요약을 보존한다. 문맥을 특정 못한 인자 오류용 요약 슬롯은
한 개 더 둔다. OUT 값 복사 후에만 pending을 해제하며 loss latch는 자동 해제하지 않는다.
UINT32 TX wrap/과거 순번은 거부하고 새 정상 세대로 추정하지 않는다.

## P1-B Warning typed 경계

`Input_ProcessWarningFrame`은 기존 INPUT 파일 안의 private 동기 adapter helper다.
제품 callback/public Core/Wire ABI를 추가하지 않는다. 고정 배열 순서는
REAR_ACTIVATION → REAR_RISK → OCCUPANT_HAZARD → ANTI_PINCH다. DOMAIN Boot/TX/RX
검사는 프레임당 한 번, 의미·원 계보·품질 검사는 항목마다 독립 수행한다.
뒤 항목의 거부는 앞 항목을 rollback하지 않는다. 반환 `complete`는 네 결과가
준비됐다는 뜻이며 전부 수용했다는 ACK가 아니다. P2의 `VssFlow_OnWarning`은
이 전체 반환 뒤 한 번만 재중재한다. 실제 Network callback 연결은 여전히 OPEN이다.

`structureVerified`, `provenanceProven`, `ageComplete`, `activationBasisProven`은
실제 adapter가 먼저 입증해야 할 조건이다. 기본 false는 정상 승격 근거가 없다.
Host fixture가 true를 제공한 시험을 실제 producer/원 Power E03 검증으로 쓰지 않는다.
초기 source generation도 명시적 provenance가 필요하다. 원 sensor Boot가 바뀌면
해당 lane은 fence되고 자동 승인·재개하지 않는다. DOMAIN Boot/clock 손실은 origin
전체를 닫는다. 정상 재개에 필요한 lifecycle/승인 경계는 E05에 남긴다.

Typed duplicate는 source/Boot/TX와 네 payload 의미 필드를 값 비교한다. 수신 시각
변경만으로 같은 TX를 새 전달로 만들지 않는다. 이는 raw bytes 동일성 검사가 아니다.
새 증가 TX는 네 lane의 거부 여부와 무관하게 보호된다. Event와 Warning은 같은
DOMAIN source TX 고수위를 사용하며 서로의 과거 TX를 수용하지 않는다.

| signal | 원 근거 | TTL(ms) | Hold(ms) |
|---|---|---:|---:|
| Anti-Pinch | WINDOW, TEST만 지원 | 250 | 250 |
| Occupant | Pi 원 근거, CIS 경유 보존 | 1000 | 500 |
| Rear Risk | CIS Rear 원 근거 | 150 | 150 |
| Rear Activation | DOMAIN 독립 Power 평가, availability NOT_PROVIDED | 500 | 0 |

같은 원 순번의 정상 반복은 liveness만 갱신하며 원 시각/TTL/revision을 새 판단으로
갱신하지 않는다. 같은 근거에서 정상값 변경이나 품질 회복은 E01 INVALID 제한이다.
과거 원 순번은 품질 상실도 적용하지 않는다. 새 원 순번도 품질·사유·가용성·원
출처·시각을 모두 검증한다. 제한/UNKNOWN/NO_DATA는 CLEAR가 아니다. 최초 NO_DATA와
activation 근거 부재는 UNASSESSED, 유효 이력 이후에는 INVALID다. PRODUCT Anti는
항상 미지원/UNASSESSED이며 TEST와 별개다. C2 원 구별은 named meaning tag로 보호하지만
이 tag를 실제 Wire 확장으로 선언하지 않는다. raw format0/1/60/64/DLC15 검사는 없다.

## 수신 감시·Hold·후보 사용

감시는 첫 구조 검증·승인 Boot·증가 TX Warning의 RX에서 시작한다. 첫 예상 RX나
startup grace를 임의로 만들지 않는다. 이후 lastAdvancingRx+150ms를 사용한다.
동일 TX/과거 TX/malformed는 이 기산을 갱신하지 않는다. Event silence 자체는
감시하지 않지만, 승인된 Warning monitor 상실은 해당 DOMAIN Event replay를
단조 false로 내린다. 감독이 복구되어도 기존 replay=false는 유지된다.

`Input_Process(NULL,time,...)`은 정적 총18문맥 중 제공된 time domain의 9개를
고정 순회한다. 다른 origin의 시각은 빼지 않는다. 대표 반환은 첫 REJECTED 전체,
없으면 첫 변화 전체, 없으면 NO_CHANGE/false/NOT_SUBMITTED다. 항목별 결과와 진단은
별도 보호한다. INPUT public 경계 반환 전에 Warning 전용 STORE 기한 평가도 끝낸다.
Event/Warning 호출과 malformed 프레임 처리 기회에서도 유효 Runtime 시간 감시는
진행한다. 실제 10ms owner 주기/WCET는 E04의 측정 대상이다.

STORE의 마지막 유효값/meta, 현재 품질, firstLossAt, holdAnchor는 독립이다.
firstLossAt은 실제 최초 상실의 원 구간, anchor는 그 하한과 원 validUntil의 최소다.
최초 시각이 UNKNOWN이면 anchor를 만들지 않는다. 후속 INVALID/늦은 now로 보완하지
않는다. 새 유효 원 판단만 이전 Hold를 끝낼 수 있다. `[earliest,latest]` 전체가
deadline 미만이면 APPLICABLE, earliest>=deadline이면 EXPIRED, 경계 걸침/비교 부족은
UNCERTAIN이다. 확정 expiry는 latch되며 CLEAR나 물리 STOP 완료로 바뀌지 않는다.

DISABLED/activation loss는 riskApplicable/Hold를 제거하고 riskFence를 보존한다.
ENABLED만으로 옛 위험은 부활하지 않는다. 현재 fresh 활성과 fence보다 엄격히 새로운
동일 승인 계보의 위험만 결합한다. Rear source 세대 상실도 결합을 닫는다.

`Store_ReadWarning`은 lastValidMeta/quality/loss/anchor와 현재 validity,
newSessionAllowed/evaluatedAt/revision을 복사한다. getter는 시간 평가를 숨겨 수행하지
않으므로 consumer는 해당 INPUT 처리 완료, 현재 시간/owner revision을 함께 확인한다.
유효 적용의 실패가 주입된 시험에서는 inputIncomplete로 옛 후보도 차단하며 새 유효
평가 전에는 복원하지 않는다. 정적 Warning 슬롯에는 정상 실행의 용량 부족 경로가
없고, `Store_TestRejectWarningOnce`는 이 부분 실패 계약만 시험하는 Host 전용 hook이다.

`Store_AdvanceWarningDeadlines`는 INPUT의 시간/품질 반영 뒤 사용하는 private Warning
부분 구현이다. P2 companion의 `Store_AdvanceDeadlines(time,playback)`가 PB 통지와
원 binding을 확인한 뒤 Event 기한까지 평가한다. 후보 허용은 실제 Session 시작/
old retirement/STOP/actual 증거가 아니다.
INPUT/STORE는 음원 선택이나 오디오를 호출하지 않는다.

기존 C1 20그룹/2,769검사는 유지했다. STATE가 실체화되어 기존 시험 한 곳의 다른
타입 pointer cast를 실제 invalid VssStateUpdate fixture로 교체했으며 검사 의미/개수는
같다. 신규 시험은 `test_warning_hold.c`에서 별도 실행한다. Host sizeof/BSS/stack usage/
반복 시간은 ABI·성능 참고이며 S32K344 SRAM/WCET 충족 근거가 아니다.

근거: [A1 입력 Binding](../../docs/vss/design/90_BINDING/92_NETWORK_BINDING.md),
[R6](../../docs/vss/design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md),
[Ownership](../../docs/vss/design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md),
[Timing](../../docs/vss/design/50_CONTRACTS/30_TIMING_FRESHNESS.md).
