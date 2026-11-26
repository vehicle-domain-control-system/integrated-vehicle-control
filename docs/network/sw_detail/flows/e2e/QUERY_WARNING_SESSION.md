# Query·경고 읽음 — 현재 수신자와 원 요청·발생의 분리

[E2E 전체](../../09_E2E_FAILURE_RECOVERY.md) · [DOMAIN](../../nodes/DOMAIN.md) · [ESP32/MOBILE](../../nodes/ESP32_MOBILE.md) · [공통 실패·복구](COMMON_FAILURE_RECOVERY.md)

SC15는 기존 Getter 결과를 조회하는 경로이고 SC16은 같은 경고의 읽음과 사용자 이탈 후 열린 도어를 판단하는 경로다. Network가 새 원격 Query RPC·차량 cache·경고 Manager를 만들지 않는다. CF01/UART-M VERSION2는 후보이며 실제 양단 채택·Session·종결 정책과 Driver/소비 바인딩은 HOLD다.

## 기존 구조와 Data/API

N08 MOBILE→P04→N05 ESP32→P02/R05→N01 DOMAIN, I05/I01이 주 경계다. SC16은 N02 BCM→P01/R01→DOMAIN의 실제 도어 상태를 추가한다. L01/M01/F01~02/ST01~02가 현재 문맥과 원 관측을, M02/F04~05/ST03이 Query와 종결을, L02/M03/F07~10/ST04가 typed 호출을, L03/M04/F11~13/ST05~07이 사본·순서를 맡는다. L04/M06/F16~18/ST08과 L05/M07~09/F19~25/ST09~12는 Stream·Driver·결과 수명이다.

| 기존 정확한 원형 | 소유·입력·정본 |
|---|---|
| `DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message, const NetUartRxMeta *meta);` | Network private, 검증된 동일 frame의 borrowed IN. [Query 단일 정본](../../functions/UART_M_QUERY_CONTROL.md#process) |
| `DomainIf_Status_t DomainIf_ValidatePassiveQueryContext(DeviceContextId_t deviceContextId, SessionId_t sessionId);` | DOMAIN 공개 본체. 기존 현재 단말/Session 검증이며 활성화 API가 아님 |
| `DomainIf_Status_t DomainIf_QueryGetRequestResult(DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);` | DOMAIN 기록 Owner; 현재 권한 있는 단말과 과거 원 요청을 분리. [Getter 정본](../../functions/DOMAIN_QUERY_PROVENANCE.md#api-DomainIf_QueryGetRequestResult) |
| `NetSubmitResult DomainNetPriv_SubmitMobileRequestResult(const DomainIf_MobileRequestResult_t *result, const DomainQueryResponseContext_t *query);` | Network 안정 사본 접수. [M_RESULT](../../functions/uart_m/M_RESULT.md) |
| `NetSubmitResult DomainNetPriv_SendQueryEnd(const NetMobileQueryEnd *message);` | 기존 private 종결 제출, 실제 final TX가 아님. [M_QUERY_END](../../functions/uart_m/M_QUERY_END.md) |
| `void EspNetPriv_OnQueryEnd(const NetMobileQueryEnd *message, const NetUartRxMeta *meta);` | ESP32 private 실제 RX 인계. void 반환은 BLE/App 소비 ACK가 아님 |
| `DomainIf_Status_t DomainIf_RxWarningAck(const DomainIf_WarningAck_t *ack);` | DOMAIN 공개 본체. [M_WARNING_ACK](../../functions/uart_m/M_WARNING_ACK.md#deliver) |
| `DomainIf_Status_t DomainIf_TxMobileWarning(const DomainIf_WarningInfo_t *warning);` | DOMAIN 확정 Warning→등록 txMobileWarning→Network. [M_WARNING](../../functions/uart_m/M_WARNING.md) |

Getter 전체18와17 schema의 원형·인자·실패 OUT은 [DOMAIN Query provenance](../../functions/DOMAIN_QUERY_PROVENANCE.md#getter18)를 따른다. RequestResult 단건/목록 Getter2개가 M_RESULT 하나를 공유한다. Data는 [UART-M DTO](../../data/UART_M_DATA.md), [DOMAIN 공개 타입](../../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md), [Query 계약](../../functions/UART_M_QUERY_CONTROL.md#contracts) 정본이다. IN은 호출 동안 borrowed, OUT은 caller 소유이며 성공 시에만 읽는다. DOMAIN 호출은 Owner Task의 직렬 문맥에서 수행한다. 전달 후 참조 보관은 같은 원 identity까지 복사해야 하며 ISR에서 C2 호출하지 않는다.

<a id="sc15"></a>
## SC15 — 기존 상태·결과·경고 Query와 부분 응답·종결

직접 Wire/Rx **19/19**는 M_QUERY + 기존 하향17 schema + M_QUERY_END다. M_CONTEXT·ALIVE 및 원 CAN 자료는 이 흐름의 선행·보조 근거이며 직접 분모에 새로 추가하지 않는다. 실제 응답 수는 응답 schema 수17 또는 Getter 수18과 다르다.

<a id="sc15-normal"></a>
### 정상 조건부 경로

1. MOBILE 요청을 Gateway가 현재 Device/Session의 M_QUERY로 전달한다. Stream/Codec은 완전 Frame·CRC·정확한 schema를 검사하고 Router가 기존 문맥/순서를 검증한다. NetUartRxMeta에는 Boot가 없다.
2. 먼저 반영된 M_CONTEXT와 동일한 DOMAIN Owner 직렬 문맥에서 passive context 검증을 수행한다. SESSION_READY0만을 이유로 최초 유효 Query를 거부하거나 이 검증으로 Session을 발급하지 않는다. 현재 DOMAIN Boot는 M01의 별도 실제 기동 근거다.
3. 기존 scope/권한·필터 계획에 따라 Getter를 호출한다. 함수 OK 뒤에만 값/hasRecord/count를 읽는다. 원 품질 STALE/INVALID/NO_DATA를 정상 품질로 보정하지 않는다. 실패 OUT의 남은 정상값은 응답이 아니다.
4. 현재 recipient Device/Session/QueryId/scope를 `DomainQueryResponseContext_t`로 유지한다. 이 구조에 Boot를 추가하지 않는다. 원 CAN11 projection은 같은 raw→C2 실제 반영→Getter 출력 근거가 있어야 하며 값 비교나 `sourceBindingVerified=true` 설정으로 대신하지 않는다. 그 근거가 없는 항목만 미제공/부분으로 제한한다.
5. 기존 private Submit은 전체 응답 사본 접수만 보고한다. 실제 송신 결과는 원 Query 및 응답 항목에 귀속해 관측하며 final TX count와 enqueue count를 분리한다. Driver release는 별도 수명 사건이다.
6. [기존 terminal 절차](../../functions/UART_M_QUERY_CONTROL.md#terminal)가 완전성/부재/부분·모든 항목 최종 결과·순서를 확인한 뒤 End를 제출한다. ESP32가 같은 Boot/recipient/Query의 실제 End RX를 수용하고 BLE/App에서 표시·종결을 처리하는 경계는 그 담당자 소유다. End의 enqueue나 void callback으로 App 완료를 선언하지 않는다.

<a id="query-callsite"></a>
### 기존 Getter 호출 예 — recipient B에서 과거 Request A 조회

새 함수가 아닌 `DomainNetPriv_ProcessQuery` 내부의 기존 단건 분기를 보여 준다. 진입 전 실제 현재 M_CONTEXT·DOMAIN Boot·권한·scope 및 단건 대상 선택을 확인한다. `query`는 Network가 소유한 현재 recipient B의 응답 문맥이며 `target`은 요청에서 온 과거 `RequestContext_t.sessionId/requestId` A다. `nowMs`는 이 Getter 호출 시점의 확인된 로컬 단조시간이다. 앞 조건이 미확인이라면 해당 경계를 HOLD하고 아래 코드를 실행하지 않는다.

```c
DomainIf_Status_t contextRc = DomainIf_ValidatePassiveQueryContext(
    requestingDevice, currentSession);
if (contextRc != DOMAIN_IF_OK) {
    // Getter/Submit을 호출하지 않고 정확한 문맥 실패를 원 Query에 남긴다.
} else {
    DomainIf_MobileRequestResult_t record;
    bool hasRecord;
    DomainIf_Status_t getterRc = DomainIf_QueryGetRequestResult(
        requestingDevice, &target, nowMs, &record, &hasRecord);
    if (getterRc != DOMAIN_IF_OK) {
        // record/hasRecord 모두 읽지 않는다. 확인된 빈 결과로 세지 않는다.
    } else if (!hasRecord) {
        // 이 대상의 확인된 부재. 아직 전체 scope의 End status는 결정하지 않는다.
    } else {
        NetSubmitResult submitRc = DomainNetPriv_SubmitMobileRequestResult(
            &record, &query);
        if (submitRc == NET_SUBMIT_OK) {
            // 안정 사본 접수만 기록. finalTxCount 증가/End 전송 없음.
        } else {
            // 정확한 제출 실패를 보존. 같은 요청의 차량 동작을 다시 실행하지 않는다.
        }
    }
}
```

위 호출 동안 현재 문맥의 교체/재진입이 없다는 실제 직렬화가 필요하다. Query가 과거 요청을 조회하더라도 다른 Device 기록 접근 권한을 만들지 않는다. 공개 Getter 본체·모바일 요청 실행 함수를 다시 구현하지 않는다.

<a id="sc15-failure"></a>
### 실패와 관측 상태

| 사건 | 실제 상태·금지 부작용 | 종결 경계 |
|---|---|---|
| Getter 실패, OUT 이전값 남음 | OUT/flag/count 미소비; 부분·원 오류 보존 | 성공0건과 실패0건을 구분 |
|10개 접수,8 final TX,2 미확정 | release가 모두 와도 두 결과 미확정 | End 보류 |
|8 final TX,2 확정 실패 | 동일 Query·송신 순서/완전성까지 입증 | status2/count8 후보; 전체성 미입증이면 여전히 보류 |
| End 접수 후 최종 TX 실패 | 응답8건을 소급 실패로 만들지 않음 | Gateway/App 종결 미확인, End 완료 아님 |
| End가 먼저 도착하거나 Session/Boot 교체 | 구 응답·End를 현재 Query로 재귀속하지 않음 | 원 문맥 결과 유지·현재 완료 없음 |
| 같은 payload, 새로운 LINK_SEQUENCE 재전송 | 응답 의미 ordinal 부재 정책 OPEN | 중복을 별도 응답으로 count하지 않음; 불명확하면 COUNT_HOLD |
| count가 u16 범위 초과 | wrap0 또는 포화 성공 금지 | 분할/목록/종료 정책 회신 전 HOLD |
| WINDOW2 요청 | 실행·표시 HOLD, 해당 목록에서 미제공 사유 유지 | ALL scope가 WINDOW를 활성화하지 않음 |

QUERY_ID0 자발 보고, End 자체, 확인된 중복은 응답 count에서 제외한다. Query의 새 deadline/재시도 수/capacity/순번 비교창을 정하지 않는다. 성공 목록의 count==capacity만으로 전체 이력 완료를 주장하지 않는다. CURSOR와 scope 표현이 준비되지 않으면 임의 분할·원격 ECU 조회를 추가하지 않는다.

<a id="sc15-recovery"></a>
### 복구

재접속은 구 Query의 재개나 새 recipient 귀속 허가가 아니다. 현재 문맥에서 새 유효 Query를 검증하고 과거 요청 identity는 조회 대상으로만 보존한다. 원 TX 결과 미확정 항목은 Driver token/epoch 근거로만 후발 관측하며 임의 재전송하지 않는다. 수신과 실제 App 표시·종결에 필요한 원 Query correlation/정책은 IF-O12/IF-O21/IF-O23/IF-O24/IF-O25, G06 및 CF01 회신 대기다.

<a id="sc16"></a>
## SC16 — 경고 읽음 및 사용자 이탈 후 열린 도어

직접 Wire/Rx **3/3**: BCM_DOOR_STATE, M_WARNING_ACK, M_WARNING. M_CONTEXT·VSS 경고 등은 보조 경계이며 분모를 늘리지 않는다.

<a id="sc16-normal"></a>
**정상 조건부 흐름:** BCM 실제 도어 상태의 원 Boot/update/quality/AGE → 기존 DOMAIN Rx/Manager → DOMAIN이 실제 사용자 이탈 조건과 열린 도어를 함께 판단 → 원 kind/occurrence/DOMAIN Boot를 가진 M_WARNING → Gateway/App 표시 → 사용자의 같은 경고 읽음 M_WARNING_ACK → Stream/Router의 현재 Device/Session 및 원 경고 Boot/kind/occurrence 검증 → C2 `DomainIf_RxWarningAck`. BLE 끊김만으로 사용자 이탈을 판정하지 않는다.

M_WARNING_ACK는21B이고 AGE가 없다. 수신시각/LINK_SEQUENCE로 원 생성 AGE를 발명하지 않는다. C2 `DomainIf_WarningAck_t`에는 deviceContextId/sessionId/occurrenceId/type 네 필드만 있고 warning_domain_boot가 없다. Network가 Wire의 원 Boot를 검증한 **뒤** C2로 투영한다. 정확한 kind1~8→C2 의미 매핑과 호출은 [기존 DeliverWarningAck](../../functions/uart_m/M_WARNING_ACK.md#deliver)를 재사용한다. 새로운 ACK wrapper는 만들지 않는다.

<a id="sc16-failure"></a>
**실패:** 이전 Session 또는 이전 DOMAIN Boot의 동일 occurrence ACK는 현재 경고 읽음으로 수용하지 않는다. kind가 다르거나 원 경고 연결이 미입증이면 정상 C2 호출을 보류한다. 읽음 인계 OK는 위험 CLEAR, VSS STOP, 실제 App 표시 성공, 도어 닫힘을 뜻하지 않는다. 같은 ACK 반복을 새 occurrence로 만들지 않는다. WINDOW 경고 kind의 존재도 WINDOW 표시·기능 HOLD를 해소하지 않는다.

<a id="sc16-recovery"></a>
**복구:** 재접속 후 현재 세션에서 확인한 같은 경고를 대상으로만 읽음을 처리한다. 과거 읽음을 새 Boot 경고에 복사하지 않는다. 위험 지속/해소와 occurrence 생명주기는 DOMAIN Owner가 판단하며 새 유효 도어 상태가 없으면 CLOSED로 채우지 않는다. 양단 ACK 정책·App 실제 소비는 기존 Owner 미결/외부 HOLD에 남긴다.

## 전문 자체검수 반례

| 사전 상태와 입력 | 경계·예상 부작용 | 정적 기대 |
|---|---|---|
| Session B의 유효 Query가 Request A를 조회 | 같은 Device 권한 확인, target A 보존 | 수신 recipient만 B, 차량 동작 재실행 없음 |
| Getter 실패, hasRecord=true 찌꺼기 | 성공 분기 이전 OUT 미소비 | 빈/성공 응답 합성 없음 |
| 응답8 final TX 뒤 End 최종 실패 | 원 응답 결과 보존 | Query/App 완료 미확인 |
| 구 Session ACK와 현 경고 occurrence 일치 | Session·Boot·kind 전체 비교 | 새 읽음 처리 없음 |
| BLE disconnect, 도어 열림 | 이탈 조건을 DOMAIN에서 별도 확인 | disconnect만으로 이탈 경고 발생 없음 |
| 읽음 OK, 위험 여전히 active | 읽음과 위험 상태·음향 정책 분리 | CLEAR/STOP 합성 없음 |

본 문서는 기존 정본을 연결한 문서 자체검수이며 ABI·실행·BLE/App 시험은 NOT_EXECUTED다. 미결 확정이나 S6 통합 반영을 수행하지 않는다.
