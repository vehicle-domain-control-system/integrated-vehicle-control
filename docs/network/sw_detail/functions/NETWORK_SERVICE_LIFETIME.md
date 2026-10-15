# Network Service — 안정 접수·Queue·결과·참조 수명

[개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [Data](../data/NETWORK_SERVICE_DATA.md) · [TxPort26](DOMAIN_TX_PORT.md) · [후발 실패·Health](NETWORK_FAILURE_HEALTH.md)

책임은 L02/L03/L05의 M03/M04/M07/M08/M09, F07/F09~13/F19~25와 ST04~07/ST09~12에 둔다. 새로운 Service Manager나 모든 메시지용 공개 `Submit(void*)`를 만들지 않는다. 메시지 필드 검사·값복사·Codec는 기존 [CAN22](../04_CAN_FD_OVERVIEW.md), [UART-M23](../05_UART_M_OVERVIEW.md), [UART-C3](../06_UART_C_OVERVIEW.md)가 정본이다. B15는 DOMAIN 경계에서 그 절차를 호출하는 조건과 소유권을 연결한다.

<a id="admission"></a>
## F07/F10/F11 — 접수 성공을 반환할 때 이미 성립해야 할 일

| 단계·담당 | 실제 입력과 원자 처리 | 실패 시 상태 |
|---|---|---|
| M03 typed 검사 | 실제 callback 원형의 모든 인자, NULL/enum/범위/필수 문맥·원 identity·현재 타입에서 표현 가능한 품질을 검사 | 잘못된 입력은 INVALID_ARGUMENT. 임의 원 Generation/Session·정상 품질로 보정하지 않음 |
| M01/M03 준비 | 해당 Port와 profile·원 생산 세대/대상 Boot·필수 후발 실패 sink·호출 문맥의 실제 바인딩 확인 | 해당 경로 NOT_AVAILABLE. 다른 정상 경로까지 차단하지 않음 |
| M04 저장소 예약 | 원 DTO 전체와 모든 의미상 필수 metadata·identity·시각을 보관할 bounded 공간을 **함께** 예약 | 일부만 확보하면 원장 공개 없이 예약만 되돌림. 기존 개별 항목은 그대로. QUEUE_FULL |
| M03/M04 복사 | callback 반환 전 caller와 독립된 완전 사본 또는 보관 가능한 의미/출처를 동반한 동기 encode | struct 안 pointer까지 점검. 포인터 값만 복사한 것은 실패. prepared 사본은 아직 외부에 보이지 않음 |
| M04 원장 공개 | 사본·접수시각·원 identity·Queue 엔트리를 같은 직렬/원자 처리에서 공개 | 부분 Queue 게시/sequence만 진전 금지. 실제 동기화 방식은 BINDING_OPEN |
| callback 반환 | 위 단계가 모두 끝난 경우 기존 NET_SUBMIT_OK를 명시 변환하여 DOMAIN_IF_OK | 이 시점은 실제 Driver 전송·상대 수신·차량 DONE이 아님 |

반환전 전체 복사가 성공했어도 Queue 게시가 실패하면 OK를 반환하지 않는다. Queue 예약 후 encode가 실패해도 이전 의미 사본/다른 항목을 지우지 않는다. 실제 allocator에 부분 실패가 가능하면 예약·취소·게시의 원자성을 바인딩에서 입증해야 한다. 입증 전 의사코드를 제품에 연결하지 않는다.

기존 모든 C2 callback의 정확한 인자와 복사 대상은 [TxPort 함수별 명세](DOMAIN_TX_PORT.md)를 따른다. Gateway nested ByteView는 이 C2 table에 새 인자로 추가하지 않는다. 기존 Gateway 경로의 deep copy를 같은 Service 원칙으로 설명할 수 있지만 Gateway 본체나 새 공개 C2 signature를 만들지는 않는다.

<a id="queue-policy"></a>
## 자료 성격별 보존·포화·선택

| 성격 | 같은 항목의 식별과 보존 | 교체·포화·재시도 |
|---|---|---|
| 현재 상태/현재 Warning | 메시지·scope/source·평가 identity·품질·원 AGE | Owner가 허용한 같은 의미 key의 아직 전송 전 pending만 교체. 고장/열화도 유효한 새 보고일 수 있으므로 값의 정상/불량만으로 숨기지 않음. 정확 정책 OPEN |
| Command | 원 producer Generation+commandId+불변 명령 본문, 필요 target Boot 및 원 Request | 개별 보존, Queue full이면 새 제출 거부. 중복으로 새 commandId 발급하거나 기존 명령 덮지 않음 |
| Result/Event | 원 결과 단계·request 또는 occurrence identity 및 원 생산 세대 | 결과단계 전이는 Owner 근거로 판단. 단순 같은 commandId만으로 다른 단계 삭제 금지. 개별항목 덮어쓰기 없음 |
| Query 응답/End | 원 Query Device/Session/DOMAIN Boot/QueryID/scope와 응답 사본 | 자발 갱신과 분리, M02가 실제 최종 TX로 집계. 한 Query 응답을 다른 Query의 최신상태로 교체하지 않음 |
| 완성 CAN/UART Frame | 원 의미 사본 + 해당 전송 시도 token/epoch + 정확 bytes | Driver가 참조할 가능성이 있으면 불변. AGE/sequence/CRC도 변경 금지 |
| 원 CAN evidence/C2 snapshot | 같은 관측의 단일 인계/commit 결합 근거 | 값만 같다고 합치지 않음. 결합 불확실하면 해당 투영만 제한 |

현재상태의 교체는 새 raw 관측과 원 품질·순서·시간 근거를 비교한 후 결정한다. B12/B13의 같은 관측 보수적 AGE 규칙을 유지한다. A의 AGE20@100, B의 AGE15@130을 now150에서 비교하면 A70/B35이며 새 수신시각만으로 원 관측을 젊게 만들지 않는다. raw 비교용 AGE와 누적 AGE를 별도 보관하고 frame순서·원 meta·보수적 AGE는 함께 원자 갱신한다.

Queue 깊이·우선순위·재시도 수/간격은 정하지 않는다. S4 T38의 50ms/20ms/40ms 재시도 문구는 후보 이력이지 B15에서 채택한 Driver 정책이 아니다. 결과 미확정 항목에 남는 buffer가 생겼다는 이유로 새 전송을 시도하지 않는다.

<a id="dispatch"></a>
## F12/F19/F22 — 송신 선택부터 Driver 호출

1. 기존 메시지 Service가 현재 문맥·원 AGE·기한·profile·HOLD를 다시 검사한다. `UNKNOWN elapsed`는 0ms가 아니다. 과거 RequestSession을 현재 recipient Session으로 바꾸지 않는다.
2. 기존 Codec가 정확한 Frame을 만든다. M07은 Frame 저장소와 별도의 원 identity/결과 기록을 확보한다. 불변 Frame과 logical result record는 수명이 다르다.
3. Driver **호출 전** 고유한 로컬 token/epoch record를 callback이 찾을 수 있게 공개한다. 참조 가능 상태를 먼저 설정하여 Driver 호출 중 즉시 callback이 오는 경우도 같은 원장에 직렬화한다. 이 순서·callback 동기화가 입증되지 않았으면 호출하지 않는다.
4. 실제 Driver 반환/통지를 [CAN 관측 정본](CAN_TRANSFER_CONTROL.md#port), [UART-M 관측](UART_M_TX_RX_CONTROL.md), [UART-C 관측](UART_C_TX_RX_CONTROL.md#port-observe)에 연결한다. 이 함수들의 이름이 공용 SDK API라는 뜻은 아니다. 링크별 제안을 M08이 실제 Driver 계약으로 바인딩해야 한다.
5. admission/terminal/release가 뒤섞여 도착해도 원 token/epoch와 원 immutable identity를 대조한다. 중복 통지는 새 성공·실패·해제가 아니며 상반된 terminal은 충돌 기록과 HOLD다.

Driver 임시 busy와 확정 제출 실패는 다르다. 반환 OK는 terminal success가 아니며 accepted=false만으로 no-reference를 가정하지 않는다. `localToken/epoch` 발급·wrap·보관 수명은 실제 M07/M08 계약으로 확인한다. 원격 Boot나 명령 ID를 token으로 재사용해 지연 callback을 다른 항목에 붙이지 않는다.

<a id="retirement"></a>
## 내부 함수 — 세 수명 종료의 개별 판정

**PROPOSED_PRIVATE_FUNCTION / Network 소유.**

```c
NetSvcResult NetSvc_ClassifyRetirement(const NetSvcLifetimeView *view,
    NetSvcRetirement *outActions);
```

| 항목 | 계약 |
|---|---|
| 호출자·문맥 | M07/M04의 같은 소유자 Worker/Superloop 처리. ISR·DOMAIN Manager 안에서 재진입 호출하지 않음 |
| IN | [NetSvcLifetimeView](../data/NETWORK_SERVICE_DATA.md#type-NetSvcLifetimeView) 전체. 동일 시도의 실제 원장을 한 번에 읽은 borrowed 사본. 복합 상태를 서로 다른 시점에서 조립하지 않음 |
| OUT | caller-owned [NetSvcRetirement](../data/NETWORK_SERVICE_DATA.md#type-NetSvcRetirement). NULL 불가. 성공 때 세 필드를 함께 쓰며 실패 때 모두 불변. IN/OUT 저장소 중첩 금지 |
| 반환 | OK는 행동 가능성 판정. INVALID_ARGUMENT는 NULL/enum/불가능 조합. EVIDENCE_HOLD는 미확정 결과인데 이미 예약을 종료한 등 모순. 실제 물리 회수·송신·C2 반영을 수행하지 않음 |
| 선행 바인딩 | 실제 Driver reference 의미, 소유자 직렬화, 원 token/epoch 연결, 필수 결과 소비/예약 완료 기록. 이 근거는 임의 bool 입력으로 대체하지 않음 |

```c
NetSvcResult NetSvc_ClassifyRetirement(const NetSvcLifetimeView *view,
    NetSvcRetirement *outActions)
{
    if (view == NULL || outActions == NULL) return NET_SVC_INVALID_ARGUMENT;
    if (view->reference < NET_SVC_REFERENCE_NEVER_EXPOSED ||
        view->reference > NET_SVC_REFERENCE_ENDED ||
        view->terminal < NET_SVC_TERMINAL_UNKNOWN ||
        view->terminal > NET_SVC_TERMINAL_STOPPED_BEFORE_SEND ||
        view->consumer < NET_SVC_CONSUMER_NOT_REQUIRED ||
        view->consumer > NET_SVC_CONSUMER_CONFIRMED ||
        view->reservation < NET_SVC_SOURCE_RESERVED ||
        view->reservation > NET_SVC_SOURCE_RECONCILED)
        return NET_SVC_INVALID_ARGUMENT;
    if (!view->frameStorageAcquired &&
        (view->frameMemoryReturned ||
         view->reference != NET_SVC_REFERENCE_NEVER_EXPOSED))
        return NET_SVC_INVALID_ARGUMENT;
    if (view->frameMemoryReturned && view->reference == NET_SVC_REFERENCE_POSSIBLE)
        return NET_SVC_EVIDENCE_HOLD;
    if (view->frameMemoryReturned &&
        view->reference == NET_SVC_REFERENCE_NEVER_EXPOSED &&
        view->terminal != NET_SVC_TERMINAL_STOPPED_BEFORE_SEND)
        return NET_SVC_EVIDENCE_HOLD;
    if (view->consumer == NET_SVC_CONSUMER_CONFIRMED &&
        view->terminal == NET_SVC_TERMINAL_UNKNOWN)
        return NET_SVC_EVIDENCE_HOLD;
    if ((view->terminal == NET_SVC_TERMINAL_TX_SUCCEEDED ||
         view->terminal == NET_SVC_TERMINAL_TX_FAILED) &&
        view->reference == NET_SVC_REFERENCE_NEVER_EXPOSED)
        return NET_SVC_EVIDENCE_HOLD;

    bool terminalKnown = view->terminal != NET_SVC_TERMINAL_UNKNOWN;
    bool consumerDone = view->consumer != NET_SVC_CONSUMER_WAITING;
    if (view->reservation == NET_SVC_SOURCE_RECONCILED &&
        (!terminalKnown || !consumerDone)) return NET_SVC_EVIDENCE_HOLD;

    NetSvcRetirement next;
    bool referenceEnded = view->reference == NET_SVC_REFERENCE_ENDED;
    bool neverExposedStopped =
        view->reference == NET_SVC_REFERENCE_NEVER_EXPOSED &&
        view->terminal == NET_SVC_TERMINAL_STOPPED_BEFORE_SEND;
    next.mayReturnFrameMemory = view->frameStorageAcquired &&
        !view->frameMemoryReturned && (referenceEnded || neverExposedStopped);
    next.mayEndSourceReservation = terminalKnown && consumerDone &&
        view->reservation == NET_SVC_SOURCE_RESERVED;
    bool storageDone = !view->frameStorageAcquired || view->frameMemoryReturned;
    next.mayRetireIdentity = terminalKnown && consumerDone && storageDone &&
        view->reference != NET_SVC_REFERENCE_POSSIBLE &&
        view->reservation == NET_SVC_SOURCE_RECONCILED;
    *outActions = next;
    return NET_SVC_OK;
}
```

참조가 먼저 끝났고 결과가 UNKNOWN이면 `mayReturnFrameMemory=true`일 수 있어도 두 후속 값은 false다. 회수 후에도 원 identity와 M04 예약을 별도 공간에 유지한다. terminal이 먼저이면 실제 release 전 Frame 주소를 재사용하지 않는다. 동일 시도 record의 storage 반환·reservation 종료·identity 제거는 각 실제 작업과 원장 반영이 함께 완료돼야 하며 위 출력만 보고 완료 flag를 미리 쓰지 않는다.

원 의미 항목이 다시 송신 가능한지는 새 결정이다. `mayEndSourceReservation`은 과거 시도의 결과 정리 허용일 뿐 재송신 승인·새 SourceGeneration/AGE0·원격 성공을 뜻하지 않는다. 기존 retry/HOLD와 원 문맥·기한을 다시 검사한다. Query는 응답 최종 결과를 M02가 한 번 집계하기 전 consumerDone으로 바꾸지 않는다. 실패 sink가 필요한 항목은 [후발 실패](NETWORK_FAILURE_HEALTH.md#delivery)의 미확정 반영을 생략하지 않는다.

<a id="recovery"></a>
## 오류·재시작·해제와 재개

| 관측 | 보존·제한 | 다시 진행할 근거 |
|---|---|---|
| CAN bus-off/UART link down | 해당 링크 신규 인계 제한, in-flight/미확정/참조종료 뒤결과대기를 구분 | 실제 Driver 회복·참조 상태와 해당 profile/원 문맥/필수 consumer 확인 |
| peer reboot/Session 변경 | 옛 원본/대상/Query를 현재 세대로 재라벨하지 않음 | Owner가 수용한 새 세대·원 의미/기한 연결. 다른 peer 상태는 유지 |
| local runtime reset | 과거 callback과 새 token 공간 격리, 기존 Driver 참조 미확정이면 저장소 재사용 금지 | 실제 초기화/등록/참조 드레인과 clockEpoch 근거. callback 취소를 추정하지 않음 |
| Queue full | 새 입력의 부분 접수 없음. 기존 discrete·Query 사본 보존 | 안전히 반환된 용량과 현재 입력의 재검증. 무제한 자동 retry 없음 |
| C2 거부/Observer 오류 | 원 반환·원 사건 보존. Core 반영 불명확하면 해당 identity 재호출 HOLD | 실제 Owner의 부분 반영/commit/Observer 단계 구분 |

실제 local Network init/reset과 peer reboot·정상 수신·Query 완료는 다르다. 후자는 운영 counter 초기화 근거가 아니다. WINDOW/VSS 미결을 정상 구현 완료로 켜지 않고 관련 경로의 HOLD를 유지한다.

## 문서 반례

| ID | 사건 | 기대 판정·보존 |
|---|---|---|
| SVC-L01 | callback 사본 완료, Queue 게시 실패 | OK 없음, prepared 예약만 취소, 기존 항목 불변 |
| SVC-L02 | Queue full, 기존 command 존재 | 새 QUEUE_FULL, 기존 command/identity 덮기 없음 |
| SVC-L03 | release 먼저, terminal UNKNOWN | Frame 회수만 허용 가능, 원 예약/identity 유지 |
| SVC-L04 | terminal TX success 먼저, reference POSSIBLE | Frame 회수/identity 제거 금지. Query 집계와 참조 수명 구분 |
| SVC-L05 | 옛 epoch/token callback | 실제 기존 관측 함수에서 현재 record 불변, 새 buffer 해제 금지 |
| SVC-L06 | terminal success와 실패가 같은 시도에 충돌 | 정상 상태로 합치지 않고 원 관측 보존·HOLD |
| SVC-L07 | never exposed·실제 송신 전 중단, Frame 미취득 | 메모리 반환 작업 없음, 필수 실패 인계/예약 반영 뒤 identity 정리 가능 |
| SVC-L08 | Query 최종 TX 미집계인데 ConsumerConfirmed 표시 | 금지된 입력 생성. M02의 실제 원자 집계 증거 없으면 WAITING |
| SVC-L09 | Driver accepted=false, release 미확인 | POSSIBLE 유지, 물리 회수 금지 |
| SVC-L10 | 여러 시점에서 조립한 LifetimeView | 호출 선행 원자 사본 계약 위반, 실제 바인딩 HOLD |
| SVC-L11 | 같은 원본 반복 송신과 새 LinkSequence | 원 평가/AGE 갱신 없음. 기존 시도 미확정이면 추가 시도 제한 |
| SVC-L12 | Core commit 후 optional Observer 실패 | Core 재실행 없음, 필수 실패 sink 성공과도 혼동하지 않음 |

정적 근거: 기존 CAN_TRANSFER_CONTROL·UART_M_TX_RX_CONTROL·UART_C_TX_RX_CONTROL과 S4C RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 이 의사코드는 문서 계약의 판정 예이며 실제 Queue/SDK 실행 검증 결과가 아니다.
