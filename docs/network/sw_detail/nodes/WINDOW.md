# N07 WINDOW — 실행 보류 상태의 이동·STOP·보고 경계

[노드 경계 전체](../08_NODE_BOUNDARIES.md) · [노드·링크 지도](../02_NODE_LINK_MAP.md) · [Layer·Module·Function 색인](../03_LAYER_MODULE_INDEX.md) · [Interface 개요](../../interface/02_NODES/WINDOW/00_OVERVIEW.md) · [Data](../../interface/02_NODES/WINDOW/10_DATA.md) · [API](../../interface/02_NODES/WINDOW/20_API.md)

**IMPLEMENTATION_HOLD / DESIGN_PRESERVED.** 이 문서는 예정 S32K144의 Network 경계를 보존한다. 현재 WINDOW 제품 실행·송수신 활성·표시·timeout 감시를 개시하지 않는다. 이동·끝단·끼임 보호·실제 결과는 WINDOW App 소유이며 Network가 CAN 왕복으로 로컬 보호를 제어하지 않는다.

## 적용 범위와 기존 정확 원형

| Node / Link | 설계 경계 | 현재 상태 |
|---|---|---|
| N07 / P01 | 공유 CAN의 WINDOW 전용5 Wire와 공유 DOMAIN_PERMISSION WINDOW 수신 | DESIGN_ONLY, 실행·감시 비활성 |
| N07 / R04 | DOMAIN 허용·명령 → WINDOW, State/Event/Result/Fault → DOMAIN | 양방향 IMPLEMENTATION_HOLD |
| N07 / I07 | WINDOW App↔Network typed 인계6개 | DEFERRED / PROPOSED_DESIGN / BINDING_OPEN |
| 직접 MOBILE 창문 제어·WINDOW BLE/UART | 현재 범위 없음 | NOT_APPLICABLE. 기존 M_WINDOW_STATE/FAULT 2종은 별도 표시 HOLD |

Layer 적용은 L02~L05 설계만이다. M03/F10/ST04의 typed 경계, M04/F11~13/ST05~07의 사본·시간·순서, M05/F14~15의 CAN Codec, M07~09/F19~25/ST09~12의 Driver 수명·관측을 재사용한다. 수신 의무는 전용 CAN5와 공유 Permission의 WINDOW 수신1을 합한 **6건의 설계**다. 전체 CAN22/Rx25 및 전체 Wire48/Rx51 분모를 바꾸지 않는다.

<a id="apis"></a>

| 정확한 기존 원형 | 원 생산자 → 호출/소비 Owner | 메시지 정본 |
|---|---|---|
| `void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | DOMAIN 원 판단 → WINDOW Network → WINDOW 동작 App | [DOMAIN_PERMISSION](../functions/can/DOMAIN_PERMISSION.md) |
| `void WindowApp_OnDomainCommand(const NetWindowCommand *message, const NetCanRxMeta *meta);` | DOMAIN 원 목표 → WINDOW Network → WINDOW 동작 App | [WINDOW_COMMAND](../functions/can/WINDOW_COMMAND.md) |
| `NetSubmitResult WindowNet_SendEvent(const NetWindowEvent *message);` | WINDOW 끝단/끼임 발생 Owner → WINDOW Network → DOMAIN 소비 | [WINDOW_EVENT](../functions/can/WINDOW_EVENT.md) |
| `NetSubmitResult WindowNet_SendResult(const NetWindowResult *message);` | WINDOW 실행 Owner → WINDOW Network → DOMAIN 결과 소비 | [WINDOW_RESULT](../functions/can/WINDOW_RESULT.md) |
| `NetSubmitResult WindowNet_PublishState(const NetWindowState *message);` | WINDOW 실제 상태 평가 → WINDOW Network → DOMAIN 상태 소비 | [WINDOW_STATE](../functions/can/WINDOW_STATE.md) |
| `NetSubmitResult WindowNet_PublishFault(const NetWindowFault *message);` | WINDOW 진단 Owner → WINDOW Network → DOMAIN Fault 소비 | [WINDOW_FAULT](../functions/can/WINDOW_FAULT.md) |

이 원형은 Interface의 **EXISTING_DECLARATION**이며 실제 설치된 구현이 아니다. 별도 공개 wrapper나 DOMAIN C2 본체를 생성하지 않는다. 각 구조체·enum·필드 범위·Wire Codec은 위 정본을 그대로 사용한다. Rx `message/meta`는 NULL 불가 IN, 같은 검증 수신 사건의 borrowed 자료다. `void`는 수용·복사·이동·정지·보호 완료를 보고하지 않는다. 소비자가 반환 뒤 사용할 값·원 식별·시각을 복사한다는 실제 계약이 없으면 정상 호출을 약속하지 않는다.

Tx 네 함수는 NULL/범위·조합 오류에 INVALID_ARGUMENT, 준비/원 문맥/HOLD에 NOT_READY, 안정 저장소 부족에 NO_CAPACITY, 완전 사본·원자 Queue 게시에만 OK라는 기존 NetSubmitResult 의미를 유지한다. 현재는 WINDOW 실행 HOLD를 해소하지 않았으므로 형식이 맞아도 정상 실행 접수는 열지 않는다. 한 함수 원형이 있다는 사실로 support를 true로 만들지 않는다.

<a id="command-stop"></a>
## 명령·허용과 로컬 보호를 연결하는 순서

아래는 **향후 별도 재개 승인과 Owner 바인딩 후** 적용할 설계다. 현재 경로는 첫 실행 준비 경계에서 HOLD한다. 단순 `enabled=true` 플래그나 숫자 profile 선택으로 재개 승인을 대신하지 않는다.

1. DOMAIN은 원 `commandId`, 명령 birth Age, 원 판단 meta를 보존한다. Network는 확인된 대상 WINDOW Boot와 원 문맥을 연결한다. `request_sequence↔C2 commandId`의 결과 대응, source_context 축약, VENT70↔80, 수용한도200ms↔1초는 아직 미합의다.
2. WINDOW의 기존 `NetCan_DecodeWindowCommand()`가 flags/DLC/length/padding/모든 필드를 검증한다. `target_channel=0`, 닫힘비율0~100, OPEN/CLOSE/STOP/VENT/MOVE_TO_POSITION 및 source_context 코드를 명시적으로 검사한다. Codec 성공은 명령 사용 승인과 다르다.
3. [공통 Rx](../functions/CAN_TRANSFER_CONTROL.md#rx-order)가 수용된 DOMAIN Boot·대상 WINDOW Boot·원 command identity와 불변 본문·TX 순서·원 명령 Age를 대조한다. sourceGeneration0 정책과 Wire Boot0 예약을 혼동하지 않는다. 불명확한 wrap/Boot 재사용·현재 기동·시간/정확한 deadline 비교는 해당 ORDER_HOLD/TIME_HOLD/CONTRACT_HOLD다.
4. 같은 명령 재전송은 높은 TX라도 새로운 움직임을 만들지 않는다. 원 commandId가 같고 본문이 달라지면 충돌 보존·HOLD다. 수용 시점까지 실제 미반영 경과를 누적하며 Queue·재수신·reboot로 Age0을 만들지 않는다.
5. 새 이동에는 유효한 현재 동작 permission이 필요하다. 정당한 DOMAIN STOP은 계속 이동할 permission 상실만으로 수송을 막지 않는다. **STOP도 위 출처·대상 Boot·명령 ID·순서·Age·채널·형식 검증은 그대로 통과해야 한다.** 오래된 STOP, 다른 대상의 STOP 또는 확인되지 않은 출처를 이 예외로 통과시키지 않는다.
6. callback 수명·소비 준비와 원자 인계 보장 후 `WindowApp_OnDomainCommand()`를 호출한다. 반환 `void`로 실제 정지·출력 차단을 기록하지 않는다. WINDOW의 로컬 STOP·AntiPinch는 이 호출·DOMAIN 허용·CAN 응답을 기다리지 않는다. 로컬 우선순위 **AntiPinch > Local Switch > DOMAIN**을 유지한다.
7. WINDOW 실행 Owner가 실제 상태/결과/발생을 아래 각 별도 경계로 생산한다. permission 복구나 CAN 복구만으로 과거 움직임을 자동 재개하지 않는다.

<a id="stop-selection"></a>
## 내부 C형 의사코드 — 검증 완료 명령의 마지막 수송 선택

**PROPOSED_PRIVATE_FUNCTION**, L02/M03/F10의 좁은 후단 분기다. 입력은 기존 [WINDOW_COMMAND](../functions/can/WINDOW_COMMAND.md)의 전체 검증과 [NetCan_ClassifyRx](../functions/CAN_TRANSFER_CONTROL.md#rx-order) 분류가 끝난 동일 사건이어야 한다. 이 함수가 source/Boot/Age 검사나 현재 실행 HOLD를 대체하지 않는다. 현 구성에서는 실행 경로에 연결하지 않는다.

```c
NetCanResult NetWindowBoundary_SelectCommandDelivery(
    const NetWindowCommand *message,
    NetCanRxAction acceptedAction,
    bool movementPermissionUsable);

NetCanResult NetWindowBoundary_SelectCommandDelivery(
    const NetWindowCommand *message,
    NetCanRxAction acceptedAction,
    bool movementPermissionUsable)
{
    if (message == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (message->window_command < NET_WINDOW_COMMAND_OPEN ||
        message->window_command > NET_WINDOW_COMMAND_MOVE_TO_POSITION ||
        message->target_channel != 0 || message->target_closed_percent > 100 ||
        message->source_context > NET_SOURCE_CONTEXT_TEST)
        return NET_CAN_MALFORMED;
    switch (acceptedAction) {
    case NET_CAN_RX_EXACT_DUPLICATE:
    case NET_CAN_RX_SAME_INPUT:
        return NET_CAN_DUPLICATE; // 같은 원 명령을 새 실행 입력으로 보내지 않는다.
    case NET_CAN_RX_NEW_INPUT:
        break;
    default:
        return NET_CAN_INVALID_ARGUMENT;
    }
    if (message->window_command == NET_WINDOW_COMMAND_STOP)
        return NET_CAN_OK;
    if (!movementPermissionUsable) return NET_CAN_NOT_READY;
    return NET_CAN_OK;
}
```

`message`는 IN/borrowed, `acceptedAction`은 동일 원 명령의 실제 공통 분류 결과다. `movementPermissionUsable`은 Owner가 현재 permission의 값·품질·원 기한을 검증한 **런타임 판단 결과**이며 `wire permission!=0`, `수신됨`, `팀 바인딩 완료` flag가 아니다. NO_DATA/INVALID/STALE/만료를 사용불가 false로 분류할 수 있어도 원 품질을 정상 permission=false로 다시 저장하지 않는다. STOP에는 이 값이 true일 필요가 없다. 전체 바인딩 또는 명령의 공통 검증 근거가 미확정이면 호출 전에 HOLD다. OUT/저장소·Driver·App 부수효과는 없다. OK는 후단 수송 선택만 뜻하며 실제 callback·물리 STOP을 수행하지 않는다. DUPLICATE에는 새 호출을 하지 않되 SAME_INPUT의 높은 TX와 보수적 Age 원자 갱신은 앞선 공통 단계가 책임진다. NOT_READY는 새 이동 허용 부족이며 기능 Result를 생성하지 않는다.

선행조건이 빠진 호출을 허용하는 공개 Guard API로 제품에 추가하지 않는다. 실제 구현 시 같은 Owner의 기존 Dispatcher 내부 분기로 흡수할 수 있는 제안이다. MCU 환경의 비중첩 저장소·직렬화·실제 callback 소비 보장은 여전히 외부 바인딩 항목이다.

**명명된 설계 호출자:** [WINDOW_VSS_HEALTH의 SC11 조건부 인계](../flows/e2e/WINDOW_VSS_HEALTH.md#sc11-normal) 안의 `WINDOW command-stop / M03 F10` 단계가 selector를 호출한다. 기존 `NetCan_DecodeWindowCommand`와 `NetCan_ClassifyRx` 이후, `NetCan_DeliverWindowCommand` 이전의 한 번의 수송 선택이다. 현재 WINDOW 실행 HOLD에서는 이 연결 조각 전체를 실행하지 않는다. 실제 Task/Superloop·Owner 공급 함수는 BINDING_OPEN이다.

```c
// WINDOW command-stop / M03 F10: 재개 승인·공통 검증이 입증된 동일 사건만
NetCanResult selection = NetWindowBoundary_SelectCommandDelivery(
    &acceptedMessage, acceptedAction, movementPermissionUsable);
if (selection == NET_CAN_OK) {
    NetCanResult delivered = NetCan_DeliverWindowCommand(&acceptedMessage, &acceptedMeta);
    // delivered는 기존 typed 인계 결과다. 실제 이동/STOP 완료를 만들지 않는다.
} else {
    // DUPLICATE/NOT_READY/오류를 원 사건에 보존하고 App을 새로 호출하지 않는다.
}
```

위 메시지와 meta는 같은 검증 수신의 caller-owned 안정 사본이며 이 조각 동안 유효하다. selector가 입력을 수정하거나 새로운 permission/AGE를 생산하지 않는다. 뒤의 기존 Deliver 함수도 callback 지원·품질·시간·실행 HOLD를 독립적으로 지키므로 selector OK가 그 검사를 우회하지 않는다.

<a id="report-semantics"></a>
## State·Event·Result·Fault의 별도 생산과 DOMAIN 수용

| 보고·Data | 원 의미·필드 연결 | DOMAIN 쪽 남는 조건 |
|---|---|---|
| `NetWindowState` | 실제 motion/ECU/position, fully_open/closed, protection/reverse, 원 update_sequence/source_age_ms | UNKNOWN255를 bool false·위치0으로 만들지 않음. C2 품질·보호 상태 축약 근거 OPEN |
| `NetWindowEvent` | 새 끝단/끼임 `event_kind`, WINDOW Boot+occurrence, 선택 related_domain_boot/request_sequence, origin, event_age/quality | END는 AntiPinch 입력이 아님. END 소비 owner OPEN; ANTIPINCH에도 `outputDisabled/movementStopped` 실제 확인 근거가 추가로 필요 |
| `NetWindowResult` | 원 request_domain_boot/request_sequence/target_channel와 실제 result/reason | commandId 대응·결과 meta의 원 품질/Age/update 근거 OPEN. TX_SEQUENCE·수신시각으로 채우지 않음 |
| `NetWindowFault` | 현재/복구 fault_status와 occurrence/update/source_age | 좁은 FaultInfo_t ingress의 품질·발생/세대·Age 보존 Owner OPEN. MOBILE projection을 ingress 증거로 쓰지 않음 |

같은 fully_open_state 반복은 새로운 FULLY_OPENED 발생이 아니다. State의 protection DONE도 실제 outputDisabled·movementStopped를 Wire가 제공했다는 뜻이 아니다. timeout·CAN ACK·새 frame 한 개로 보호 완료를 생성하지 않는다. Fault 해제와 최근 occurrence 보존은 공존할 수 있으며 RECOVERING이 모터 재개 허용도 아니다.

State/Fault의 아직 송신 미시작인 같은 의미 키만 기존 pending 정책에서 교체 가능하다. Event/Result는 원 발생/명령 단계별로 보존한다. Queue full이면 기존 discrete를 조용히 지우지 않는다. terminal Result 뒤 늦은 ACCEPTED/IN_PROGRESS로 되돌리지 않으며 상반된 최종 결과는 충돌 HOLD다. Driver 참조 중 frame의 값·Age·순번은 불변이다.

<a id="holds"></a>
## 감시·재기동·재개에 필요한 근거

현재 미활성 WINDOW는 필수 online 대상으로 등록하지 않으며 Rx deadline 경과만으로 차량 구성에 상시 통신 Fault를 만들지 않는다. 기존 설계의 주기/timeout 값은 활성 노드에서 검토할 후보이지 현재 감시 개시 지시가 아니다. DOMAIN의 기존 Health snapshot에 가짜 WINDOW 정상/비정상 상태를 추가하지 않는다.

향후 기동/재기동은 원 WINDOW 세대, 원 DOMAIN 명령 Boot, 로컬 Driver token/epoch를 구별한다. old callback을 새 명령에 귀속하지 않고 새 Boot 뒤 과거 이동을 자동 전송하지 않는다. 참조 release와 terminal 결과·필수 소비를 [B15 Service 수명](../functions/NETWORK_SERVICE_LIFETIME.md#retirement)대로 분리한다. 참조 해제만 먼저 확인돼도 원 Source 예약은 terminal/필수 소비가 끝날 때까지 남긴다.

재개 전 확인할 Owner 항목은 기존 IF-O07/IF-O18/IF-O24·G05 아래에 유지한다: 제품 활성 범위, commandId association·Boot lifecycle, END consumer, 실제 보호 확인, Fault provenance/quality, enum/UNKNOWN 투영, VENT70/80·수용200ms/1초, Driver/clock·callback 동기화·로컬 보호 시험. 이 문서 검수는 그 결정을 완료하지 않는다. 협의는 S6 후 통합 큐에 기록만 하며 B16에서 Owner 미결35개를 증감·종결하지 않는다.

## 문서 반례

| ID | 입력·사건 | 정적 기대 |
|---|---|---|
| B16-WIN-01 / 필수6 | 미활성 WINDOW의 마지막 Rx 시각 부재·deadline 경과 | 실행/감시 HOLD 유지. 현재 차량 구성에 상시 timeout Fault 생성 없음 |
| B16-WIN-02 | 향후 유효한 새 STOP, 이동 permission만 상실 | 다른 공통 검사 통과 시 후단 수송 선택 가능. 실제 정지 성공은 App 근거 필요 |
| B16-WIN-03 | STOP이지만 이전 DOMAIN Boot/다른 target/만료 Age | 후단 selector 이전 ORDER_HOLD/TIME_HOLD, STOP 예외로 통과하지 않음 |
| B16-WIN-04 | permission 회복, 과거 CLOSE가 pending | 옛 움직임 자동 재개 없음; 원 문맥·기한과 별도 Owner 결정 필요 |
| B16-WIN-05 | FULLY_CLOSED Event 또는 protection DONE 수신 | AntiPinch 전체 확인·outputDisabled=true 합성 없음 |
| B16-WIN-06 | WINDOW_STATE 위치255/끝단255 | UNKNOWN 보존, 0%/false로 정상화 금지 |
| B16-WIN-07 / 필수9·10 | release 먼저/terminal 늦음 또는 옛 epoch callback | 원 예약·identity 유지, 새 제출·새 buffer 변경 없음 |

근거는 현행 WINDOW 00/10/20 전문, S4A api/window, B12 CAN 노드 Flow/공통 수명, S4C 시간·자원 계약과 B15 Service다. 외부 HOLD를 유지한다. 실제 제품/SDK/보드/모터·보호 시험은 **NOT_EXECUTED**다.
