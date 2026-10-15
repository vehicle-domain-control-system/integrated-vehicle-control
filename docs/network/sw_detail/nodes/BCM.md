# N02 BCM — 명령 수신과 실제 실행 결과의 Network 경계

[노드 경계 전체](../08_NODE_BOUNDARIES.md) · [노드·링크 지도](../02_NODE_LINK_MAP.md) · [Layer·Module·Function 색인](../03_LAYER_MODULE_INDEX.md) · [Interface 개요](../../interface/02_NODES/BCM/00_OVERVIEW.md) · [Data](../../interface/02_NODES/BCM/10_DATA.md) · [API](../../interface/02_NODES/BCM/20_API.md)

BCM은 도어·공조·실내등을 실행하고 실제 상태·결과·보호 발생을 생산한다. Network는 원 DOMAIN 명령을 BCM 실행 담당에게 전달하고 BCM의 보고를 DOMAIN으로 운반한다. CAN 송신 성공과 BCM 실행 성공 사이에 있는 책임을 구별하기 위한 문서다. **PROPOSED_DESIGN / BINDING_OPEN**이며 S32K144의 실제 설치·실행 완료를 뜻하지 않는다. EXTERIOR_LIGHT를 추가하지 않는다.

## 적용 구조와 경계

| Node / Link | 실제 경계와 Owner | 적용 책임·상태 |
|---|---|---|
| N02 / P01 | 공유 CAN의 BCM 송수신. 다른 ECU의 정상 수신은 BCM 또는 DOMAIN의 생존 증거가 아님 | L03~L05, M04/M05/M07~M09; CAN profile·Driver 실제 바인딩 OPEN |
| N02 / R01 | DOMAIN 명령3종 → BCM; BCM 상태3·Result·Event·Status → DOMAIN; 공유 Permission 수신 | L02~L05, M03/M04/M05/M07~M09; 아래 정확한 API와 메시지 연결 |
| N02 / I02 | BCM Network→BCM App 목표 전달; BCM App→Network 실제 보고 | F10/F11, ST04/ST05. App 실행·진단·로컬 보호는 BCM 소유 |
| 해당 없음 | BCM 직접 BLE/UART-M/UART-C, BCM에 대한 원격 Query RPC | NOT_APPLICABLE. MOBILE 조회는 DOMAIN의 기존 snapshot 경계 |

수신의 F13/F14/F20은 ST07/ST10에 같은 frame·typed·meta를 보존한다. 송신 F11/F12/F15/F19~23은 ST05/ST06/ST09/ST11, 실제 관측 F24/F25는 ST12를 사용한다. DOMAIN 쪽 원 명령·결과 연결은 기존 M01/ST01~02 및 [DOMAIN Adapter](../07_DOMAIN_ADAPTER_SERVICE.md)를 따른다. 새로운 Layer·Module·F번호·차량 상태 저장소를 만들지 않는다.

<a id="apis"></a>
## 정확한 기존 선언과 모든 사용 방향

다음 10개 원형은 Interface의 **EXISTING_DECLARATION**이며 설치 헤더가 확인됐다는 뜻은 아니다. 모두 `PROPOSED_DESIGN / BINDING_OPEN`이다. 전체 구조체·enum·필드 범위는 위 Data 정본, Wire/Codec은 아래 메시지 정본에 한 번만 둔다.

| 정확한 원형 | 원 생산자 → 호출자 → 구현/소비 Owner | Wire·구체 Function |
|---|---|---|
| `void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta);` | DOMAIN 기능 → BCM Network → BCM 도어 App | [BCM_DOOR_COMMAND](../functions/can/BCM_DOOR_COMMAND.md#rx) |
| `void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta);` | DOMAIN 기능 → BCM Network → BCM 공조 App | [BCM_CLIMATE_COMMAND](../functions/can/BCM_CLIMATE_COMMAND.md#rx) |
| `void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta);` | DOMAIN 기능 → BCM Network → BCM 실내등 App | [BCM_LIGHT_COMMAND](../functions/can/BCM_LIGHT_COMMAND.md#rx) |
| `NetSubmitResult BcmNet_SendResult(const NetBcmResult *message);` | BCM 실행 담당 → BCM Network → DOMAIN 결과 소비 | [BCM_RESULT](../functions/can/BCM_RESULT.md) |
| `NetSubmitResult BcmNet_SendEvent(const NetBcmEvent *message);` | BCM 보호·복구 담당 → BCM Network → DOMAIN 발생 소비 | [BCM_EVENT](../functions/can/BCM_EVENT.md) |
| `NetSubmitResult BcmNet_PublishDoorState(const NetBcmDoorState *message);` | BCM 도어 평가 → BCM Network → DOMAIN 현재 상태 | [BCM_DOOR_STATE](../functions/can/BCM_DOOR_STATE.md) |
| `NetSubmitResult BcmNet_PublishClimateState(const NetBcmClimateState *message);` | BCM 공조 평가 → BCM Network → DOMAIN 현재 상태 | [BCM_CLIMATE_STATE](../functions/can/BCM_CLIMATE_STATE.md) |
| `NetSubmitResult BcmNet_PublishLightState(const NetBcmLightState *message);` | BCM 실내등 적용 평가 → BCM Network → DOMAIN 현재 상태 | [BCM_LIGHT_STATE](../functions/can/BCM_LIGHT_STATE.md) |
| `NetSubmitResult BcmNet_PublishStatus(const NetBcmStatus *message);` | BCM 진단 → BCM Network → DOMAIN BCM 진단 소비 | [BCM_STATUS](../functions/can/BCM_STATUS.md) |
| `void BcmNetPriv_OnDomainPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);` | DOMAIN 판단 → BCM Network 내부 감독 | [DOMAIN_PERMISSION](../functions/can/DOMAIN_PERMISSION.md); App 필수 permission API 추가 없음 |

`message/permission/meta`는 NULL 불가 IN이며 호출 중 읽기 전용이다. Rx callback 동안 borrowed이고 이후 필요한 전체 값·원 식별·시각은 BCM App이 반환 전에 복사한다. `void` 반환으로 그 복사 성공을 관측할 수 없으므로 소비자 등록만으로 안정 인계를 보장하지 않는다. 실제 동기 소비 또는 복사·저장 성공 보장과 실패 관측을 BCM Owner가 바인딩해야 한다. 미확인 경로는 `BINDING_OPEN`이다. ISR에서는 위 의미 API를 호출하지 않는다. Task/Superloop 중 실제 문맥과 직렬화는 해당 노드 환경에서 확인한다.

Tx 입력은 Network가 OK 전에 caller와 독립인 전체 사본·원 identity·시간 근거·Queue 엔트리를 함께 확보한다. `NET_SUBMIT_INVALID_ARGUMENT`는 NULL/필드 조합 오류, `NET_SUBMIT_NOT_READY`는 현재 원 문맥·profile·필수 소비/실패 경계 미준비, `NET_SUBMIT_NO_CAPACITY`는 유한 저장소 부족이다. 어느 실패도 새 항목을 부분 게시하지 않는다. `NET_SUBMIT_OK`는 안정 접수만 보장한다. 상대 DOMAIN의 공개 함수를 BCM MCU에서 직접 호출하지 않는다.

<a id="command-result-flow"></a>
## DOMAIN 명령 → BCM 실행 → DOMAIN 결과 → MOBILE

1. DOMAIN 기능 Owner가 원 `commandId`, 명령 생성 Age, 판단 자료의 meta, 선택적 원 MOBILE Request를 생성한다. [TxPort 정본](../functions/DOMAIN_TX_PORT.md)은 전체 Context를 보존해 기존 CAN callback에 연결한다. `target_boot_id`는 마지막 정상 BCM 상태와 채택된 Boot 수명 근거가 있어야 하며 기본 0 또는 현재 DOMAIN Boot를 대입하지 않는다.
2. DOMAIN Network가 원 명령 사본을 접수한 뒤 [전송 제어](../functions/CAN_TRANSFER_CONTROL.md#admission)의 준비·원자 게시·만료 재검증을 수행한다. Driver accepted·terminal TX·reference release를 별도 관측한다. CAN ACK는 BCM의 의미 수신이나 실행 확인이 아니다.
3. BCM 수신 경로는 실제 frame 오류/flags/DLC/length/padding/필드 → 원 DOMAIN Boot·TX 순서 → 대상 BCM Boot → 원 command identity·본문 → 소비 시점 Age → callback 준비를 검사한다. 명령 한도 200ms 후보↔구 1초 및 `>`/`>=` 비교 경계는 미합의다. 수신 필드의 ACCEPT_LIMIT 숫자만 믿어 정책을 채택하지 않는다. 미확인은 TIME_HOLD 또는 CONTRACT_HOLD로 해당 경로를 제한한다.
4. 기존 `NetCan_DecodeBcmDoorCommand()` → `NetCan_ClassifyRx()` → 새 입력에만 `BcmApp_OnDomainDoorCommand()`의 순서를 따른다. 정확한 전체 호출 절차는 [기존 Rx](../functions/can/BCM_DOOR_COMMAND.md#rx)에 있다. Climate/Light는 위 표의 대응 함수로 같은 구조를 적용한다. SAME_INPUT은 원 보수적 Age·높은 TX 원장을 함께 갱신할 뿐 새 구동을 요청하지 않는다. 동일 Boot/commandId의 상반된 본문은 충돌 HOLD다.
5. BCM 실행 Owner가 로컬 보호·실제 피드백을 확인하고 `BcmNet_SendResult()`에 원 `command_domain_boot/command_id/command_kind` 및 실제 `result/result_reason`을 제출한다. 선택 도어 발생은 같은 Result의 `door_event/occurrence_id/event_age_ms/event_quality/door_target`과 원자적으로 인계한다. 이를 BCM_EVENT로 복제하지 않는다.
6. DOMAIN은 BCM 송신 Boot와 원 DOMAIN 명령 Boot를 구별해 Result를 해당 명령에 연결한다. [BCM_RESULT의 C2 투영](../functions/can/BCM_RESULT.md)을 거쳐 공개 `DomainIf_RxBcmCommandResult`의 실제 Owner 처리를 관측한다. `NetBcmResult`에 없는 `confirmation`을 DONE/CAN ACK로 CONFIRMED 생성하지 않는다. 실제 확인 근거·Boot 대응·결과 meta가 미입증이면 해당 투영 HOLD다.
7. DOMAIN이 원 요청과 결과를 연결한 후 기존 [하향 경로](../flows/UART_M_DOWNLINK.md)의 M_RESULT로 전달한다. 원 요청 Session/RequestId는 현재 수신자 Session과 구별한다. BCM 상태 report, 원 BCM_RESULT, DOMAIN의 MOBILE 결과는 서로 다른 경계이며 Network가 상태값 일치만으로 새 결과를 생성하지 않는다. UART 최종 TX·ESP32 수신·BLE 인계·MOBILE 표시도 각각 별도 증거다.

BCM이 실행하지 못했으면 실제 거부·실패 결과의 생산자는 BCM 실행 Owner다. CAN 송신 성공으로 Network가 DONE을 생성할 수 없고, BCM 결과 자체가 오지 않은 경우에도 Network timeout을 BCM의 실제 실패 사유로 꾸밀 수 없다. DOMAIN은 통신 관측과 명령 처리 정책으로 미확정 상태를 다룬다.

<a id="correlation-function"></a>
## 원형이 드러나는 좁은 내부 검증 — 원 도어 명령과 Result 귀속

**PROPOSED_PRIVATE_FUNCTION**, DOMAIN 측 M01/F01과 M03/F08의 보조 검사다. 새로운 공개 API나 BCM 실행 본체가 아니다. [BCM_RESULT](../functions/can/BCM_RESULT.md)의 전체 형식·순서 검증 뒤, 보존한 같은 원 명령 사본을 대조하는 부분만 설명한다. App의 실제 완료·confirmation을 판정하지 않는다.

```c
NetCanResult NetBcmBoundary_CheckDoorResultOrigin(
    const NetBcmResult *result,
    const NetCanRxMeta *resultMeta,
    const NetBcmDoorCommand *originalCommand,
    uint32_t originalDomainBoot);

NetCanResult NetBcmBoundary_CheckDoorResultOrigin(
    const NetBcmResult *result,
    const NetCanRxMeta *resultMeta,
    const NetBcmDoorCommand *originalCommand,
    uint32_t originalDomainBoot)
{
    if (result == NULL || resultMeta == NULL || originalCommand == NULL)
        return NET_CAN_INVALID_ARGUMENT;
    if (originalDomainBoot == 0 || resultMeta->source.boot_id == 0 ||
        originalCommand->command.target_boot_id == 0)
        return NET_CAN_ORDER_HOLD;
    if (result->command_kind != NET_BCM_COMMAND_DOOR)
        return NET_CAN_INVALID_ARGUMENT;
    if (result->command_domain_boot != originalDomainBoot ||
        result->command_id != originalCommand->command.command_id ||
        resultMeta->source.boot_id != originalCommand->command.target_boot_id)
        return NET_CAN_ORDER_HOLD;
    if (result->door_event != NET_DOOR_EVENT_NONE &&
        result->door_target != originalCommand->door_lock_target)
        return NET_CAN_ORDER_HOLD;
    return NET_CAN_OK; // 원 명령 귀속 대조만 완료. 실행·확인 성공은 뜻하지 않는다.
}
```

IN 전부 호출 동안 borrowed, OUT/자원 변경 없음이다. `originalDomainBoot`는 접수 당시 원 송신 기동이며 현재 Boot로 보정하지 않는다. `originalCommand`는 Network가 이미 소유한 전체 원 명령 사본, `resultMeta`는 Result와 같은 수신 frame의 meta다. 현재 profile에서 Boot0 예약이라는 Wire 전제를 적용하며 C2 sourceGeneration0 정책과 혼합하지 않는다. 같은 target Boot라도 재사용 방지 lifecycle·명령 ID association이 미확인하면 **호출 전 바인딩 HOLD**다. 이 함수의 equality 통과로 그 증거를 생성할 수 없다.

`NET_CAN_INVALID_ARGUMENT`는 NULL 또는 도어용 함수에 다른 kind, `NET_CAN_ORDER_HOLD`는 해당 원 identity/target 불일치다. 선택 도어 event가 있을 때만 그 `door_target`을 원 목표와 대조하며 event 없음은 확인 근거 없음으로 보존한다. 호출자는 실패 시 기존 C2 결과·명령 이력을 갱신하지 않고 원 보고와 충돌 근거를 보존한다. OK 뒤에도 Result/Reason 지원, 단계 순서, 품질·Age·confirmation 투영과 실제 C2 소비 준비를 별도로 통과해야 한다.

## 현재 상태·보호·진단의 독립성

| 자료 | 보존할 구분 | 금지되는 성공 추정 |
|---|---|---|
| DoorState | lock/open/composite 각각 품질 + update_sequence/source_age_ms | 잠금 현재값이 같다는 이유로 해당 원 명령의 DONE 생성 |
| ClimateState | 지시 Fan/실측 Fan/열 방향/출력/방열 상태의 5개 품질 | 미측정 Fan을 지시값으로 채움, DONE을 목표 온도 도달로 해석 |
| LightState | 종류·밝기·RGB·apply_result·물리 확인 지원을 분리 | `light_physical_feedback_capability=0`에서 APPLIED를 실제 점등 확인으로 표시 |
| BcmEvent | 과열/불일치/복구의 occurrence와 발생 Age, 선택 관련 원 명령 | RECOVERY_CONFIRMED를 이전 공조 출력 자동 재개로 해석 |
| BcmStatus | 현재 fault_mask·대표 code·영향 기능과 평가 ID/Age | Bus-off/무수신을 ECU 전체 실패 또는 특정 실행 실패로 자동 변환 |

현재 상태의 동일 의미 키·송신 미시작 pending만 기존 정책에서 교체 가능하다. Result/Event는 원 명령 단계/발생별로 보존하고 용량 부족으로 다른 discrete를 덮지 않는다. 형식상 정상 INVALID/STALE 보고는 품질 그대로 보존한다. source_age와 event_age는 최초 기준점이 다르며 TX_SEQUENCE로 원 평가 번호를 대신하지 않는다.

## 오류·재시작·수명과 남는 HOLD

[공통 Service](../functions/NETWORK_SERVICE_LIFETIME.md#retirement)를 그대로 적용한다. Driver 참조가 먼저 해제되면 물리 Frame은 회수 가능할 수 있으나 terminal 결과·필요한 실패 소비가 미확정인 원 예약/identity는 유지한다. 재기동 후 옛 callback은 localToken/localEpoch로 구별하며 현재 BCM Boot/commandId만으로 새 슬롯에 연결하지 않는다. 새 BCM Boot를 확인하면 이전 대상 명령을 새 Boot로 다시 라벨하여 자동 실행하지 않는다.

BCM 자체 Send/Publish의 후발 실패는 BCM의 기존 기능·진단 Owner에게 관측시킬 실제 경계가 필요하다. DOMAIN의 `DomainIf_RxNetworkLocalFailure`를 BCM에 복제하거나 가짜 명령을 만들어 호출하지 않는다. DOMAIN 송신 항목은 [기존 세 실패 reason](../functions/NETWORK_FAILURE_HEALTH.md)과 원 접수 identity만 사용한다. CAN 복구·Permission 회복·한 번의 정상 수신으로 과거 실패를 지우거나 counter를 초기화하지 않는다.

유지 미결: IF-O09/IF-O24의 세대·원 품질 연결, IF-O12의 로컬 수명, D-03/D-05의 필수 실패/직렬화·문맥, 명령 한도 충돌, BCM_RESULT 확인 근거, BCM 진단의 실제 health sink. Owner 미결 원 ID를 유지하며 협의 변경은 S6 이후 통합 큐에 기록한다. 제품/Driver/보드 검증은 **NOT_EXECUTED**다.

## 문서 반례

| ID | 입력·사건 | 정적 기대 |
|---|---|---|
| B16-BCM-01 / 필수1 | DOMAIN CAN terminal TX 성공, BCM 구동 거부 | Network DONE 생성 없음. BCM 실제 Result/Reason → DOMAIN. 결과 미수신은 실행 실패 확정이 아님 |
| B16-BCM-02 | 원 DOMAIN Boot7/id8/대상 BCM9, Result header BCM10 | 귀속 ORDER_HOLD, 현재 명령에 결과 합성 없음 |
| B16-BCM-03 | 원 명령 DONE 뒤 같은 ID의 늦은 IN_PROGRESS | 종결 상태 후퇴 없음, Owner 순서 검증·충돌 보존 |
| B16-BCM-04 | 정상 INVALID 공조 품질, CAN 형식은 유효 | 원 품질 보고 유지, 형식 오류로 삭제하거나 정상 목표값으로 채우지 않음 |
| B16-BCM-05 / 필수9 | Result 송신 Frame release 먼저, terminal UNKNOWN | Frame 회수 조건과 원 Source 예약/identity 보존을 분리 |
| B16-BCM-06 / 필수10 | 재부팅 뒤 옛 localEpoch의 completion | 새 항목·새 buffer 불변, 이전 제출 근거로 별도 관측 |

읽기 근거: 현행 Interface BCM 00/10/20 전문, S4A api/bcm, B12 BCM/Permission Flow 및 CAN_TRANSFER_CONTROL, B15 Service 수명, S4C 시간·자원·AGE 계약. 기존 Wire48/Rx51·C2 72/26/11 및 외부 HOLD를 바꾸지 않는다.
