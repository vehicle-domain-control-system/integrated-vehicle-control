# M02 — 조회 범위·생산·종료 감독

[Network 개요](../01_NETWORK_OVERVIEW.md) · [UART-M 개요](../05_UART_M_OVERVIEW.md) · [L01 문맥](../layers/L01_NODE_CONTEXT.md) · [L03 메시지](../layers/L03_MESSAGE_SERVICE.md)

M02의 기존 F04/F05는 M_QUERY의 현재 문맥을 확인하고 DOMAIN 소유 Getter의 성공 사본을 응답에 연결하며 M_QUERY_END의 종료 근거를 감독한다. S2의 Module9/Function25 책임을 유지하고 새 공개 조회 API·차량 cache를 만들지 않는다. ST03은 Query의 원 Device/Session·확인된 DOMAIN Boot·QueryID/scope와 응답별 실제 송신 관측을 맡는다.

| 책임 | 입력 → 처리 → 출력 | 상세 정본 |
|---|---|---|
| F04 | M_QUERY+동일 수신 meta → passive 검사 → scope별18 Getter 성공 OUT; Recent만 실제 배열 용량과5 중 작은 capacity·최신순 선정 → 17 기존 submit | [조회 함수](../functions/UART_M_QUERY_CONTROL.md#process) |
| F05 | 실제 응답 final TX/확정 실패·현재 문맥·순서 → 기존 status0..3/count → 응답 뒤 End 접수 및 후속 outcome 관측 | [종료 함수](../functions/UART_M_QUERY_CONTROL.md#terminal) |

<a id="existing-boundaries"></a>
## 기존 네 경계와 호출 계약

다음은 S4A의 기존 원형이다. M02의 F04/F05는 여러 로컬 경계를 연결하는 책임 ID이며 아래 네 본체를 새로 생성하거나 DOMAIN 공개 본체를 Network로 이관하지 않는다. `const` IN은 호출 중 borrowed이고 non-NULL이며, 지연 처리에는 동일 입력의 의미·문맥·meta 전체를 독립 소유해야 한다.

```c
DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message, const NetUartRxMeta *meta);
NetSubmitResult DomainNetPriv_SendQueryEnd(const NetMobileQueryEnd *message);
DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);
void EspNetPriv_OnQueryEnd(const NetMobileQueryEnd *message, const NetUartRxMeta *meta);
```

| 기존 함수 / 본체 Owner | 인자·수명·선행 조건 | 반환의 최소 의미와 후속 경계 |
|---|---|---|
| `DomainNetPriv_ProcessQuery` / DOMAIN Network | `message`는 수신한 Query, `meta`는 같은 Frame의 LINK_SEQUENCE·실제 로컬 수신시각이다. 현재 Device/Session과 선행 M_CONTEXT를 검증하고, 값 인자로 passive 검사를 호출한다. 어느 포인터도 반환 후 차용하지 않는다. | `DomainIf_Status_t`는 해당 로컬 Query 처리 경계의 결과다. OK만으로 Getter 성공·응답 생산·최종 TX·End 전송을 확정하지 않는다. 각 단계 실패와 원 Query를 ST03에 보존한다. |
| `DomainIf_ValidatePassiveQueryContext` / DOMAIN | Device·Session은 값 복사 IN이다. 동일 수신 Query의 현재 recipient 문맥을 넘기고 과거 request_session으로 바꾸지 않는다. 포인터 OUT이 없고 Session을 생성하지 않는다. | OK는 수동 조회 문맥의 로컬 수용이며 모든 기록에 대한 권한·미래 Session 수명·Getter/TX 준비를 대신하지 않는다. DOMAIN_IF_NOT_AVAILABLE 등 반환을 빈 기록이나 새로운 SessionReady로 합성하지 않는다. |
| `DomainNetPriv_SendQueryEnd` / DOMAIN Network | `message`는 보존한 Query 문맥과 확정된 응답별 final TX/실패를 반영한 종료 DTO다. 모든 실제 필드를 초기화하고, 성공 접수 전에 안전한 사본을 확보한다. 실패 접수이면 caller가 소유를 유지한다. | `NetSubmitResult`의 OK는 로컬 접수일 뿐 End의 UART TX·ESP32 수신·BLE/App 목록 완료가 아니다. 응답 순서와 Driver 참조·후속 outcome을 계속 감독한다. |
| `EspNetPriv_OnQueryEnd` / ESP32 Network | `message/meta`는 같은 정상 Frame의 임시 입력이다. 현재 Boot/Device/Session·QueryID·scope·중복/지연·실제 응답 수를 확인하고, 필요한 상태를 반환 전에 복사한다. | `void` 반환은 Query 완료 성공을 보고하지 않는다. 불일치 End나 늦은 End가 현재 목록 완료·새 Session·재시도를 만들지 않게 private 감독 결과를 보존한다. |

각 enum의 전체 반환값과 실패·후발 실패 처리는 [Query 함수 정본](../functions/UART_M_QUERY_CONTROL.md), [공통 반환 경계](../functions/UART_M_TX_RX_CONTROL.md#returns)를 따른다. 출처는 `S4A_RESULT/api/domain_private.md`, `api/domain_management.md`, `api/esp32.md`이며 새 협의안은 기존 S6 큐에 기록만 한다.

Query는 현재 문맥을 수동으로 검증하며 최초 SESSION_READY0을 단독 거부 사유로 쓰지 않는다. Session 발급·활성화·권한·기록 필터·DOMAIN Manager 상태는 DOMAIN/ESP32 Owner 책임이다. sourceGeneration 및 Boot 대응·동일 CAN association은 실제 근거 전 해당 항목 HOLD다. WINDOW2 Tx/Rx·표시·timeout·liveness는 보류한다.

Device별 최대8건 보관은 DOMAIN ResultManager 책임이며 Network는 Recent의 최신순 최대5건만 조회·순차 제출한다. 준비된 결과 배열의 용량이 3개 항목이면 Getter에 `capacity=3`을 전달한다. 반환 건수가 배열 용량을 초과하거나 Getter 호출이 실패하면 임의로 결과를 잘라 정상 완료로 처리하지 않는다. 다른 상태10응답·경고·단건 조회는 이 상한을 공유하지 않는다. recent5 완료와 생산8 중5 성공을 원 선택 범위로 구분한다.

실패 Getter OUT을 빈 결과로 바꾸지 않고 enqueue 수를 ITEM_COUNT로 세지 않는다. Driver accepted·final UART TX·참조 해제·ESP32 RX·BLE/App 목록 완료를 분리한다. partial/빈 집합/기록 부재/문맥 거부는 원 후보 의미를 유지하며 IF-O12/IF-O25/D-03/D-05가 미해소이면 정상 종료 실행은 제한한다. Query 실패는 새로운 NetworkLocalFailure kind가 아니다.

실제 양단 profile 채택·scope/권한·직렬화·저장소·최종 callback·결과 집계 바인딩은 미검증이다. 전문 함수·반례·정확한 Getter 원형은 [단일 정본](../functions/UART_M_QUERY_CONTROL.md)에 둔다. G6 당시 Interface39 봉인본은 보존하며, 현재 별도 후보는 Interface40개 후보의 출처·해시 기준을 따라 참조한다. 이번 보완은 Query 계약·동작을 바꾸지 않는다.

[Data/API](../data/UART_M_DATA.md) · [입력 M_QUERY](../functions/uart_m/M_QUERY.md) · [종료 M_QUERY_END](../functions/uart_m/M_QUERY_END.md) · [Query flow](../flows/UART_M_QUERY_AND_PROJECTION.md) · [상향 flow](../flows/UART_M_UPLINK.md)

## DOMAIN Service에서 Query 정본 재사용

[Query·원본 연결](../functions/DOMAIN_QUERY_PROVENANCE.md)은 기존 F04/F05와 위 Query 함수의 호출 조건을 DOMAIN Adapter에 연결한다. Getter18과17 응답 schema는 다른 분모이며 새 Getter·Encoder·차량 cache를 만들지 않는다. 현재 recipient Session과 과거 RequestSession, 로컬 접수와 최종 TX count·End 순서를 구분한다.

[공통 전송 수명](../functions/NETWORK_SERVICE_LIFETIME.md) · [DOMAIN 경계 개요](../07_DOMAIN_ADAPTER_SERVICE.md)
