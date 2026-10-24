# DOMAIN 등록 — C2 관리 6개와 실행 준비

[개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [TxPort26](DOMAIN_TX_PORT.md) · [공통 수명](NETWORK_SERVICE_LIFETIME.md) · [실패·Health](NETWORK_FAILURE_HEALTH.md) · [기존 M03](../modules/M03_TYPED_HANDOFF.md)

공개 관리 함수 6개의 본체는 DOMAIN 소유다. Network는 자신의 콜백과 통신 관측을 기존 C2 경계에 연결한다. 이 문서는 공개 C2 본체, 새 전역 Service Manager, 차량 상태 cache를 작성하지 않는다. `DomainIf_Status_t` 원 값은 [기존 선언](../data/CIS_REAR_DATA.md#declared), 등록 table과 Health 자료는 [Interface 정본](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md)에 보존한다.

원형은 `DECLARED_SOURCE`, 아래 Network 내부 연결 예는 `PROPOSED_PRIVATE_FUNCTION`, 실제 초기화 owner·Task·호출 순서·재진입·해제·Driver 주소는 `BINDING_OPEN`이다. S4A `api/domain_management.md`, `api/common.md`와 B14 Interface `20_API.md`를 대조했다. D-03/D-05/G06/IF-O12 및 기존 외부 HOLD를 유지한다.

<a id="readiness"></a>
## 하나의 ready 값으로 합치지 않는 조건

| 조건 | 확인할 실제 근거 | 충족해도 알 수 없는 사실 |
|---|---|---|
| C2 초기화 | DOMAIN 초기화 owner의 Init 결과와 IsInitialized 반환 | CAN/UART·상대 ECU·차량 기능 준비 |
| TxPort 등록 | SetTxPort 실제 반환, 26개 중 해당 route의 함수 주소/지원 확인 | 코드가 존재한다는 것만으로 Queue·Driver·외부 Wire 채택이 완료되지는 않음 |
| Rx 소비 준비 | DOMAIN owner 문맥, 해당 typed 소비자 지원, 반영 단계/오류 관측 경계 | Core commit·Observer 성공·같은 원 Snapshot 증거는 각각 필요 |
| 송신 자원 | 자기 소유 사본, 원 identity/AGE 기준, bounded 슬롯의 원자 게시 | Driver accept·terminal·reference release |
| Driver/Port | M08 실제 장치 binding과 profile·기동·전송 관측 계약 | 상대 ECU ACK/기능 DONE 또는 source 원 관측의 freshness |
| 필수 실패 인계 | identity 있는 discrete 항목을 소비할 기존 DOMAIN Manager/Supervision 경계 | optional RxObserver 등록이 이를 대신할 수 없음 |
| Observer | DOMAIN/기능 통합 owner가 결정한 소비자·등록·함수 수명 | Network가 11개 본체를 소유하거나 Core를 재실행할 권한 |

초기화의 보수적인 **선후 제약**은 “owner 초기화 확인 후 C2 호출”, “자원/필수 sink/route 준비 확인 후 해당 송신 접수”, “검증된 Rx를 owner 문맥에 인계한 후 실제 commit/관찰 결과 기록”이다. 초기 연결의 합의된 상위 순서는 DOMAIN Manager 준비→C2 Init→Observer 등록→TxPort 등록→통신 시작이다. DriverInit·IRQ 활성화·SDK별 세부 초기화 순서는 실제 장치 바인딩에서 별도로 확인한다. FreeRTOS Task와 Superloop의 실제 직렬화 방식은 M08 Port 바인딩에 남긴다. ISR은 안정된 장치 관측 확보까지만 담당한다.

<a id="DomainIf_Init"></a>
## 1. DomainIf_Init — DOMAIN 논리 경계 초기화

```c
/* DECLARED_SOURCE / DOMAIN_OWNED */
DomainIf_Status_t DomainIf_Init(void);
```

인자/OUT은 없다. DOMAIN 초기화 담당자가 로컬 시작 조건을 충족한 문맥에서 호출한다. L02/M03/F06/ST04의 **연결 책임**이며 Network가 공개 본체를 구현한다는 뜻이 아니다. OK는 C2 논리 초기화 단계만 뜻한다. 실패한 초기화 이후 부분 상태, 재호출의 멱등성, 재시작 시 table 유지/초기화 여부는 실제 C2 구현 근거를 요구한다.

```c
// DOMAIN 초기화 owner의 호출 예. Network가 반복 호출하는 loop가 아니다.
DomainIf_Status_t initialized = DomainIf_Init();
if (initialized != DOMAIN_IF_OK) {
    // 원 반환과 확인된 초기화 단계를 보존한다.
    // 실패를 무시하여 정상 C2 접수/필수 sink 준비를 선언하지 않는다.
}
```

Network 재접속이나 한 CAN peer의 reboot를 C2 전체 Init 재호출 이유로 삼지 않는다. 원 명령·미결 전송·Driver 참조가 남은 동안 논리 초기화로 메모리 해제를 가정하지 않는다. [정확한 원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_init).

<a id="DomainIf_IsInitialized"></a>
## 2. DomainIf_IsInitialized — 로컬 상태 확인

```c
/* DECLARED_SOURCE / DOMAIN_OWNED */
bool DomainIf_IsInitialized(void);
```

인자/OUT은 없다. Network Adapter가 C2 수신/등록을 호출할 준비를 확인할 때 사용한다. false이면 그 C2 인계를 진행하지 않는다. true라도 해당 Port 미등록, 실패 sink 부재, 대상 Boot 불명, 미승인 profile이면 해당 송신은 NOT_AVAILABLE이다. 다른 확인된 경로의 자료까지 무효화하지 않는다.

```c
bool c2Initialized = DomainIf_IsInitialized();
if (!c2Initialized) {
    // 해당 C2 경계 호출을 제한하고 원 수신/송신 근거를 기존 제한 수명에 보존한다.
    // 함수 반환의 false를 차량 UNAVAILABLE 또는 특정 ECU 단절로 변환하지 않는다.
}
```

L02/M03/F06/ST04. 검사와 후속 호출 사이 reset 가능성이 있으면 owner 직렬화/수명 보호가 필요하며 bool을 읽었다는 사실만으로 동시성 안전을 증명하지 않는다. [원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_isinitialized).

<a id="DomainIf_SetTxPort"></a>
## 3. DomainIf_SetTxPort — Network 콜백 등록 요청

```c
/* DECLARED_SOURCE / DOMAIN_OWNED */
DomainIf_Status_t DomainIf_SetTxPort( const DomainIf_TxPort_t *port);
```

`port`는 IN/borrowed `const DomainIf_TxPort_t *`다. [원 table](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-txport-t)에는 26개의 함수포인터만 있고 Gateway의 user_context 필드를 추가하지 않는다. source 설명의 table 값 복사와 실행 함수/상태의 수명 보장은 다르다. 함수 주소와 해당 Network 자원은 등록 기간 및 진행 중 호출이 종료될 때까지 유효해야 한다. stack table을 복사했다는 이유로 함수가 참조하는 상태를 해제하지 않는다.

Network Adapter 등록 담당자가 DOMAIN 본체에 요청한다. 현재 Production `DomainIf_SetTxPort(NULL)`은 초기화 이후 등록 Port table을 해제하고 `DOMAIN_IF_OK`를 반환한다(초기화 전 `DOMAIN_IF_NOT_INITIALIZED`). 다만 Network가 오류 복구를 위해 임의로 unbind·Init 재호출할 수 있다는 허가는 아니다. NULL인 개별 callback은 전체 table NULL과 다르며 미연결 경로는 NOT_AVAILABLE로 남긴다. 구현 보류 WINDOW·CF02 경로를 정상 성공하는 no-op 함수로 채우지 않는다. 26개 선언이 존재한다고 26개 route를 모두 활성화해야 하는 것은 아니다.

```c
/* PROPOSED_PRIVATE_FUNCTION / BINDING_OPEN */
DomainIf_Status_t NetDomain_RegisterTxPort(const DomainIf_TxPort_t *port)
{
    if (port == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    if (!DomainIf_IsInitialized()) return DOMAIN_IF_NOT_INITIALIZED;
    // 같은 owner 직렬 문맥에서 현재 table 교체/호출 중 여부를 확인한다.
    // 각 비-NULL 함수 주소의 수명, 해당 Network 자원, 지원 profile,
    // identity 있는 discrete route의 필수 failure sink 준비 근거가 필요하다.
    if (/* 초기화/교체/실행 문맥·함수 수명 보장을 확인하지 못함 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    // 이 검사는 필요한 경로별 readiness를 기록한다. HOLD 경로를 정상화하지 않는다.
    DomainIf_Status_t registered = DomainIf_SetTxPort(port);
    // 등록 결과를 그대로 보존한다. 실제 지원/반영이 미확인인 경로는 계속 제한한다.
    return registered;
}
```

함수의 IN 수명은 source SetTxPort와 동일하며 OUT은 없다. 이 wrapper는 독립적인 C2 상태를 만들지 않는다. 등록 실패 후 “이전 table은 무조건 그대로다” 또는 “새 table은 일부도 반영되지 않았다”를 source 증거 없이 단정하지 않는다. 실제 반영 상태가 미확인이면 영향 route를 제한하고 확인한다. rollback을 위해 임의 SetTxPort(NULL)/Init 재호출을 하지 않는다. L02/M03/F06/ST04. [원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_settxport).

**명명된 설계 호출자:** [DOMAIN_COMMAND_TO_NETWORK](../flows/DOMAIN_COMMAND_TO_NETWORK.md)의 L02/M03/F06 route 준비 단계가 이 함수를 호출하고, 반환된 원 C2 값을 등록 담당자가 소비한다. 실제 시작 함수명·Task·재등록 동기화는 BINDING_OPEN이며 아래 조각은 새 진입점이 아니다. `ownerPort`는 위 수명과 초기화·교체 조건이 확인된 기존 table의 포인터다.

```c
// DOMAIN_COMMAND_TO_NETWORK / M03 F06 등록 준비 단계
DomainIf_Status_t registrationStatus = NetDomain_RegisterTxPort(ownerPort);
if (registrationStatus != DOMAIN_IF_OK) {
    // 원 반환과 실제 table 적용 상태를 보존하고 영향 route만 제한한다.
    // 이전 table 유지 또는 무반영을 추정하거나 Init/NULL로 rollback하지 않는다.
} else {
    // 등록 요청 반환만 기록한다. route 지원·Driver/실패 sink 준비는 각각 확인한다.
}
```

<a id="DomainIf_SetRxObserver"></a>
## 4. DomainIf_SetRxObserver — DOMAIN/기능 소비자 등록

```c
/* DECLARED_SOURCE / DOMAIN_OWNED */
DomainIf_Status_t DomainIf_SetRxObserver( const DomainIf_RxObserver_t *observer);
```

`observer`는 IN/borrowed `const DomainIf_RxObserver_t *`이고 [11개 원 callback](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-rxobserver-t)을 가진다. 등록 요청 주체는 DOMAIN 통합 owner이며 각 소비자 구현·준비의 실제 담당자는 C2/기능 owner와 확인한다. Network가 자동으로 이 table을 만들거나 11개 본체를 대신 구현하지 않는다. 현재 Production `DomainIf_SetRxObserver(NULL)`은 초기화 후 등록 table을 해제하고 `DOMAIN_IF_OK`를 반환한다(초기화 전 `DOMAIN_IF_NOT_INITIALIZED`). 개별 callback 미연결과는 별개이고, 임의의 초기화/복구 시 호출 권한을 뜻하지 않는다.

```c
// DOMAIN 통합 owner가 승인한 기존 table을 제공한 경우의 호출 예.
const DomainIf_RxObserver_t *ownerObserver = /* owner가 수명을 보장하는 table */;
if (ownerObserver != NULL && DomainIf_IsInitialized()) {
    DomainIf_Status_t observed = DomainIf_SetRxObserver(ownerObserver);
    // 등록 결과와 확인된 table 적용 상태를 기록한다.
    // observed==OK는 NetworkLocalFailure 필수 sink 준비 증거가 아니다.
}
```

Rx Core commit은 optional Observer보다 앞선다. Observer failure만으로 이미 반영된 입력·차량 구동을 다시 실행하지 않는다. Rx 함수가 non-OK를 반환하더라도 실제 Core와 Observer의 어느 단계가 반영됐는지 모르면 재호출을 보류한다. 이 문서는 `DomainIf_Rx*` 본체에서 어떻게 observer를 호출하는지 재구현하지 않는다. L02/M03/F06/ST04. [원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_setrxobserver).

<a id="DomainIf_ValidatePassiveQueryContext"></a>
## 5. DomainIf_ValidatePassiveQueryContext — 조회 문맥 확인

```c
/* DECLARED_SOURCE / DOMAIN_OWNED */
DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);
```

두 인자는 IN 값이다. `DeviceContextId_t`는 u32 원 단말, 현 source `SessionId_t`는 u64다. Network M02/F04/ST03이 선행 M_CONTEXT를 적용한 동일 owner 직렬 문맥에서 현재 조회의 Device/Session을 검증한다. 함수는 Session 발급/기능 활성화/Request 재실행 API가 아니다. 유효한 최초 SESSION_READY=0 조회도 일괄 거부하지 않는다.

```c
DomainIf_Status_t context =
    DomainIf_ValidatePassiveQueryContext(deviceContextId, sessionId);
if (context == DOMAIN_IF_OK) {
    // 같은 owner 직렬 처리에서 scope에 맞는 기존 Getter만 호출한다.
    // Getter 성공 OUT만 사용하고 원 CAN provenance의 같은 관측 결합을 별도로 확인한다.
} else {
    // 원 C2 반환을 보존한다. 거부된 문맥으로 Getter/응답 성공을 만들지 않는다.
}
```

Getter18/schema17/실 final TX count/Query End 순서는 [기존 Query 흐름](../flows/UART_M_QUERY_AND_PROJECTION.md)과 [Query 제어](UART_M_QUERY_CONTROL.md)를 재사용한다. 현재 컨텍스트 확인은 원 기록 접근 권한이나 완전 응답의 증거가 아니다. IF-O25/Q02/Q10/D-05/G06 및 세션 ABI 확인을 보존한다. [원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_validatepassivequerycontext).

<a id="DomainIf_GetNetworkHealthSnapshot"></a>
## 6. DomainIf_GetNetworkHealthSnapshot — 관측된 링크 상태 조회

```c
/* DECLARED_SOURCE / DOMAIN_OWNED */
DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot( NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);
```

`link`와 `nowMs`는 IN 값, `outSnapshot`은 NULL 불가인 caller-owned OUT이다. DOMAIN CommunicationMonitor/DiagnosticManager가 사용하며 M09/F25/ST12가 실제 raw 통신 관측을 공급하는 경계다. Network가 이 공개 Getter나 새 차량 cache를 구현하지 않는다. `nowMs`는 해당 owner의 확인된 monotonic u32 ms이며 원 producer TTL 재정의에 쓰지 않는다.

```c
NetworkHealthSnapshot_t snapshot;
DomainIf_Status_t got =
    DomainIf_GetNetworkHealthSnapshot(NETWORK_LINK_CAN, nowMs, &snapshot);
if (got == DOMAIN_IF_OK) {
    // snapshot.transportState==UNAVAILABLE이어도 Getter 성공일 수 있다.
    // hasValidRx가 false이면 lastValidRxAgeMs를 읽어 fresh로 해석하지 않는다.
    // counters는 관측된 Network 계측이며 차량/특정 peer 생존의 증거가 아니다.
} else {
    // 실패 OUT은 undefined다. snapshot 어느 필드도 소비하지 않는다.
}
```

DOMAIN에 직접 연결되지 않은 UART-C는 UNSUPPORTED이며 enum 존재만으로 지원하지 않는다. 공유 CAN의 다른 ECU 한 프레임을 특정 BCM/CIS/VSS/WINDOW 정상으로 해석하지 않는다. [통신 관측·Health 상세](NETWORK_FAILURE_HEALTH.md)와 [원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_getnetworkhealthsnapshot)을 연결한다.

<a id="restart"></a>
## 교체·재시작과 자원 수명

새 등록 table의 값 복사는 진행 중 callback의 종결이나 DMA 참조 해제를 증명하지 않는다. 실제 hot replacement/해제 계약이 없으면 현재 처리에 table을 교체하지 않는다. owner가 새 접수를 제한하고 진행 중 callback/보관 항목/Driver 참조/필수 실패 전달 상태를 구분해 확인한 뒤 재등록 가능 여부를 판단해야 한다. 특정 drain 시간이나 자동 purge 정책을 정하지 않는다.

local reset은 과거 항목을 새 생성 세대로 성공 접수한 사실로 바꾸지 않는다. 새 Port epoch와 구 submission identity를 구분하고 늦은 callback을 새 버퍼/명령에 적용하지 않는다. 원격 Boot 변경·Gateway reconnect·C2 재초기화는 서로 다른 사건이다. 현 Interface v0.5와 후보 v0.6/Vehicle v0.3의 ABI 차이를 실제 팀 승인·동일 버전 재빌드 근거 없이 혼용하지 않는다.

<a id="counterexamples"></a>
## 자체검수 반례

| ID | 반례 | 기대 제한 |
|---|---|---|
| B15-REG-01 | IsInitialized=true, 특정 txPort=NULL | 미연결 route는 NOT_AVAILABLE; 전체 차량 준비로 확대하지 않음 |
| B15-REG-02 | SetTxPort(NULL)을 임의 장애 복구에 호출 | Production에서는 초기화 후 등록 table 해제 동작이 확인됨. Owner 승인·callback/Driver 수명 확인 전 임의 해제 금지 |
| B15-REG-03 | table 값 복사 후 callback 상태 메모리 해제 | 함수/상태 수명 위반; 진행 호출 종료 근거 전 해제 금지 |
| B15-REG-04 | Observer 등록 OK, 필수 LocalFailure sink 없음 | correlated discrete 접수 준비 미충족; Observer로 대체하지 않음 |
| B15-REG-05 | Observer 실패, Core commit 완료 | 입력/차량 동작을 다시 실행하지 않음 |
| B15-REG-06 | 관리 API 등록 실패의 부분 반영 상태 불명 | 원 반환·상태 미확인 보존, 영향 경로 제한; 가짜 rollback 성공 없음 |
| B15-REG-07 | Query Context OK 뒤 owner 갱신 끼어듦 | 같은 Snapshot/원 CAN 결합 증거 없음; 해당 projection 제한 |
| B15-REG-08 | Health Getter 실패 후 OUT 사용 | 금지; 실패 OUT undefined |
| B15-REG-09 | Health OK, transport UNAVAILABLE | snapshot 조회 성공만 유지; 링크 정상/차량 가용으로 바꾸지 않음 |
| B15-REG-10 | DOMAIN에서 UART-C enum 조회 | UNSUPPORTED; CIS/Pi 링크를 DOMAIN 직결로 생성하지 않음 |
| B15-REG-11 | 한 peer 재접속으로 DomainIf_Init 무조건 재호출 | 미완료 원 항목/참조 수명과 별개이므로 금지 |
| B15-REG-12 | 개별 HOLD callback 주소 존재 | 지원/외부 합의/기동 준비 근거 없으면 정상 활성화하지 않음 |

실제 등록·컴파일·제품 실행·보드 시험의 증거는 별도로 필요하며, 이 문서의 등록 설계가 이를 대신하지 않는다.
