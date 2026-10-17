# DOMAIN — C2 소유권과 두 전송 경계의 연결

N01은 S32K344이며 P01 CAN FD와 P02 UART-M를 연결한다. R01~R04의 차량 노드 경로와 R05 MOBILE 경로가 I01에서 DOMAIN 공개 입력·출력에 닿는다. Pi와는 물리적으로 직접 연결되지 않으며 R06의 CAN 수신 끝점이다. 이 문서는 기존 [DOMAIN Adapter](../07_DOMAIN_ADAPTER_SERVICE.md)의 함수 본체를 노드 관점에서 연결한다. 공개 API 72개, Network TxPort callback 26개, DOMAIN/기능 Observer 11개의 소유권·정확 원형은 그대로 유지한다.

## 담당자와 Layer·Module·Function

| 책임 | 소속 | 생산자 → 호출자 → 소비자 |
|---|---|---|
| 판단·명령·기존 Snapshot·Getter | DOMAIN 공개 본체와 Manager | DOMAIN 기능 → DomainIf → 등록된 Network callback; Network가 판단·cache를 재구현하지 않음 |
| 현재 peer·원 기동·동일 관측 연결 | L01/M01/F01·F02 | 링크 검증 자료 → Network Adapter → DOMAIN Rx; [동일성 근거](../functions/DOMAIN_QUERY_PROVENANCE.md) |
| Query의 응답·종결 감독 | L01/M02/F04·F05 | 원 Query → 기존 Getter18 → schema17 → 실제 응답 TX·End 관측 |
| 등록·TxPort·Rx·후발 관측 | L02/M03/F06~F09 | [등록](../functions/DOMAIN_REGISTRATION.md)·[송신](../functions/DOMAIN_TX_PORT.md)·[수신](../functions/DOMAIN_RX_ADAPTER.md) |
| 안정 접수·AGE·순서 | L03/M04/F11~F13 | 전체 의미 입력 사본 → 기존 CAN/UART Service |
| 바이트 변환 | L04/M05·M06/F14~F18 | [CAN22](../04_CAN_FD_OVERVIEW.md)·[UART-M23](../05_UART_M_OVERVIEW.md)의 정본 |
| 장치·저장소·관측 | L05/M07~M09/F19~F25 | Driver 접수·terminal·release → [수명](../functions/NETWORK_SERVICE_LIFETIME.md)·[실패/Health](../functions/NETWORK_FAILURE_HEALTH.md) |

같은 기능을 별도 Task나 Queue로 나누라는 뜻이 아니다. DOMAIN C2 호출은 Task 문맥에서 하며 실제 Core/Observer/TxPort 연결·배제·시간원은 G55-A01/A02의 BINDING_OPEN이다. 문서상의 지원 표와 실제 callback 등록은 다르다.

## Data와 기존 정확 원형

아래 선언은 `EXISTING_DECLARATION`이다. DOMAIN 공개 본체를 새로 작성하는 지시가 아니다. 원 Data는 [DOMAIN Data](../../interface/02_NODES/DOMAIN/10_DATA.md), 전체 선언은 [API](../../interface/02_NODES/DOMAIN/20_API.md), Network private 선언은 [Private Routing](../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md)에 있다. 기존 내부 제안의 세부 본체는 위의 B15 함수 정본을 사용한다.

```c
DomainIf_Status_t DomainIf_TxBcmDoorCommand(const DomainIf_BcmDoorCommand_t *command);
DomainIf_Status_t DomainIf_RxBcmCommandResult(const DomainIf_BcmCommandResult_t *result);
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);
DomainIf_Status_t DomainIf_TxMobileRequestResult(const DomainIf_MobileRequestResult_t *result);
NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);
NetSubmitResult DomainNetPriv_SubmitMobileRequestResult(const DomainIf_MobileRequestResult_t *result,
                                                      const DomainQueryResponseContext_t *query);
```

| 입력·호출 | 필드와 선행 조건 | 동기 결과·후속 경계 |
|---|---|---|
| `DomainIf_TxBcmDoorCommand` / DOMAIN 명령 Owner | 원 commandId·decisionSequence·origin·CommandContext 전체, hasRequestContext에 따른 RequestContext(sessionId/requestId), requestDeviceContextId, commandAgeMs와 SignalMeta를 안정 보존. 대상 Boot는 생산 세대와 별개 | C2 초기화/지원/인자/준비/용량 결과를 그대로 소비. OK는 접수이며 도어 동작 완료가 아님 |
| `DomainIf_RxBcmCommandResult` / CAN Adapter | 원 COMMAND_DOMAIN_BOOT+commandId+kind와 BCM 결과·reason·같은 결과의 doorEvidence. CAN에 없는 C2 confirmation을 만들지 않음 | Core 수용과 Observer 통지는 분리. 한 차례 Core 반영 후 Observer 오류로 공개 Rx 전체를 재호출하지 않음 |
| `DomainIf_RxNetworkLocalFailure` / Network 실패 관측자 | 실제 원 submission의 correlation과 아래 허용 원인, 현 callback epoch/token | 차후 DOMAIN의 요청 결과 정책은 DOMAIN 소유. 자기 원인이 아닌 차량 실행 실패를 합성하지 않음 |
| `DomainIf_TxMobileRequestResult` / DOMAIN 결과 Owner | 과거 원 RequestSession과 현재 수신 recipient Session을 구분. Network가 결과 상태를 판단하지 않음 | 17 schema 중 M_RESULT. 권한이 확인된 동일 단말에 과거 RequestSession A의 결과를 현재 recipient Session B로 보고해도 원 RequestSession을 B로 덮지 않음. 다른 Device의 기록을 공개하는 권한이 아님 |
| `DomainNetPriv_SubmitMobileRequestResult` / 기존 Query 또는 DOMAIN 경로 | result 전체 사본 및 query의 존재·QueryID·scope·current recipient가 해당 전송과 같은 건인지 확인 | 동기 접수와 Query의 실제 응답 TX는 별개. Getter18과 schema17 차이는 단일/최근목록 RequestResult 공유 때문 |
| `DomainNet_PublishPermission` / DOMAIN 원 허용 생산자 | vehicle_power_permission, power_permission_quality, power_operation_state, operation_allowed, operation_quality, update_sequence, source_age_ms의 실제 원 평가·현재 품질·합성 근거 | 서로 다른 C2 입력3개만 왔다고 완성 Permission3개를 생성하지 않음. 원 성공 평가와 CAN 합성 번호를 동일시하지 않고, 합성·AGE 근거 없으면 HOLD |

Permission에 quality_reason·relay revision·use_limit를 임의 추가하지 않는다. SignalMeta.ageMs와 commandAgeMs의 기산점도 다르다. 공통 문서의 BOOT_ID 예약0과 후보 SourceGeneration0 회신 대기를 분리하며 Generation0을 임의 정상값 또는 금지값으로 확정하지 않는다.

<a id="command-flow"></a>
## DOMAIN → BCM → DOMAIN → MOBILE의 조건별 흐름

1. DOMAIN Owner가 원 명령과 요청 문맥을 생산한다. 실제 등록된 TxPort를 공개 Tx가 호출한다. `NULL`, 잘못된 열거·범위·문맥이면 INVALID_ARGUMENT, 미초기화면 NOT_INITIALIZED, 필요한 Network Port 미등록·부재이면 NOT_AVAILABLE이다. UNSUPPORTED는 실제 미지원 기능의 별도 의미이며 ESP32 NULL callback 계약과 혼동하지 않는다. 이미 승인된 명령을 Network가 새 commandId로 발급하지 않는다.
2. [TxPort 정본](../functions/DOMAIN_TX_PORT.md)이 전체 입력을 caller와 비중첩 저장소에 복사한다. 실제 준비·문맥 검증 전에는 Queue에 게시하지 않는다. 준비 불충족은 NOT_AVAILABLE, 안정 사본/Queue 부족은 QUEUE_FULL이다. 성공 전에 입력과 귀속정보의 인수가 원자적으로 완료되어야 한다.
3. [명령 전송 흐름](../flows/DOMAIN_COMMAND_TO_NETWORK.md)이 접수 이후 미반영 대기를 AGE에 한 번 누적한다. 전송 전 원 문맥 폐기·기한 만료 시 해당 명령을 송신하지 않는다. Driver가 받았다면 참조와 terminal을 별도로 추적한다. 실제 CAN TX 성공은 BCM Application 인수·실행 성공이 아니다.
4. [BCM 경계](BCM.md)의 `void` App callback은 결과를 돌려주지 않는다. 실제 BCM 실행 결과가 CAN으로 왔을 때만 원 Boot/commandId/kind에 연결한다. 원 명령 기록이 없거나 재기동 뒤 과거 결과이면 새로운 명령에 붙이지 않는다. confirmation 증거가 없으면 기존 Rx Adapter의 UNCONFIRMED 후보를 유지한다. 해당 mapping/binding 자체가 미확정이면 영향받는 C2 투영만 HOLD하고 confirmation을 합성하지 않는다.
5. DOMAIN Owner가 자기 결과 정책에 따라 M_RESULT를 요청한다. BLE 송신이나 App 표시 여부는 [ESP32/MOBILE 경계](ESP32_MOBILE.md)의 외부 계약이다. Network는 CAN 성공을 M_RESULT DONE으로 합성하지 않는다.

### 접수 반환과 후발 결과를 분리하는 C형 연결 의사코드

`bool NetNodeDomain_MapSubmit(NetSubmitResult submit, DomainIf_Status_t *out)`은 L02/M03/F07의 `PROPOSED_PRIVATE_FUNCTION`이다. 기존 호출 결과를 C2 의미로 바꾸는 부수효과 없는 문서상 부분 함수이며 공개 API가 아니다. caller는 Network 결과를 소비하는 Adapter이고 `submit`은 위 정본의 검증·사본·원자 게시를 수행한 실제 반환값이다. `out`은 caller가 소유한 writable 단일 객체이며 실패에서는 변경하지 않는다. 실제 반환 배치와 비중첩·직렬화 증거는 BINDING_OPEN이다.

```c
bool NetNodeDomain_MapSubmit(NetSubmitResult submit, DomainIf_Status_t *out)
{
    if (out == NULL) {
        return false;
    }
    switch (submit) {
    case NET_SUBMIT_OK:
    case NET_SUBMIT_INVALID_ARGUMENT:
    case NET_SUBMIT_NOT_READY:
    case NET_SUBMIT_NO_CAPACITY: break;
    default: return false;
    }
    *out = NetDomain_MapSubmitStatus(submit);
    return true;
}
```

**명명된 설계 호출자와 단일 정본:** 아래 `DOMAIN command-flow / M03 F07` 연결 조각이 기존 submit 반환을 검사할 때 호출한다. 네 상태의 C2 대응표는 [NetDomain_MapSubmitStatus](../functions/DOMAIN_TX_PORT.md#status-map)에만 두고 이 부분 mapper는 알려진 입력인지 확인한 뒤 그 정본에 위임한다. 전체 mapper의 unknown→NOT_AVAILABLE 후보를 부분 mapper의 성공으로 확정하지 않는다. 별도 runtime wrapper가 반드시 필요하다는 뜻은 아니며 실제 구현에서는 이 guard를 기존 Adapter에 인라인할 수 있다. 현재 문서의 기존 함수 ID와 실패 OUT 불변 계약은 보존한다.

```c
// DOMAIN command-flow / M03 F07: 실제 기존 submit의 반환을 관측하는 조각
DomainIf_Status_t mappedStatus;
bool mappedKnown = NetNodeDomain_MapSubmit(actualSubmitReturn, &mappedStatus);
if (mappedKnown) {
    // Adapter 호출자가 mappedStatus를 로컬 접수 결과로만 소비한다.
} else {
    // mappedStatus를 읽지 않는다. 원 unknown 반환과 바인딩 미확인을 보존한다.
}
```

false이면 caller는 OUT을 소비하지 않고 해당 바인딩 오류를 격리한다. 원 C2 API 문서에는 unknown 내부 반환을 NOT_AVAILABLE로 제한하는 후보 방침이 있으나 실제 채택은 OPEN이다. 이 부분 mapper는 그 공개 fallback을 확정하지 않는다. 네 값 외의 결과를 처리할 실제 제품 방침은 별도 바인딩이며 새로운 C2 enum을 만들어 반환하지 않는다. true/DOMAIN_IF_OK에서도 BCM 결과·차량 완료·UI 완료 상태는 바꾸지 않는다. 재시도는 원 요청·기한·멱등성 근거를 다시 검사해야 하며 실패 반환만으로 commandId를 교체하지 않는다.

<a id="rx-query"></a>
## CAN/UART 수신 → C2 → Query의 인계

형식 검증, 원 Boot/순번 수용, 값·품질·AGE 투영, C2 Core 수용, Observer 소비를 순서대로 구분한다. [수신 Adapter](../functions/DOMAIN_RX_ADAPTER.md)는 성공 OUT만 소비하고 partial/unknown commit에서는 재인계하지 않는다. CAN raw와 Snapshot 값이 같아도 동일 사건으로 보지 않는다. 동일 입력의 수용 결과와 Snapshot의 실제 association이 없으면 해당 raw 기반 UART 투영만 HOLD하며 검증된 다른 독립 경로는 유지한다.

Query에서는 Getter 실패 OUT을 읽지 않고 기존 Query의 원 recipient/Boot/QueryID/scope를 끝까지 보존한다. 응답 enqueue 횟수를 실제 final TX 수로 세지 않는다. End enqueue 후 TX 실패이면 Query 완료를 주장하지 않는다. `SessionReady=0`의 첫 Query를 일괄 거절하는 규칙을 만들지 않으며, 실제 현재 문맥·schema·scope 검증은 그대로 적용한다. 상태보고의 품질 열화는 보존하되 Query 성공 때문에 fresh로 승격하지 않는다.

## 메모리·오류·복구

| 관측 | 허용 처리 | 금지 처리 |
|---|---|---|
| 호출 중 const 입력 | 송신/비동기 보유는 사본 또는 동기 encode 후 반환; 동기 Rx/App 인계는 호출 동안 유효한 borrow 사용. callback과 pointed 객체 수명을 분리 | 반환 뒤 caller stack 또는 원 RX buffer 참조 유지 |
| Driver 접수 전 거절 | 안정 Queue 인수 여부를 기준으로 해당 저장소·예약 정리 | 제출 거절을 수신 노드 실행 실패로 표시 |
| release가 terminal보다 먼저 | 해당 프레임 참조만 해제; 원 결과·token/epoch·예약 기록은 terminal 또는 확인된 정리까지 유지 | 메모리 해제를 명령 성공이나 모든 record 재사용으로 해석 |
| terminal이 release보다 먼저 | 결과 관측은 한 번 기록; Driver 참조는 해제 근거까지 유지 | terminal만 보고 참조 메모리 재사용 |
| 재기동 뒤 late callback | 이전 epoch/token으로 격리하고 이전 submission의 정리에만 사용 | 현재 Boot의 명령 또는 같은 숫자 token에 귀속 |
| NetworkLocalFailure | 최종 Driver 제출 실패·전송 전 만료·전송 전 문맥 무효의 기존3범위와 실제 원 identity | 모든 in-flight 실패·주기 프레임 손실·Query 종료 오류를 공개 실패로 확장 |

수명 구체 조건·순서 판단 의사코드는 [공통 Service](../functions/NETWORK_SERVICE_LIFETIME.md), 공개 실패는 [실패/Health](../functions/NETWORK_FAILURE_HEALTH.md)를 따른다. Network Availability 문서에는 ITEM_COUNT=9 제안이 존재하지만 실제 Header/ABI 확인은 OPEN이다. raw 통신 관측과 DOMAIN의 가용성 판단을 섞지 않는다.

## 노드 자체검수와 남은 경계

CAN TX 성공/BCM 실패, 오래된 Boot 결과, 접수 후 입력 변경, 먼저 온 release, Core 반영 후 Observer 실패, 응답 전송 실패 뒤 End enqueue, Generation0 미결을 각각 위 분기에 대입했다. 각 경우 차량 성공 합성·참조 조기 재사용·원 식별 교체·미결 확정이 발생하지 않는다. 이 검토는 문서와 기존 함수의 연결 자체검수이며 제품 실행 시험이 아니다.

IF-O01/02/08/09/23 등 실제 Owner와 C2 binding, G55-A01~A05의 기술 조건은 계속 OPEN이다. r1 [기술 제약과 연동 조건](../../interface/06_TECHNICAL_CONSTRAINTS.md)을 참조한다. 이 문서는 Owner35 원대장이 아니며 기존 로컬·봉인 원장과 제출물 S6Q-01~14 원 이력을 별도로 보존한다. Interface 협의 반영은 S6 완료 후 별도 승인 범위에서 한다. B16이 공개72/TxPort26/Observer11을 승인된 제품 구현으로 바꾸지 않는다.

[노드 전체](../08_NODE_BOUNDARIES.md) · [BCM](BCM.md) · [CIS/Pi](CIS_PI.md) · [VSS](VSS.md) · [WINDOW](WINDOW.md) · [ESP32/MOBILE](ESP32_MOBILE.md)
