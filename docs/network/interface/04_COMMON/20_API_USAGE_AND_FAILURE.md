# API 사용·반환·실패 계약

Interface v2 · r1 · [규격과 연동 조건](../07_CROSS_NODE_INTERFACE_RULES.md#status)

**PROPOSED_DESIGN / BINDING_OPEN.** 정확한 기존 반환·원형은 [DOMAIN](../02_NODES/DOMAIN/20_API.md), [ESP32](../02_NODES/ESP32/20_API.md)의 DECLARED_SOURCE를 따른다. 신규 노드 API는 제안이다.

[시작 안내](../00_START_HERE.md) · [공통 Data](10_DATA_AND_ID_RULES.md) · [메시지별 상대 API](../05_MESSAGE_INDEX.md)

바로 찾기: [호출 방향](#section-local-boundary) · [로컬 반환·C2 변환](#section-return-status) · [Gateway 반환 후보](#section-gateway-status) · [포인터 수명](#section-ownership) · [보관·재전송](#section-lifetime-ownership) · [Query 종결](#section-query-boundary) · [Health·수용 뒤 실패](#section-network-health).

<a id="section-local-boundary"></a>

**Status/Fault의 저장·전달 경계:** DOMAIN Core commit 이후 optional Observer 오류는 완료한 저장을 재실행할 근거가 아니다. 단일 원 관측의 raw·Meta·품질 연결에는 기존 원자적 수용 규칙을 유지한다. 서로 다른 Status/Fault 갱신 사이의 원 순서·부분 저장 성공/실패를 어떻게 관측하고 반환할지는 DOMAIN 확인 대기이며, 일부 성공을 전체 성공 또는 전체 rollback으로 추정하지 않는다. 원 식별·품질·AGE와 완료한 범위를 구분해 인계하고 실제 Manager/Observer 연결 검증 전에는 통합 완료로 처리하지 않는다.

## 어느 함수에 무엇을 전달하는가

공개 `DomainIf_*` 72개, Lifecycle·Rx/Tx 래퍼·Query Getter 18개와 기존 Manager 연결은 DOMAIN이 구현한다. Network는 TxPort Callback 26개·Codec·Queue·Adapter·실제 송수신과 통신 원자료를 제공한다. `DomainIf_SetTxPort()` 함수 자체는 DOMAIN 소유다. 지원하지 않는 Port를 성공으로 처리하지 않는다. 실제 Port·Manager 연결은 별도로 검증해야 한다. [전체 범위](../07_CROSS_NODE_INTERFACE_RULES.md#roles)

Application→Network TX 함수는 기능 owner가 만든 의미값·원식별·quality·AGE를 접수한다. Network→Application RX 함수는 Codec·기동·순서·문맥 검증 후 typed 자료를 빌려 전달한다. 실제 상대 노드에는 메시지를 보내며 상대 C 함수를 직접 호출하지 않는다. Codec·HAL/SDK·Link worker 배치를 API 이름에서 추정하지 않는다.

DOMAIN은 제공 C2 `DomainIf_*`72 선언/TxPort26 callbacks를 사용하고 내부 Adapter가 필요한 변환만 수행한다. ESP32는 Gateway13 선언을 사용하고 내부 message dispatch로 subtype을 선택한다. 신규 공개 `DomainNet_*`/`Esp32App_On*` 병렬 계약을 만들지 않는다. C2 QueryGet18은 DOMAIN 캐시 사본 Getter이며 원격 ECU RPC가 아니다.

<a id="section-return-status"></a>
## NetSubmitResult — local 인계 범위

전체 선언: [NetSubmitResult](10_DATA_AND_ID_RULES.md#type-netsubmitresult).

| 반환 | 호출 이후의 의미 |
| --- | --- |
| `NET_SUBMIT_OK` | caller 저장소와 무관한 안정된 bounded 인계 완료. 실제 TX/peer receive/ECU ACCEPTED/DONE 아님 |
| `NET_SUBMIT_INVALID_ARGUMENT` | NULL·예약 코드·범위·필드 조합 오류. 새 항목 접수 없음 |
| `NET_SUBMIT_NOT_READY` | Link/현재 문맥/필수 owner가 해당 제출 준비 안 됨. 새 항목 접수 없음 |
| `NET_SUBMIT_NO_CAPACITY` | 필요한 로컬 공간 없음. 기존 discrete 항목 덮어쓰기 없음 |

INVALID/STALE/NO_DATA라는 **유효한 품질 보고**는 인자 오류가 아니다. 정당한 열화 상태 보고를 거부해 정상인 것처럼 숨기지 않는다. Return은 차량 Result Wire에 넣지 않는다.

### C2 TxPort 내부 반환 변환

| NetSubmitResult | C2 `DomainIf_Status_t` | 의미 |
| --- | --- | --- |
| OK(0) | `DOMAIN_IF_OK`(0) | stable local 인계 완료 |
| INVALID_ARGUMENT(1) | `DOMAIN_IF_INVALID_ARGUMENT`(1) | 인자/표현 오류 |
| NOT_READY(2) | `DOMAIN_IF_NOT_AVAILABLE`(3) | local 전송 경로·binding 준비 안 됨 |
| NO_CAPACITY(3) | `DOMAIN_IF_QUEUE_FULL`(4) | local 보관 용량 없음 |

`DOMAIN_IF_NOT_INITIALIZED`(2)/REJECTED(5)/UNSUPPORTED(6)는 기존 C2 자체 초기화·의미 거부·지원 판단이다. NetSubmitResult에 억지 대입하지 않는다. source enum숫자끼리 cast하지 않는다.

<a id="section-gateway-status"></a>
### Gateway 반환 — PROPOSED

**NETWORK_OWNED / CROSS_NODE_AGREEMENT / IMPLEMENTATION_BINDING :** Gateway의 기존 반환값을 그대로 사용한다. 안정된 전체 인계만 `GATEWAY_STATUS_OK`(0), 포인터·방향·schema/문맥 필드의 형식 불일치는 `INVALID_ARGUMENT`(1), 현재 경로·인증 Session 준비 안 됨 또는 일시적 로컬 용량 부족은 `NOT_READY`(2), 정의되지 않은 subtype/지원하지 않는 의미는 `UNSUPPORTED`(3), 준비된 경로의 예기치 않은 내부 처리 오류는 `INTERNAL_ERROR`(4)를 권장한다. 용량 부족의 실제 원인은 내부 진단에서 보존하며 `NOT_READY`가 차량 거부·실행 실패를 뜻하지 않게 한다. 조건 확정 전 반환 바인딩은 OPEN이다. 부분 접수 후 실패 반환은 금지하고, 실패 시 호출자 소유 저장소를 비동기 보관하지 않는다. ESP32 Gateway owner는 기존 13개 함수·transport port가 같은 의미를 적용하는지 확인한다.

<a id="section-ownership"></a>
## 포인터 수명과 호출 시점

초기화는 DOMAIN Manager → C2 Init → 필요한 Observer → Network TxPort 등록 → 통신 시작 순서를 따른다. 실제 등록 실패·재초기화·호출 Queue는 연결 시 검증한다. C2 Rx/Query의 직렬 Task 문맥을 CIS·Pi 전체에 동일 RTOS를 강제하는 조건으로 확대하지 않는다.

TX const 포인터는 호출 중 유효하면 된다. OK 반환 전 Network가 bounded 값 복사 또는 Network-owned frame 동기 encode를 마친다. 호출자 포인터·문자열·외부 버퍼 주소를 비동기 보관하지 않는다. 동기 encode도 실제 송신 전 AGE/CRC를 갱신해야 한다. 실패 시 partial admission을 만들지 않는다.

RX message/meta 포인터는 callback 반환까지 읽기 전용이다. 이후 처리에 필요하면 값·identity·수신시각을 함께 복사한다. Getter out 객체는 호출자 소유이며 성공일 때만 사용한다. callback은 owner 처리 문맥에서 호출한다. S32K C2는 Task-only이고 새 노드 PROPOSED는 superloop owner도 가능하다. ISR에서 parsing·차량 기능 판단을 수행하지 않는다. multi-task 동시 안전성을 이 문서로 보장하지 않으며 실제 Task/Queue/priority/capacity는 OPEN이다.

<a id="delta-rx-completion"></a>
** 환경 저장 완료는 별도 확인:** TX의 local OK나 callback Queue 접수 의미로 DOMAIN VSM 무효화 완료를 판단하지 않는다. 구 BOOT 콜백 반환 후에도 비동기 후속 쓰기가 남아 있으면 새 정상 관측을 전달할 조건이 충족되지 않는다. [기존 환경 Rx의 실제 commit·완료 판정](../02_NODES/DOMAIN/20_API.md#delta-environment-completion)은 DOMAIN 기능 정책·연동 검증 전이며 공개 반환값에 새 의미를 부여하지 않는다. 무효화 실패·미확인에서 새 정상 전달 차단과 구 VSM 정상값 무효화 성공은 별개다. Core commit 뒤 optional Observer 실패 때문에 이미 끝난 저장을 재실행하지 않는다. 비차단 owner 호출·포인터 수명은 그대로 유지한다.

<a id="section-lifetime-ownership"></a>
## 보관·재전송·차량 결과

통신 처리 규칙 — 같은 수용 BOOT에서 같은 전송 순번·같은 raw인 정확한 중복은 수신감시를 갱신하지 않는다. 새 TX_SEQUENCE/LINK_SEQUENCE로 수용한 정상 반복은 같은 원 관측이라도 기존 수신 콜백으로 전달하여 기능 수신감시를 갱신한다. 원 생성시각·관측번호는 유지하고 현재 AGE는 경과에 따라 증가한다. 콜백 생략은 별도 감시 경로가 같은 수용 사실을 전달한다는 연결 증거가 있을 때만 허용한다. CRC/길이/코드 오류와 구세대는 감시 근거가 아니다. UART-C Pi Result/Status는 기존 두 CisApp 수신 콜백의 감시 연결을 따른다. CAN에서 같은 원 관측·같은 값·같은 품질인 정상 반복은 감시/AGE에 반영하되 C2의 새 관측으로 재발행하지 않는다. 확인된 품질 저하는 품질 저하 인계 분기로 별도 처리한다.

통신 처리 규칙 — 같은 원 관측의 확인된 품질 저하는 기존 C2 수신 경로에 전달하되 원값·원 BOOT/관측번호는 바꾸지 않는다. 높은 TX만으로 값 변경이나 INVALID→VALID로 복원하지 않는다. raw·Meta·품질·순서의 수용 연결은 함께 commit하고 저장/인계 실패 시 부분 commit을 남기지 않는다. 원 생성 기준점은 유지하고 현재 AGE만 경과에 따라 증가한다. 실제 DOMAIN 저장·Getter 연결이 검증되기 전에는 연결 완료라고 하지 않는다. [품질 전이 정본](10_DATA_AND_ID_RULES.md#section-repeat-and-quality)을 따른다.

| 종류 | 보관/반복 |
| --- | --- |
| Current State/Warning | 같은 의미 key의 아직 TX 시작 안 된 pending만 최신값 교체 가능 |
| Command/Result/Event | identity·내용·필요 순서 보존. silent overwrite 금지 |
| Query 응답 | 현재 Device/Session/Domain Boot/Query ID에 묶인 순서 있는 사본. 자발 갱신에 덮이지 않음 |
| 과거 결과/발생 이력 | 기존 Application owner가 보존. Network pending을 별도 차량 이력DB로 확장하지 않음 |

같은 명령 제한 재송신은 commandId/원본문 고정, AGE와 frame sequence만 갱신한다. 같은 identity인데 본문이 바뀌면 새 실행하지 않고 충돌로 기록한다. 새 기동/세션·link recovery가 과거 Command/Event 자동 실행 근거가 아니다. 현재 상태가 목표와 같다는 이유만으로 과거 요청 DONE을 합성하지 않는다. 확정 terminal 이후 늦은 ACCEPTED/IN_PROGRESS로 회귀하지 않는다. UNKNOWN/UNCONFIRMED는 FAILED와 다르다.

CAN 명령은 50 ms 후 최대 1회, Result/Event는 20/40 ms 후 최대 2회 보완하는 PROPOSED 후보이다. 수용 200 ms↔구 SysRS 1 s 충돌은 CONTRACT_OPEN이다. 응용 ACK 무한 재전송·새 Heartbeat를 추가하지 않는다.

<a id="r1-command-deadline"></a>
**기한 충돌:** 200ms는 Network 수용 기한의 보수적 검증 후보이고 구 SysRS 1초는 별도 요구다. producer의 원 명령 생성, Network 로컬 접수, ECU 수용의 시점을 각각 기록하여 어느 구간에 전송·큐 대기가 포함되는지 대조해야 한다. 공통 출발점과 배포 deadline은 미확정이다. 출발점·포함 구간·최종 수용 deadline에 대한 DOMAIN/BCM 확인 전까지 POLICY_REVIEW를 유지하며, 200ms로 SysRS를 자동 개정하거나 1초로 만료 명령을 되살리지 않는다.

<a id="section-query-boundary"></a>
## Query·private 제어

최근 요청 결과 Query에 한해 DOMAIN은 Device별 최근 최대 8건을 보관하고 최신순 최대 5건을 제공한다. Network는 기존 `M_RESULT`를 기록당 한 번씩 순차 전송한 뒤 `M_QUERY_END`로 끝낸다. 이 5건 제한을 전체 상태 Query의 다른 응답까지 합친 총량 제한으로 사용하지 않는다. 내부 동일 요청 갱신 방식과 표시의 중복 제거 요구, Query 종료 count의 전송 완료 기준은 각각 구분한다. 이력 8/5 규칙과 아래 ITEM_COUNT 후보·MOBILE 수신 완료 조건은 별개다. [규격과 미확정 조건](../07_CROSS_NODE_INTERFACE_RULES.md#query)

M_QUERY는 passive DeviceContext/u64 Session 검증 후 기존 Getter로 사본을 얻으며 차량 실행을 만들지 않는다. SESSION_READY=0인 최초 조회에도 현재 등록·M_CONTEXT/문맥 검증이 필요하다. 같은 Query 사본의 의미·quality·identity는 고정하고 AGE만 누적한다. 새 조회는 최신 Snapshot을 얻을 수 있다. 자발 발행은 query_id0, 조회 응답은 원 query_id이며 같은 projection/source API와 별도의 내부 순서·count 문맥을 사용한다.

**M_QUERY_END는 Network-private 조회 종료다. PROPOSED · NETWORK_OWNED / CROSS_NODE_AGREEMENT / IMPLEMENTATION_BINDING :** ITEM_COUNT는 같은 Device/Session/DOMAIN Boot/Query ID에서 **성공한 최종 로컬 UART TX를 실제 증명한 응답 frame 수**다. 확정 실패 terminal, End, 자발 갱신, 접수만 성공한 항목, 재전송 복제는 제외한다. 모든 예정 응답이 TX 성공 또는 확정 실패 terminal에 도달한 뒤에만 앞 응답과의 순서를 지켜 End를 제출한다. 아직 pending인 응답이 있으면 End를 보류한다. 품질 UNCONFIRMED인 응답이라도 실제 TX 성공이 확인되면 한 응답으로 셀 수 있지만, 송신 결과 불명은 성공 count에 넣지 않는다. 응답별 item 번호가 없는 기존 Wire에서 결과 불명 전송을 같은 Query에서 재시도해 count를 늘리지 않는다. TX 완료 관측이 없으면 count 완전성은 **BINDING_OPEN**이며 완료0을 만들지 않는다.

QUERY_STATUS 완료0은 예정 범위의 누락·변환·송신 실패가 없다는 근거가 있을 때만 사용한다. 확인된 누락·부분 실패는 일부 확인 불가2, 확인된 요청 결과 기록 부재는 상태1/count0으로 구분한다. Getter 실패·미확인 0건은 완료0이 아니다. ESP32는 현재 문맥·조회 범위·실제 검증한 응답 수와 End count/status를 대조한다. 중복·지연·문맥 변경으로 비교 근거가 부족하면 미완료로 남기며 받은 항목의 원 품질·AGE를 유지한다. End는 App 전달 완료·목록 전체 최신성·차량 실행 DONE을 증명하지 않는다.

양단 처리·실패·0건 범위의 정본: [DOMAIN Query 처리](../02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#query-flow) · [DOMAIN TX 완료·종결 권장안](../02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer) · [ESP32 수신·BLE 종결 권장안](../02_NODES/ESP32/25_PRIVATE_ROUTING.md#r2-query-consumer). TX 완료 관측·순서·수신/BLE 연결은 OPEN이다.

M_DOMAIN_ALIVE는 **Network-private UART 경로 감시**다. BLE/App으로 원프레임을 중계하지 않는다. ESP32 SessionReady와 App 실제 목록/count 완료 판단은 별도다. WARNING_ACK/READ는 위험CLEAR 또는 VSS음향STOP이 아니다.

<a id="section-network-health"></a>
## Health·수용 뒤 실패

접수 전 INVALID_ARGUMENT/NOT_READY/NO_CAPACITY 반환은 미접수이며 그 항목의 후발 실패 사건을 생성하지 않는다. 정상 접수한 필요한 명령·결과·일회성 이벤트만 아래 기존 LocalFailure 세 reason의 사건 범위로 인계한다. 주기 상태·Query 실패는 각 통신/Query 경로를 사용하고 차량 실행 FAILED로 자동 변환하지 않는다. CAN 송신 성공 공개 Callback이나 CIS·Pi용 동일 C2 API를 새로 추가하지 않는다.

정상 RX/liveness·CRC/format reject·overflow·bus-off·송신 실패는 Network 원천이고 기능 상태/고장 여부는 기존 기능 owner가 해석한다. 공유 CAN bus-wide RX가 모든 ECU의 생존을 보장하지 않는다. DOMAIN raw Health Snapshot→CommunicationMonitor→DiagnosticManager/Availability 흐름을 따른다.

<a id="r1-local-failure-reasons"></a>
**접수 후 실패의 닫힌 범위 :**

| 기존 reason | 필요한 실제 사건 | 해당하지 않는 사례 |
|---|---|---|
| `NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL` | accepted 항목의 driver 제출 시도가 확정적으로 최종 실패함 | 일시 busy, 아직 재시도/제출 대기, 제출 뒤 미분류 in-flight 오류 |
| `NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND` | accepted 항목이 실제 송신 전에 기한 만료됨 | 이미 송신된 항목에 뒤늦게 만료를 붙이는 경우 |
| `NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND` | accepted 항목이 실제 송신 전에 원 문맥 무효화됨 | 송신 이후의 임의 문맥 상실 또는 callback 부재 |

admission 전에 필요한 기존 `DomainIf_RxNetworkLocalFailure` core sink의 준비를 확인하고 원 kind·generation·Command/RequestSession/Occurrence 식별자를 보존한다. 진단 observer만 준비된 것을 core 인계 완료로 보지 않는다. 미분류 in-flight 오류와 완료 callback 부재는 원 식별자·현재 송신 phase·실제 진단을 남기고 영향 경로를 HOLD한다. 이 경우의 의미 연결은 미결이며 세 reason 중 하나를 추정하지 않는다. 차량 FAILED/DONE 합성, 무조건 재전송, 이미 DISPATCHED인 명령의 AbortBeforeDispatch 및 observer 실패로 core 결과 재적용을 금지한다. 실제 sink/driver 연결은 BINDING_NOT_VERIFIED다.

**PROPOSED · NETWORK_OWNED / FUNCTION_OWNER / IMPLEMENTATION_BINDING :** Network는 실제 관측한 link별 오류·정상 수신 시각을 기존 Health 자료에 기록하고 기능 owner가 도어·센서·음향의 사용 가능성을 판단한다. CAN의 임의 ECU frame을 받은 사실을 특정 ECU 생존이나 센서 정상의 증거로 쓰지 않는다. C2 local failure는 원 kind·generation·commandId/RequestContext/Occurrence를 유지한 채 기존 세 실패 사유에 정확히 맞는 경우만 사용한다. Query 자체의 송신 실패에는 이 구조에 Query ID/kind가 없으므로 차량 Command/Result 실패를 꾸며 전달하지 않는다. Query 내부 순서·count 관측과 기존 transport 오류 진단으로 처리하며, 필요한 구현 관측 지점은 별도로 바인딩한다. 이는 새 공개 failure API나 차량 Fault 종류를 추가하는 권고가 아니다.
