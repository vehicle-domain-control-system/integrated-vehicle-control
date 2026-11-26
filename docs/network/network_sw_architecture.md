# Network SW Architecture

> Status: Draft  
> Related Issue: #47 `[ARCH] 네트워크 SW 전체 구조 설계`  
> Scope: CAN FD / UART 기반 Network Software의 논리 구조와 책임 경계  
> Validation: Document design only · Firmware/board validation not performed

## 1. 목적

본 문서는 현재 네트워크 설계를 실제 펌웨어 구조로 연결하기 위해
Network Software의 전체 Layer, 모듈 책임, Node별 적용 범위를 정의한다.

이 문서에서는 다음을 다룬다.

- UART / CAN FD Network SW의 공통 구조
- Layer별 책임과 의존 방향
- UART와 CAN의 공통점 및 개별 책임
- Domain / BCM / CIS / VSS의 Network SW 배치
- Application과 Network SW의 책임 경계
- 초기 Superloop 실행 구조와 후속 RTOS 전환 방향
- 후속 상세 설계로 분리할 항목

CAN ID, UART Type, payload layout, 주기, timeout 등
**wire contract의 상세 값은 이 문서에서 다시 정의하지 않는다.**

상세 통신 계약의 현재 설계 후보는 [Interface 읽기 안내](interface/00_START_HERE.md)와 [노드 간 공통 규칙](interface/07_CROSS_NODE_INTERFACE_RULES.md)을 우선 대조한다.
상세 처리 경계는 [SW 상세설계](sw_detail/01_NETWORK_OVERVIEW.md)를 따른다.
[구형 네트워크 초안](network_design_draft_v0.1.md)은 이력 참고이며 현재 Wire 규범을 대체하지 않는다.
문서 참조 정렬은 양단 채택·Production ABI·제품 통합 승인이 아니다.

---

## 2. 전체 네트워크 구성

현재 논리 연결은 다음과 같다.

```mermaid
flowchart TD
    APP["Mobile App"] <-->|"BLE"| ESP["ESP32"]
    ESP <-->|"UART-M · 115200 bps"| DOMAIN["Domain · S32K344"]

    PI["Raspberry Pi"] <-->|"UART-C · 115200 bps"| CIS["CIS · S32K144"]

    DOMAIN <--> BUS["Shared CAN FD Bus"]
    BCM["BCM · S32K144"] <--> BUS
    CIS <--> BUS
    VSS["VSS · S32K344"] <--> BUS

    WINDOW["WINDOW · S32K144<br/>[구현 보류 — 설계 유지]"] -.-> BUS
```

주요 경계는 다음과 같다.

| 구간 | 통신 | 역할 |
| --- | --- | --- |
| App ↔ ESP32 | BLE | 사용자 요청/상태 표시. 상세 BLE 설계는 ESP32/App 담당 |
| ESP32 ↔ Domain | UART-M | App 요청, Query, 차량 상태/결과/경고 전달 |
| Domain ↔ BCM/CIS/VSS | CAN FD | 차량 내부 상태·명령·결과·이벤트 전달 |
| CIS ↔ Raspberry Pi | UART-C | Vision 결과와 Domain permission 전달 |
| WINDOW | CAN FD | 계약은 유지하되 현재 구현은 보류 |

Pi와 Domain은 직접 UART로 연결하지 않는다.

---

## 3. Network SW 전체 구조

Network SW는 Application/Policy와 물리 통신 Driver 사이를 계층화한다.

```mermaid
flowchart TD
    APP["Application / Policy"]
    NC["Node Communication"]

    APP <-->|"intent / functional evidence"| NC

    NC <-->|"typed UART Port"| UP["UART Application Port"]
    NC <-->|"typed CAN Port"| CP["CAN Application Port"]

    subgraph UART["UART Stack"]
        UP --> US["Message Service"]
        US --> UC["Protocol / Framer / Codec"]
        UC --> UT["Transport"]
        UT --> UD["Driver"]
        UD --> UH["HAL Adapter / BSP"]
    end

    subgraph CAN["CAN Stack"]
        CP --> CS["Message Service"]
        CS --> CC["Protocol / Codec"]
        CC --> CT["Transport"]
        CT --> CD["Driver"]
        CD --> CH["HAL Adapter / BSP"]
    end

    UART_HW["UART HW (DMA: 후속 선택)"]
    CAN_HW["CAN FD Controller"]

    UH --> UART_HW
    CH --> CAN_HW

    COMMON["Common Infra<br/>Timebase / Diagnostics / Sync Adapter"]
    COMMON -.-> NC
    COMMON -.-> UART
    COMMON -.-> CAN
```

### 기본 원칙

1. **Application/Policy**
   - 기능 상태와 차량 정책의 owner다.
   - 통신 sequence, framing, Driver 상태를 직접 관리하지 않는다.

2. **Node Communication**
   - 통신 identity, request/result correlation, source freshness, cross-link semantic mapping을 담당한다.
   - UART frame을 CAN frame으로 단순 복사하지 않는다.
   - UART Stack과 CAN Stack 사이의 직접 호출을 만들지 않는다.

3. **UART Stack / CAN Stack**
   - 각 링크의 wire format, transport scheduling, Driver lifetime을 담당한다.
   - 서로 runtime, codec, queue를 공유하지 않는다.

4. **Common Infra**
   - 초기 실행에서 시간원·정적 동기화·제한된 진단 기능을 제공하고, RTOS 전환 시 필요한 Adapter를 연결한다.
   - message semantics나 차량 정책을 소유하지 않는다.

---

## 4. Layer별 책임

| Layer | 주요 책임 | 하지 않는 일 |
| --- | --- | --- |
| Application / Policy | 기능 상태, authorization, arbitration, command intent, warning 의미, 기능 Result | UART/CAN framing, sequence, Driver 제어 |
| Node Communication | session/context, Request/Command/Result correlation, source identity/AGE, semantic mapping | 차량 기능 정책 재판정, Driver 직접 접근 |
| Application Port | Node와 Stack 사이 typed API 경계 | vendor SDK type 노출 |
| Message Service | TX pending/scheduling, sequence 관리, periodic/change/event 처리 | payload 의미의 기능 정책 결정 |
| Protocol / Codec | field encode/decode, wire validation | transport scheduling, 차량 정책 |
| UART Framer | byte stream에서 frame 조립, resync, CRC frame 경계 | Application 의미 처리 |
| Transport | bounded TX/RX lifetime, stable data handoff, Driver admission/outcome | 기능 Result 생성 |
| Driver | HW/SDK callback, TX/RX primitive, local HW error | parsing, policy |
| HAL Adapter / BSP | vendor SDK 및 target 차이 격리 | Network semantic 소유 |
| Common Infra | 시간원, 정적 동기화, 진단, 제한된 공통 기능과 후속 RTOS Adapter | UART/CAN 간 routing |

---

## 5. UART와 CAN의 공통·개별 구조

### 5.1 공통

UART와 CAN 모두 다음 원칙을 사용한다.

- callback/ISR에서는 최소 작업만 수행한다.
- 프레임 해석·Codec·송수신 선택은 초기 Network Superloop의 ISR 밖 처리 문맥에서 수행한다. RTOS Task 분리는 후속 선택이다.
- Application에는 raw frame 대신 typed message/evidence를 전달한다.
- 송신 요청의 admission과 실제 차량 기능 Result를 구분한다.
- source freshness와 local transport 상태를 구분한다.
- runtime dynamic allocation 없이 bounded/static resource 사용을 기본으로 한다.

### 5.2 UART 전용

UART는 byte stream이므로 별도의 framing이 필요하다.

```text
Driver
→ Transport
→ Framer
→ Codec
→ Message Service
→ Application Port
```

현재 설계 방향:

- 초기 UART 수신은 짧은 RX ISR과 정적 Ring Buffer를 사용한다. DMA는 필요 시 후속 전환으로 검토한다.
- UART-M / UART-C는 동일 Stack 구조를 사용하되 Profile/Runtime은 독립
- frame split/continuous 수신 대응
- CRC 및 frame resync
- LINK_SEQUENCE는 Message Service가 선택하고 Driver accepted 시 commit
- 후속 DMA 도입 시 DMA 완료와 실제 UART line 전송 완료를 구분
- RTS/CTS를 사용하지 않으므로 RX overflow/loss를 명시적으로 검출

### 5.3 CAN 전용

CAN Controller가 frame 경계를 제공하므로 UART Framer는 없다.

```text
Driver
→ Transport
→ Codec
→ Message Service
→ Application Port
```

현재 설계 방향:

- CAN DMA는 사용하지 않음
- MB / FIFO / filter의 실제 binding은 구현 직전 target 확인
- TX_SEQUENCE는 Message Service가 선택하고 Driver accepted 시 commit
- local TX complete와 상대 ECU 수신/Vehicle Result를 구분
- Bus-off는 CAN transport 내부에서 복구하며 old command를 자동 재실행하지 않음
- CAN 5 ms는 accepted frame의 강제 cancel 시간이 아니라 현재 계약의 hop allowance 제안값으로 취급

---

## 6. Message Service의 전송 의미

전송 대기 데이터는 단순 FIFO 한 종류로 보지 않는다.

### A. Replaceable Current State / Current Warning

현재 값이 중요한 정보다.

예:

- 일반 ECU State
- `VSS_WARNING_STATE`
- `M_WARNING`의 unsolicited current update

동일 semantic key의 아직 시작되지 않은 pending 값은
최신 값으로 교체할 수 있다.

### B. Discrete Command / Result / Event

발생 identity와 순서를 보존해야 한다.

예:

- Command
- Result
- Event

silent overwrite를 허용하지 않는다.

### C. Query-scoped Ordered Response

Query에 속한 응답은 일반 current-state와 분리한다.

예:

- Query에 대한 State 응답
- Query에 대한 Warning 응답
- Query Result
- Query End

동일 Type의 unsolicited 최신 상태가
진행 중 Query 응답을 덮어쓰지 않도록 한다.

> Scheduling urgency와 storage semantics는 별개다.  
> Current Warning은 높은 우선순위로 처리할 수 있지만 Event처럼 모든 중간 값을 보존해야 하는 것은 아니다.

---

## 7. Node별 Network SW 적용 범위

| Node | 통신 Stack | Node Communication 책임 | Application/Policy 책임 |
| --- | --- | --- | --- |
| Domain · S32K344 | UART-M + CAN | App session/context, Request↔Command↔Result correlation, Query, CAN↔UART semantic mapping, source freshness | authorization, arbitration, vehicle command intent, warning/hazard 정책 |
| CIS · S32K144 | UART-C + CAN | Pi source identity/AGE, Vision forwarding, Domain permission forwarding, CAN/UART semantic mapping | sensor/vision 기능값·validity·reason·status/fault |
| BCM · S32K144 | CAN | typed command/result/state 통신 경계, replay evidence | actuator 실행, 상태/Result 생성, 기능 안전 정책 |
| VSS · S32K344 | CAN | typed warning/event/evidence 전달 | warning/audio/recovery 정책 |
| ESP32 | UART-M endpoint | 차량 문맥 확인 및 `SESSION_READY` 판단, App 중계 | BLE 상세는 별도 담당 |
| Raspberry Pi | UART-C endpoint | Vision/Permission contract endpoint | Vision 내부 처리 별도 담당 |
| WINDOW · S32K144 | CAN | 계약 유지 | **[구현 보류 — 설계 유지]** |

---

## 8. 초기 Superloop 실행 구조와 후속 RTOS 전환

초기 실행은 짧은 UART/CAN 수신 ISR과 정적 Ring Buffer/Queue, Network Superloop로 구성한다. DOMAIN의 C2 Rx·Query·Private Transition은 ISR 밖 단일 담당 문맥에서 직렬 처리한다. 그림의 연결은 처리 책임이며 별도 Task 생성 의무가 아니다.

```mermaid
flowchart LR
    UI["UART RX ISR"] -->|"짧은 바이트·시각 인계"| UR["정적 UART Ring Buffer"]
    CI["CAN RX ISR"] -->|"짧은 프레임·시각 인계"| CQ["정적 CAN RX Queue"]
    UR --> LOOP["Network Superloop"]
    CQ --> LOOP
    LOOP -->|"ISR 밖 직렬 인계"| OWNER["노드 기능/C2 담당 경계"]
    OWNER -->|"의미 기반 송신 요청"| LOOP
    APP["Application / Policy"] <--> OWNER
```

후속 RTOS 전환 시 참고할 기존 Task 역할 후보(초기 실행 필수 조건 아님):

| Node | 후속 RTOS 배치 후보 |
| --- | --- |
| Domain | `UartCommTask` + `CanCommTask` + `DomainOwnerTask` |
| CIS | `UartCommTask` + `CanCommTask` + `CisOwnerTask` |
| BCM | `CanCommTask` + Application role |
| VSS | `CanCommTask` + Application role |

초기 버퍼/큐 크기와 실행 주기, 후속 RTOS의 Task 수·우선순위·스택은 실제 자원·시간 검증 후 정한다.

원칙:

- ISR/callback은 최소 수신 자료·실제 시각·장치 오류만 인계
- data는 별도 bounded storage에 보존
- notification count를 message count로 사용하지 않음
- 고정 1 ms polling 대신 event/deadline 기반 실행
- current state / discrete / query response의 storage 의미를 보존
- numeric priority/stack/capacity는 실제 target 측정 후 확정

---

## 9. Cross-stack / Network Service 책임

Issue #47에서 언급한 공통 Network Service 후보는 다음처럼 배치한다.

| 후보 기능 | 현재 배치 방향 |
| --- | --- |
| Message Router | 전역 Router를 만들지 않고 **Node Communication**이 semantic mapping 담당 |
| Tx Scheduler | UART/CAN 각 **Message Service**가 자신의 link scheduling 담당 |
| Freshness / Timeout | source AGE/context freshness는 Node Communication, transport due/repeat/expiry는 각 Stack, 기능 timeout은 Application/Policy |
| Diagnostics | Common Infra에 공통 metric/snapshot을 두고 Stack/Node별 확장 |
| Message Codec | UART/CAN별 독립. 서로 wire payload/codec을 직접 공유하지 않음 |

이 구조를 통해 `UART Stack → CAN Stack` 직접 의존을 만들지 않는다.

---

## 10. Fault / Recovery 기본 원칙

### UART

- malformed / CRC error frame은 실행하지 않는다.
- stream loss와 parser error를 구분한다.
- Driver BUSY와 accepted 이후 failure를 구분한다.
- transport failure를 Vehicle Result로 변환하지 않는다.

### CAN

- Bus-off 동안 신규 TX를 제한한다.
- recovery 후 old command/event를 자동 재실행하지 않는다.
- current state는 현재 snapshot부터 재개할 수 있다.
- local TX complete는 Vehicle Result가 아니다.

### Node Context

- restart/session 변경 시 old context의 not-yet-started pending을 제거할 수 있다.
- context-dependent pending은 local context tag를 통해 관리한다.
- 이 local tag는 wire의 `SESSION_ID`, `REQUEST_ID`, `QUERY_ID`와 별개다.

상세 lifecycle/API는 구현 상세 설계에서 정의한다.

---

## 11. 현재 확정하지 않는 항목

다음 항목은 Architecture 문서에서 임의로 수치를 고정하지 않는다.

- UART DMA backend 방식
- CAN MB / FIFO / acceptance filter 실제 binding
- physical pin / wiring / transceiver
- CAN bit timing 세부 값
- IRQ numeric priority
- FreeRTOS Task numeric priority
- Task stack size
- Queue / pool depth
- CPU 사용률
- 최종 RAM budget
- 실제 callback ISR context

위 항목은 실제 SDK/RTD와 Board에서 확인 후 확정한다.

---

## 12. 현재 상태

| 항목 | 상태 |
| --- | --- |
| Network SW 논리 구조 | 정의 |
| UART/CAN 책임 경계 | 정의 |
| Node별 적용 범위 | 정의 |
| 초기 Superloop + 짧은 ISR·정적 Buffer/Queue | 선택된 초기 실행 방향, 실제 보드 구성 검증 전 |
| FreeRTOS Task 역할 | 후속 전환 후보(초기 필수 아님) |
| Context / correlation 기본 원칙 | 정의 |
| RTD / SDK target binding | 확인 필요 |
| Resource numeric sizing | 실측 필요 |
| Firmware 구현 | 미진행 |
| Board validation | 미진행 |
| WINDOW 구현 | **[구현 보류 — 설계 유지]** |

---

## 13. 후속 상세 설계 항목

본 문서를 기준으로 실제 구현 단계에서는 다음 항목을 분리해 진행한다.

1. UART Driver / Transport binding 및 필요 시 후속 DMA 검토
2. UART Framer / Codec / Message Service
3. CAN Driver / MB-FIFO / Transport binding
4. CAN Codec / Scheduling / Recovery
5. Network Diagnostics / Observability
6. Domain Request / Command / Result / Query integration
7. CIS Vision / Permission integration
8. BCM / VSS typed Port integration
9. 초기 Superloop·Ring Buffer/Queue 배치 및 후속 RTOS Task/IPC 전환 검토
10. Resource / Timing 실측 및 closure
11. Target / Node / Multi-node validation

후속 상세 문서는 이 Architecture의 책임 경계를 변경하지 않고,
필요한 경우 Architecture 변경 Issue를 먼저 통해 갱신한다.

---

## 14. Issue #47 완료 조건 대응

| Issue #47 완료 조건 | 본 문서 |
| --- | --- |
| 전체 네트워크 SW 구조도 | §3 |
| Layer별 책임과 의존 방향 | §4 |
| CAN FD / UART 모듈 책임 구분 | §5 |
| Domain / BCM / CIS / VSS 적용 범위 | §7 |
| Network Service 공통 모듈 후보 정리 | §9 |
| 후속 상세 설계 Issue로 분리 가능한 수준 | §13 |

---

## 15. 관련 문서

- Network / Protocol contract: [`../network/network_design_draft_v0.1.md`](../network/network_design_draft_v0.1.md)
- Architecture index: [`sw_detail/00_START_HERE.md`](sw_detail/00_START_HERE.md)

본 문서는 Network SW의 **논리 구조와 책임 경계**를 관리한다.
Wire contract를 변경할 경우 Network 설계 문서를 먼저 갱신하고,
SW Architecture 영향 여부를 함께 검토한다.
