# SC17·SC18 — 재접속과 실패를 원 사건에 귀속하는 경계

[E2E 전체](../../09_E2E_FAILURE_RECOVERY.md) · [기존 Service 수명](../../functions/NETWORK_SERVICE_LIFETIME.md) · [실패·Health](../../functions/NETWORK_FAILURE_HEALTH.md) · [DOMAIN 노드](../../nodes/DOMAIN.md)

이 문서는 새 공통 FSM·Queue·타이머·복구 API를 만들지 않는다. 각 기존 경로의 원 제출, 전송 시도, 상대 소비, 기능 결과를 연결한다. 표의 상태명은 설명용이며 공개 enum이 아니다. L01~L05/M01~M09/F01~F25의 기존 책임과 ST01~ST12를 유지한다. SC18의 원 직접 Wire/Rx는 **0/0**이다. 아래 BCM_DOOR_COMMAND·P_VISION_RESULT·M_QUERY_END는 공통 경계의 예시이며 직접집합에 추가하지 않는다.

## 무엇이 완료되었는지 구분하는 축

| 축·Owner | 시작/관측 | 보존할 자료·끝나는 조건 | 다른 축으로 바꾸지 않는 사실 |
|---|---|---|---|
| 원 의미·기능 Owner | 요청·평가·명령·발생 생성 | 원 Generation/Boot·Request/Command/Occurrence·품질·AGE, 실제 기능 결과 | Network 접수·timeout·TX 성공이 기능 DONE/FAILED가 아님 |
| M03/M04 접수 | schema/문맥·필수 sink·용량 검증 → 안정 사본·원자 게시 | 원 DTO/원 meta·접수시각·불변 identity | Queue 예약만 성공하거나 pointer만 복사한 것은 접수 OK가 아님 |
| M07/M08 전송 시도 | 고유 로컬 token/epoch 기록 후 Driver 호출 | 같은 시도의 accepted·terminal·reference를 독립 관측 | accepted=false도 참조 미취득 증거가 아닐 수 있음 |
| 상대 Network/typed 소비 | 상대가 검증한 frame와 원 수신시각 | 전체 필드·출처·품질 검사와 해당 소비 Owner의 인수 근거 | CAN ACK/UART TX·void callback 반환은 소비/실행 보증 아님 |
| M02/DOMAIN/Gateway 결과 소비 | 실제 원 결과·Query응답·필수 실패 인계 | 같은 항목에 대한 필수 소비 반영과 원 예약 정리 | optional Observer·counter가 필수 Manager 수용을 대체하지 않음 |

원 ECU/Pi Boot, CAN 송신 Boot/TX_SEQUENCE, 원 관측 순번, C2 sourceGeneration, DOMAIN Boot, 현재 recipient Session, 과거 RequestSession, 로컬 Driver epoch/token은 독립이다. 같은 숫자나 같은 값은 같은 사건의 증거가 아니다. 시간은 입증된 동일 노드·같은 clock 수명의 경과만 한 번 더하며 미측정 공백을0으로 만들지 않는다. Generation0·wrap/재사용 창·Clock·Deadline은 원 미결 그대로다.

<a id="sc17"></a>
## SC17 — 부팅·세대 변경·단절/재접속·결과 미확인

원 직접집합 **9 Wire/12 Rx**, N01~N08/P01~04/R01~06/I01~07을 보존한다. 직접 Wire는 DOMAIN_PERMISSION, BCM_STATUS, CIS_STATUS, VSS_STATUS, M_CONTEXT, M_QUERY, M_DOMAIN_ALIVE, M_RESULT, M_QUERY_END다. Permission의 CAN4수신이 Wire/Rx 수 차이를 만든다. WINDOW 가지는 DEFERRED다.

1. 각 실제 Driver/기존 감독 Owner가 자기 링크 단절·bus-off·runtime reset·peer Boot·Session 변화를 관측한다. 임의 정상 CAN 한 건으로 모든 ECU의 생존이나 복구를 선언하지 않는다. 해당 링크의 신규 접수를 제한하면서 이미 참조 중인 프레임·원 결과 기록을 유지한다.
2. 미시작 명령·원 대상/기동·Query 완료 자격을 이전 문맥에서 분리한다. 다른 peer의 자료를 일괄 지우거나 현재 Boot로 과거 Result를 재라벨하지 않는다. 논리적 무효화가 메모리 해제를 뜻하지 않는다.
3. ESP32는 실제 등록/연결 공동 tuple, App 활성, 근접의 세 독립 생산 근거로 M_CONTEXT를 만든다. [Gateway 경계](../../nodes/ESP32_MOBILE.md)의 Publish3와 `EspNetPriv_PublishContext`를 사용한다. BLE CONNECTED가 App ACTIVE를 생성하지 않는다.
4. DOMAIN Network의 `DomainNetPriv_PublishAlive` → M_DOMAIN_ALIVE → `EspNetPriv_OnDomainAlive`는 private 감독이다. ALIVE12B에는 SessionReady가 없다. 이 메시지를 App으로 중계하거나 차량 상태·Permission의 정상성을 합성하지 않는다.
5. 현재 문맥이 실제 검증된 뒤 [SC15 Query](QUERY_WARNING_SESSION.md#sc15)로 기존 상태/보관 결과를 조회한다. Query가 과거 명령을 재실행하거나 잃은 ECU 결과를 복원하지 않는다. 동일 단말의 과거 RequestSession은 현재 recipient Session과 따로 보존한다.
6. DOMAIN Owner가 확인한 원 Result와 신규 유효 상태만 현재 처리 문맥에 연결한다. 운송 복귀·상태 수신·기능 복구·App 인지 중 확인된 단계까지만 기록한다. G07 전체 노드 전원 종료 프로토콜은 미확인이다.

| 사건·이전 상태 | 제한·남는 관측 | 재접수/종결에 필요한 근거 |
|---|---|---|
| Query 진행 중 새 Session, 구 End 도착 | 이전 Query의 완료 자격 없음. 새 Query count/현재 표시 불변 | 현재 Device/Session/DomainBoot/QueryID/scope와 실제 응답·End의 연결 |
| CAN bus-off 뒤 link up, 과거 명령 pending | 새 실행 자동 재송신 없음. 원 결과 미확인 보존 | Driver/참조 회복·현재 대상Boot·원 기한·Owner의 기존 재접수 정책 |
| VSS reset, 일회 음향 중단 | 과거 Event 재생 없음, 위험 CLEAR 합성 없음 | 새 유효 현재 Warning으로 기존 VSS 중재; CF02 계속 HOLD |
| peer reboot, 운영 counter 존재 | peer의 원 상태는 새 기동 근거와 분리. counter 임의reset 없음 | 실제 local Network init/reset만 기존 counter reset 근거 |
| 모든 Frame release, 일부 terminal 미관측 | 저장소와 별개로 원 결과·예약·identity 유지 | 해당 시도의 실제 terminal 및 필수 소비·예약 정리 |

유지 ID는 IF-O08/09/12/25·G06/G07 및 G56-O01/O02/O05다. 새 Session 발급 정책이나 자동 재시도 횟수를 만들지 않는다.

<a id="sc18"></a>
## SC18 — 기존 전송 경계의 거부·부분 수신·용량·후발 실패

원 직접집합은0/0이고 모든8노드·물리4/내부7 경계의 공통 책임을 추적한다. 형식이 유효한 열화 보고와 malformed를 구분하고 현재 상태·discrete 항목·ordered Query의 보존 방식은 [기존 Service](../../functions/NETWORK_SERVICE_LIFETIME.md#queue-policy)를 따른다.

| 검출 주체·사건 | 즉시 인계·부작용 제한 | 회복과 재접수 조건·정본 |
|---|---|---|
| M05 CAN ID/profile/DLC/padding/필드 오류 | typed 인계·정상 liveness 갱신 없음. 차량 Fault 합성 없음 | 새 완전 입력을 [CAN 공통](../../functions/CAN_COMMON_CODEC.md)에서 독립 검사 |
| M06 UART 불완전/길이/CRC/조립 기한 오류 | 아직 정상 Frame 아님. 신뢰 못 하는 후보 시작+1부터 보존 suffix 재탐색 | [Stream](../../functions/UART_FRAME_STREAM.md#stream)의 다음 완전 검증 경계; 잃은 bytes 복원 주장 없음 |
| CRC 정상 완전 Frame의 enum/Session admission 거부 | 전체 Frame 경계를 소모, payload 내부 SYNC 재해석 금지. C2 호출 전 거부이므로 Manager 미갱신 | 다음 Frame 독립 검증; 이 거부를 센서 정상0·위험CLEAR로 바꾸지 않음 |
| CRC 정상 Frame의 실제 소비 호출 뒤 실패 | Frame 경계는 소모하지만 Core/Observer의 확인된 반영 단계는 보존. Manager 불변으로 단정하지 않음 | 이미 반영한 Core를 재실행하지 않으며 반영 여부 미확인은 재호출 HOLD |
| Driver RX overflow | 실제 손실 관측, UART 미완성 후보 무효화 | 실제 저장소·참조 상태를 확인하고 남은 신뢰 경계에서 재동기화. overflow 단위/용량 OPEN |
| M04 Queue full/사본 또는 게시 실패 | 새 입력 전체 거절. 기존 discrete·Query 사본 불변, 실패 caller pointer 보관 없음 | 실제 반환된 공간과 원 문맥/AGE를 확인한 새 제출; 자동retry/덮어쓰기 없음 |
| M04 미시작 만료/문맥 무효 | 원 항목의 전송 자격 제한, 아래3reason 범위만 후발 인계 | 새 유효 원 입력/Owner 정책 필요. 원 AGE0·새 명령ID 발급 없음 |
| M08 accepted 이후 오류/terminal 불명 | 참조·결과를 독립 유지. 미분류 in-flight 오류를 제출실패로 재명명하지 않음 | 실제 Driver 최종 결과·reference·원 token/epoch와 원 제출 연결 |
| C2 Core 반영 후 Observer 실패 | 원 반영 단계 보존. 공개 Rx 전체 자동 재호출 없음 | 같은 원 입력의 Core/필수 소비 확인; partial unknown이면 영향 경로 HOLD |

ISR에서 Frame parsing·DOMAIN Manager·blocking 호출을 실행하지 않는다. 초기 DOMAIN C2는 Superloop의 ISR 밖 단일 담당 문맥에서 호출하며 다른 노드는 각 Owner의 Worker/Superloop 직렬화 계약을 따른다. [등록](../../functions/DOMAIN_REGISTRATION.md)의 실제 지원·수명·필수 sink는 함수주소 non-NULL 또는 IsInitialized 하나로 증명되지 않는다.

<a id="late-failure"></a>
## 후발 실패3범위와 원 identity

기존 공개 원형은 [DOMAIN API](../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxnetworklocalfailure)의 `DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);`다. 본체와 Manager는 DOMAIN 소유다. M03/F09는 로컬 성공 접수된 COMMAND/REQUEST_RESULT/OCCURRENCE에 대해서만 원 identityGeneration·존재 플래그·commandId/RequestContext/Device/occurrence를 보존해 인계한다.

- `DRIVER_SUBMISSION_FINAL`: 실제 최종 Driver 제출 실패. transient busy·송신 중 오류·callback 부재는 이 reason의 자동 근거가 아니다.
- `EXPIRED_BEFORE_SEND`: 실제 송신 시작 전, 채택된 원 기한 위반이 확인됨. 미합의 숫자나 송신 중 timeout에는 적용하지 않는다.
- `CONTEXT_INVALIDATED_BEFORE_SEND`: 실제 송신 전 그 항목의 원 문맥 무효화. 다른 peer/현재 App의 변화로 모든 과거 결과를 무효화하지 않는다.

정확 열거자·DTO 구성 분기는 [NetSvc_MakeLocalFailure](../../functions/NETWORK_FAILURE_HEALTH.md#make-failure), 한 번의 인계·원 C2반환 기록은 [NetSvc_DeliverLocalFailure](../../functions/NETWORK_FAILURE_HEALTH.md#delivery) 정본이다. Query 자체·일반 주기손실·CRC 거부·optional Observer 오류를 이 DTO로 위조하지 않는다. 이미 DOMAIN DISPATCHED인 항목을 Network 송신 전이라는 이유로 AbortBeforeDispatch로 되돌리지 않는다. Driver 실패에서 Vehicle FAILED나 새 M_RESULT를 만들지 않는다.

<a id="lifetime"></a>
## terminal·release·필수 소비의 연결 의사코드

기존 `NetSvc_ClassifyRetirement(const NetSvcLifetimeView *view, NetSvcRetirement *outActions)`를 그대로 재사용한다. 정확한 반환형은 `NetSvcResult`, 전체 조건·Data는 [수명 정본](../../functions/NETWORK_SERVICE_LIFETIME.md#retirement)에 있다. 아래는 기존 M07/M04 호출 지점의 C형 코드 조각이며 새 helper나 public API가 아니다.

`view`는 동일 token/epoch 시도의 직렬화된 실제 원장 사본이며 호출 중 불변이다. `actions`는 비중첩 caller 저장소다. 아래 bool 세 개는 로컬 행동 선택 결과만 나타내고 실제 free/예약완료 flag가 아니다. 팀 승인·reference·terminal 사실을 임의 true로 넣지 않는다.

```c
NetSvcRetirement actions;
bool requestFrameReturn = false;
bool requestReservationEnd = false;
bool requestIdentityRetirement = false;
NetSvcResult classification = NetSvc_ClassifyRetirement(&view, &actions);
if (classification == NET_SVC_OK) {
    requestFrameReturn = actions.mayReturnFrameMemory;
    requestReservationEnd = actions.mayEndSourceReservation;
    requestIdentityRetirement = actions.mayRetireIdentity;
} else {
    /* actions는 읽지 않는다. 세 행동 요청은 false를 유지한다.
       원 classification과 view를 실제 기존 관측 경계에 남긴다. */
}
/* 각 선택을 실제 저장소/예약/identity 정리에 연결하는 구현은 별도 바인딩이다.
   해당 작업이 끝난 뒤에만 같은 Owner 원장을 갱신한다. 재송신은 허가하지 않는다. */
```

| 이전 상태 → 관측 | 정본의 가능한 행동 | 보존할 결과·부작용 |
|---|---|---|
| 참조 POSSIBLE, terminal TX성공, release 미도달 | Frame 반환 불가 | 실제 TX 결과는 기록 가능. 참조 중 Frame 수정/재사용 없음 |
| 참조 ENDED, terminal UNKNOWN, 원 예약 RESERVED | Frame 반환만 가능할 수 있음 | 결과·예약·identity 별도 저장 유지, End성공/새재송신 없음 |
| terminal 확인, 필수 소비 WAITING | 결과 인계 확인을 기다림 | counter 또는 C2 boundary OK만으로 consumer CONFIRMED 금지 |
| terminal·필수 소비 확인, Frame 반환 완료, 예약 RECONCILED | 정본 조건을 만족할 때 identity 정리 가능 | 실제 정리와 원장 반영이 끝나기 전 완료 flag 변경 없음 |
| 과거epoch callback 또는 상반된 terminal | 현재 원장 불변 또는 충돌 HOLD | 새 buffer 해제·새 Query count·현재 명령 완료 없음 |

S4C의 “결과와 참조를 각각 확인” 설명은 B15/B16에서 Frame 메모리·source 예약·identity의 세 수명으로 구체화되었다. release가 먼저 왔다는 이유로 모든 기록을 지우지 않으며, 결과를 기다린다는 이유만으로 이미 참조가 끝난 Frame의 조건부 반환을 금지하는 새 규칙도 만들지 않는다.

<a id="observed-health-callsite"></a>
### 기존 M09 관측과 Health 공급의 명명된 호출 위치

`COMMON_FAILURE_RECOVERY / SC18 M09 F24`가 기존 parser/Driver의 동일 원 사건을 받아 [NetSvc_CountObservedFault](../../functions/NETWORK_FAILURE_HEALTH.md#counters)를 호출한다. 이후 `COMMON_FAILURE_RECOVERY / M09 F25`의 Health 공급 단계가 [NetSvc_BuildHealthView](../../functions/NETWORK_FAILURE_HEALTH.md#health)를 호출한다. 실제 관측 공급 함수명·Task·CommunicationMonitor 연결은 BINDING_OPEN이며 아래는 새 공개 API·Queue·저장소가 아니다.

`healthView`는 이 호출의 `NetSvcHealthView` 안정 사본, `healthNow`는 같은 로컬 시간 기준의 `NetSvcClockStamp`이며 위 수명 전이의 `NetSvcLifetimeView view`와 별도 입력이다.

```c
// COMMON_FAILURE_RECOVERY / M09 F24: 같은 원 사건 journal과 counters의 직렬 문맥
NetSvcResult counted = NetSvc_CountObservedFault(
    observedLink, observedEvent, agreedIncrement, &eventAlreadyCounted, &counters);
if (counted == NET_SVC_OK) {
    // M09가 counter와 동일 사건 집계 여부를 함께 게시한 사실만 소비한다.
} else {
    // DUPLICATE를 포함한 비OK에서는 두 저장소 불변. 원 원인/단위를 보존한다.
}

// COMMON_FAILURE_RECOVERY / M09 F25: 원자 view와 같은 로컬 clock의 불변 사본
NetworkHealthSnapshot_t built;
NetSvcResult builtStatus = NetSvc_BuildHealthView(&healthView, &healthNow, &built);
if (builtStatus == NET_SVC_OK) {
    // 기존 DOMAIN CommunicationMonitor 공급 경계가 built를 복사해 소비한다.
    // 실제 공급 바인딩이 없으면 전달 완료를 주장하지 않는다.
} else {
    // built는 읽지 않는다. 원 healthView/clock과 제한 원인을 보존한다.
}
```

`eventAlreadyCounted`는 새 지역 bool이 아니라 기존 동일 사건 원장의 집계 필드를 뜻하며 counter와 비중첩이어야 한다. callback 반복을 새 사건으로 세지 않는다. `healthView/healthNow/built`는 비중첩이고 지연 소비자는 반환 전에 복사한다. Health 복사 성공은 차량 가용성·특정 peer 생존·C2 Getter 성공을 뜻하지 않는다. 공개 Getter 실패 OUT은 undefined인 기존 계약을 유지한다.

<a id="health"></a>
## Health와 복구 완료의 한계

`DomainIf_GetNetworkHealthSnapshot(NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot)`의 정확 반환형은 `DomainIf_Status_t`이며 [기존 Health](../../functions/NETWORK_FAILURE_HEALTH.md#health)를 따른다. Getter 성공은 UNKNOWN/UNAVAILABLE Snapshot도 반환할 수 있다. 실패 OUT은 읽지 않고 hasValidRx=false의 age0은 fresh 근거가 아니다. 다섯 counter는 실제 단일 관측·정해진 단위로 u32포화 집계하며 정상 수신·peer reboot·Query 완료로 reset하지 않는다.

SC17의 복귀 후 새 Query/상태와 SC18의 다음 정상 Frame은 통신·자료 처리의 재접수 조건이다. 차량 위험 해제·BCM 동작 완료·VSS 재생·WINDOW 활성·App 표시 완료를 보증하지 않는다. IF-O12/14/09/25·CF01/CF02와 G56 외부 HOLD를 유지한다. 이 문서의 반례 판정은 정적 설계 검토이며 제품/Driver/통신 시험은 NOT_EXECUTED다.
