# BCM·DOMAIN_PERMISSION — 10 Wire·13 수신 경계

이 흐름은 DOMAIN→BCM 명령 3종, BCM→DOMAIN 상태·결과·사건·상태보고 6종, DOMAIN_PERMISSION의 BCM/CIS/WINDOW/VSS 수신 4곳을 연결한다. ID·Wire·period·timeout·profile은 B07의 원 후보이며 팀 채택을 확정하지 않는다. 원 공개 API 선언·자료형은 S4A, 기동·AGE·수명·후발 실패는 최신 S4C를 따른다.

[공통 흐름](CAN_NETWORK_PATHS.md) · [BCM 자료](../data/CAN_BCM_DATA.md) · [공통 자료](../data/CAN_COMMON_DATA.md) · [전송 제어](../functions/CAN_TRANSFER_CONTROL.md)

## 시작 조건과 소유 경계

1. DOMAIN이 원 CommandContext와 판단을 생성한다. Network는 공개 DomainIf_Tx 본체를 소유하지 않고 기존 TxPort callback 본체·등록 요청을 제공한다. 등록 ABI·ISR 밖 단일 소유자 직렬 문맥·반환값 의미·실제 실패 sink가 연결되어야 실제 송신 경로를 개방한다.
2. Network는 호출 중 원 자료를 빌려 검증하고 자기 소유 item에 복사한다. NetSubmitResult OK 또는 DomainIf OK는 이 로컬 접수 경계다. command/result/event item은 원 identity를 보존하며 queue 공간 부족 시 다른 discrete 항목을 조용히 교체하지 않는다. 상태 latest 교체는 공통 규칙에서 아직 시작하지 않은 같은 의미 키와 유효 순서인 경우만 허용한다.
3. 공통 dispatcher가 원 기동·문맥·AGE를 다시 검증하고 Codec이 만든 frame snapshot을 driver에 인계한다. 최종 전송 결과와 driver 참조 해제를 독립 관리한다. 참조 해제 근거 없이 timeout만으로 buffer를 재사용하지 않는다.
4. 수신 Network는 Frame 형식·필드, 기동·순서·시간, typed 의미·실행 문맥을 모두 확인한다. malformed·미확정 admission·원본 시간 부재는 진단/격리 경로이며 정상 heartbeat·관측 cache·허용 상태를 새로 만들지 않는다.
5. C2 수신은 DOMAIN 초기 Network Superloop의 ISR 밖 단일 소유자 문맥에서 직렬 실행하고 각 BCM/CIS/WINDOW/VSS callback은 해당 MCU의 합의된 로컬 문맥에서 호출한다. 다른 ECU의 함수 포인터를 버스 너머로 직접 호출하지 않는다. C2 접수 OK와 Domain snapshot 적용은 다른 경계다.

## 10 Wire와 13 Rx의 별도 종단

| Wire / 상세 함수 | 송신 입력 | 수신 경계 | 정상 처리의 필요 증거 |
|---|---|---|---|
| [DOMAIN_PERMISSION](../functions/can/DOMAIN_PERMISSION.md) | DomainNet_PublishPermission | BCM private monitor | 검증된 Net DTO+meta, 원 판단 AGE·revision, monitor 바인딩 |
| 같은 Wire | 같은 송신 | CIS App 및 Pi relay 별도 가지 | power 원 판단·사용 한도; App과 relay 각각의 인계 결과 |
| 같은 Wire | 같은 송신 | WINDOW | 실행/timeout/domain alive 바인딩 전 CONTRACT_HOLD, normal callback 금지 |
| 같은 Wire | 같은 송신 | VSS private monitor | VSS의 private monitor 바인딩; 새 Service/RecoveryPermission 생성 금지 |
| [BCM_DOOR_COMMAND](../functions/can/BCM_DOOR_COMMAND.md) | txBcmDoorCommand | BcmApp_OnDomainDoorCommand | 원 DOMAIN boot+command_id, BCM target boot, origin 투영·수락시간 |
| [BCM_CLIMATE_COMMAND](../functions/can/BCM_CLIMATE_COMMAND.md) | txBcmClimateCommand | BcmApp_OnDomainClimateCommand | 세 제어값·공통 명령시간·BCM 기능 admission |
| [BCM_LIGHT_COMMAND](../functions/can/BCM_LIGHT_COMMAND.md) | txBcmInteriorLightCommand | BcmApp_OnDomainLightCommand | type/level/RGB·BCM_LIGHT 채택 profile·공통 명령시간 |
| [BCM_DOOR_STATE](../functions/can/BCM_DOOR_STATE.md) | BcmNet_PublishDoorState | DomainIf_RxBcmDoorState | lock/open/composite별 품질, 평가번호·AGE·기동 연결 |
| [BCM_CLIMATE_STATE](../functions/can/BCM_CLIMATE_STATE.md) | BcmNet_PublishClimateState | DomainIf_RxBcmClimateState | commanded/measured 구분·다섯 품질·summary 원 평가 |
| [BCM_LIGHT_STATE](../functions/can/BCM_LIGHT_STATE.md) | BcmNet_PublishLightState | DomainIf_RxBcmInteriorLightState | 네 품질·RGB·apply와 물리 feedback capability 구분 |
| [BCM_RESULT](../functions/can/BCM_RESULT.md) | BcmNet_SendResult | DomainIf_RxBcmCommandResult | 원 명령 3식별자·단계·optional evidence 원자성·confirmation 바인딩 |
| [BCM_EVENT](../functions/can/BCM_EVENT.md) | BcmNet_SendEvent | DomainIf_RxBcmProtectionEvent | 원 occurrence/AGE/품질·기동·optional 명령 연결 |
| [BCM_STATUS](../functions/can/BCM_STATUS.md) | BcmNet_PublishStatus | DomainIf_RxBcmStatus | current fault mask/code/category·평가 AGE·FaultInfo 의미 |

각 Rx-ID는 해당 메시지 문서에 기록했다. Permission의 한 수신 성공으로 다른 세 수신 성공을 집계하지 않는다. WINDOW가 HOLD여도 다른 노드의 검증 결과는 각각 보존하되 전체 채택 완료로 보고하지 않는다.

<a id="permission-projection"></a>
## Permission 합성의 입력별 보존과 조건부 의미 투영

r1 [Permission 투영 조건](../../interface/07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)은 **UNKNOWN의 원인별 C2 Quality 투영 권장안 / DOMAIN Adapter 수용 대기**다. FAILED/NO_DATA 시 기존 ALLOW 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고이며 실제 Network 합성·CIS/Pi 전달은 BINDING_NOT_VERIFIED다. DOMAIN Permission Adapter가 원 의미·원인에 맞는 C2를 만들고 Network는 실제로 채택된 투영의 품질 일치·원 시간·문맥을 검증한다. 이 절은 정책 채택이나 실행 바인딩을 확정하지 않는다. 원 의미·원 품질·원인의 보존 위치도 DOMAIN 확인 전 미정이며 Network가 bool만으로 추측하거나 사유 필드·새 DTO를 추가하지 않는다.

| 기존 C2 입력 | 보존할 자료 | Wire 대응 후보 / 제약 |
|---|---|---|
| DomainIf_CisPowerPermission_t | vehiclePowerAllowed, quality, meta.quality/ageMs/updateSequence | 확인된 ALLOW true/OK는1/OK, 정상 평가 DENY false/OK는0/OK. 확인된 UNKNOWN 원인과 채택된 정책이 있을 때만 아래255 투영 |
| DomainIf_WindowOperationPermission_t | operationAllowed, quality, meta의 세 멤버 | operation_allowed는0/1뿐. power의255 규칙을 이 필드에 복제하지 않고 operation_quality와 실제 합성 근거 보존; WINDOW 보류 |
| DomainIf_WindowDomainAlive_t | domainLinkAvailable, meta의 세 멤버 | POWER_OPERATION_STATE 직접 대응 아님; alive·operation·power 합성 바인딩 필요 |

| 확인된 DOMAIN 원 의미·원인 | C2 권장 투영 | CAN Power 투영 | 검증·미확인 분기 |
|---|---|---|---|
| 실제 정상 평가의 확정 ALLOW | true + OK |1 + OK | 같은 원 시간·세대·quality/meta 일치와 합성 근거 필요 |
| 실제 정상 평가의 확정 DENY | false + OK |0 + OK | 평가 실패나 UNKNOWN을 DENY로 바꾸지 않음 |
| 미관측·판단 근거 없음 | false + NO_DATA |255 + NO_DATA | 그 원인을 DOMAIN이 확인하고 매핑 채택 범위를 입증할 때만 |
| 이전 판단 수명 만료 | false + STALE |255 + STALE | 원 성공 시각 유지; 과거 ALLOW 사용 불가 |
| 신뢰할 수 없는 평가 실패 | false + INVALID |255 + INVALID | Core의 FAILED/NO_DATA ALLOW 철회는 담당자 구현 완료 보고; 이 품질 매핑은 DOMAIN Adapter 수용 대기인 조건부 권장안이며 실제 Network→CIS→Pi 전달은 미검증 |
| UNKNOWN이면서 원 OK 품질의 무손실 보존 필요 | 기존 bool+quality+meta로 세 상태 표현 불가 | 해당 투영 HOLD | false/OK 또는 임의 비정상 품질로 조용히 치환하지 않음; 최소 계약 변경은 별도 검토 |

정상적으로 표현된 비정상 품질 보고를 인자 오류로 숨기지 않는다. 원 의미와 품질을 축약하는 투영을 채택했다면 축약 전 기록은 DOMAIN에 보존되어야 하며 실제 연결 근거가 필요하다. 품질 충돌은 해당 입력을 확인 불가로 제한하고 원 충돌 자료를 보존한다. 임의 OK 선택·enum 숫자 max·전체 입력의 OK 보정은 없다. WINDOW 부재만으로 독립 Power를 불허로 바꾸지 않으며, 필요한 입력·공통 AGE를 입증하지 못한 ALLOW도 만들지 않는다.

```c
// 기존 Permission 입력 callback/M03 owner 직렬 문맥의 조건부 한국어 의사코드.
// 정책 확인은 실제 DOMAIN 원 의미/원인 기록과 채택 근거다. bool 상수나 새 공개 인자가 아니다.
if (/* 입력 NULL 또는 선언 범위 밖 enum */) return DOMAIN_IF_INVALID_ARGUMENT;
if (/* quality와 meta.quality 충돌 */) {
    // 실제 원 충돌을 진단에 보존하고 정상 허용/새 payload를 게시하지 않는다.
    return DOMAIN_IF_NOT_AVAILABLE;
}
if (/* 원 UNKNOWN+OK를 무손실 보존해야 하거나 DOMAIN 의미 투영 근거가 없음 */)
    return DOMAIN_IF_NOT_AVAILABLE;
if (/* 정상 평가 ALLOW/OK 또는 정상 평가 DENY/OK가 확인됨 */) {
    // 기존 vehiclePowerAllowed true/false를 CAN power1/0으로 대응. 같은 OK 품질 유지.
} else if (/* false + NO_DATA/STALE/INVALID이며 실제 원인과 해당 매핑의 DOMAIN 채택이 확인됨 */) {
    // CAN power255와 동일한 비정상 품질을 함께 구성. 원 성공번호/원 시각은 보존.
    // UNKNOWN의 새 성공·AGE0 또는 DENY/OK를 만들지 않는다.
} else {
    // 일반 false 또는 해석되지 않은 true/비정상 품질을 UNKNOWN으로 추측하지 않는다.
    // 기존 정상 열화 보고의 원 자료는 보존하되 미채택 의미 투영은 HOLD다.
    return DOMAIN_IF_NOT_AVAILABLE;
}
if (/* 합성에 필요한 원 입력·POWER_OPERATION_STATE·시간/세대/순서 규칙 미바인딩 */)
    return DOMAIN_IF_NOT_AVAILABLE;
// 필요한 각 원 입력을 같은 로컬 now로 비교한다. 오래된 입력으로 ALLOW를 만들지 않는다.
// 확인된 열화 보고는 원 기한 초과만으로 형식 오류 처리하지 않으며 해당 투영 계약을 확인한다.
if (/* 입력별 source 사본·완성 DTO·원 연결을 원자 게시할 공간이 없음 */)
    return DOMAIN_IF_QUEUE_FULL;
// 원 성공 Sequence/시각을 유지한 채 별도 합성 UPDATE_SEQUENCE를 아래 조건대로 결정한다.
// 공유 source_age_ms는 채택된 포함 입력의 보수 누적 경과. 미상 시간을0으로 채우지 않는다.
// DomainNet_PublishPermission의 owned copy와 source 연결을 함께 게시한 때만 OK.
// 게시 실패는 원 실제 반환을 보존한다. 별도 현재 제한 관측은 아래 송신 재검사에 사용한다.
```

<a id="permission-sequences"></a>
## 원 성공·합성·전송·relay 번호와 제한 전달

[r1 번호 공간](../../interface/04_COMMON/10_DATA_AND_ID_RULES.md#delta-permission-sequences)에 따라 숫자가 같아도 다음 번호를 동일 identity로 취급하지 않는다. 합성 갱신 세부조건·필수 입력·공통 AGE/Quality/세대 정책은 계속 미결이다.

| 번호 | 생성자와 변경 사건 | 반복·실패에서의 보존 |
|---|---|---|
| DOMAIN 원 성공 평가 Sequence | DOMAIN 기능의 실제 성공 평가 | 성공17 뒤200ms 평가 실패는17·마지막 성공 시각 유지. 실패를 성공18/AGE0으로 만들지 않음 |
| CAN 합성 UPDATE_SEQUENCE | Network 합성 경계의 검증 원 tuple 또는 실제 전달 의미·현재 품질 제한 변경 후보 | 원 성공17이 같아도 확인된 제한 변화는 새 합성 후보. 단순50ms 송신은 변경 없음 |
| CAN TX_SEQUENCE | Network의 메시지별 실제 프레임 전송 | 원 성공/합성 갱신 근거가 아님 |
| UART LINK_SEQUENCE | UART 방향별 프레임 전송 | 원 성공/relay 평가와 별개 |
| P_PERMISSION relay UPDATE_SEQUENCE | CIS private relay 소유. CIS 기동마다 1부터, 새 원 평가·수용 세대·실제 제한 변경마다 +1 | 반복 없음. 원 성공번호나 CAN 합성번호의 무손실 전달 필드가 아님 |

**FAILED/NO_DATA 시 기존 ALLOW 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고(OWNER_REPORTED_IMPLEMENTED)**다. UNKNOWN의 원인별 C2 Quality 투영은 권장안·DOMAIN Adapter 수용 대기이며 Network 합성·CIS relay·Pi 전달은 BINDING_NOT_VERIFIED다. 실제 DOMAIN 실패·제한 원인이 확인되고 정책이 채택된 경우 원 성공17·마지막 성공 시각을 유지한 제한을 다음 성공이나500ms 만료까지 미루지 않고 기존 합성/relay에 요청한다. 같은 원 성공 Sequence만으로 제한을 중복 폐기하지 않는다. 최초 실패라 성공 기록이 없으면 정상 source Sequence·시각·AGE0을 발명하지 않으며 기존 NO_DATA/포화 규칙과 대조할 시간·Wire 표현은 HOLD다. 합성번호의 갱신 자체가 원 시간을 새로 시작하지 않는다.

CAN 수신의 같은 합성 UPDATE에 다른 품질을 넣는 것은 이 원 성공번호 규칙과 다르다. 확인된 제한의 새 합성 UPDATE와 실제 전이 근거를 사용하고, 같은 합성번호의 의미 충돌을 일반 semanticExactlyEqual에서 허용하지 않는다. Pi에서도 새 relay 제한을 같은 원 성공이라는 이유로 단순 반복 처리하지 않으며 실제 전이·원 AGE 근거를 확인한다.

### 대기·재시도·Driver 수명

실제 DOMAIN owner의 현재 제한 기록을 같은 원 성공/세대에 결합해 검증한 경우, 기존 M04 송신 선택은 그 제한을 직렬로 확인하여 구 ALLOW의 미시작 pending·재시도 재게시를 막는다. 이 확인은 새 제한 payload의 성공 접수와 별개다. 새 제한의 슬롯 부족/송신 실패가 과거 ALLOW 재개 근거가 되지 않는다. 반대로 malformed 입력·근거 없는 incoming·일반 제출 실패만으로 제한을 만들지 않는다. 실제 제한 관측과 원 item의 연결·동기화가 입증되지 않으면 해당 재전달을 HOLD한다.

```c
// 기존 M04 선택/재시도 경계. 미시작 item과 현재 검증된 owner 제한을 같은 직렬 구간에서 읽는다.
if (/* 구 ALLOW가 현재 원 기한을 넘었거나 같은 원 성공의 실제 제한과 충돌 */) {
    // 구 item의 원 payload/시각은 증거로 보존하되 pending/retry로 재게시하지 않는다.
    // 새 제한 DTO는 별도 계약/공간/원자 접수를 거쳐야 하며 실패도 실제 실패로 보존한다.
    // 송신/상대 무효화 완료나 NetworkLocalFailure 개별 사건을 만들지 않는다.
} else if (/* 현재 제한 유무·원 association·시간 근거가 미확인 */) {
    // 해당 ALLOW 재전달 HOLD. 합성번호 증가만으로 사용 회복을 만들지 않는다.
} else {
    // 채택된 기존 선택/Encode/Driver 절차로 진행한다.
}
// 이미 Driver가 참조 중인 frame은 수정·회수·취소됐다고 가정하지 않는다.
// terminal/release/원 identity를 각각 끝까지 보존하고 새 제출 전에 다시 제한을 확인한다.
```

이미 전송 중인 구 frame의 원격 도착을 취소하거나 CIS/Pi가 동시에 제한을 받았다고 보장하지 않는다. 실제 전송 불능·지연은 그대로 기록한다. 후방 Power 원500ms/약200ms 실제 재평가와 CAN50/150, UART-C 수신 중단1500(P_PERMISSION 반복 없음), 후방 거리150 후보 및 USE_LIMIT1500 후보/500조건부는 각각 유지한다.

## 명령·결과·상태의 독립 진행

도어 잠금 해제 흐름은 원 요청→DOMAIN 결정→Network 로컬 접수→CAN 인계→BCM 접수/실행→BCM_RESULT 및 BCM_DOOR_STATE→DOMAIN 기능 판단으로 진행한다. Result와 State 도착 순서는 정해진 성공 시퀀스가 아니다. result DONE와 event 이름은 원 기능 결과의 보고이며 CONFIRMED를 새로 생성하는 근거가 아니다. terminal 결과 이후 도착한 ACCEPTED/IN_PROGRESS로 같은 원 명령의 상태를 되돌리지 않는다. DoorEvidence를 BCM_EVENT로 중복 송신하지 않는다.

공조는 commanded와 measured를 별개로 운송하고 과열/FAN_MISMATCH/RECOVERY_CONFIRMED 사건을 해당 occurrence로 보존한다. 늦게 도착한 과거 사건을 최신 정상 current 상태에 덮어쓰지 않는다. 복구 상태가 이전 팬/열출력 목표의 자동 재개를 뜻하지 않는다. 조명은 control 적용 결과와 실제 물리 피드백의 지원 여부를 나누며 capability0에서 APPLIED를 실제 광출력 확인으로 바꾸지 않는다.

## 시간·실패·복구의 종료 조건

| 메시지 성격 | 원 후보 period / Rx timeout | 검수 기준 |
|---|---|---|
| Permission |50ms /150ms | Pi500ms /1500ms 후보로 DOMAIN/CAN 제한을 늘리지 않음 |
| DoorState |100ms /300ms | 재송신이 같은 원 평가 AGE를0으로 재시작하지 않음 |
| ClimateState, LightState |200ms /600ms | 값별 품질과 current evaluation time 보존 |
| Status |250ms /750ms | 원격 comm fault와 로컬 Rx-loss를 별개 유지 |
| Command, Result, Event | 사건 기반, 무발생 silence timeout 없음 | command expiry와 통신 주기 timeout을 혼동하지 않음 |

ACCEPT_LIMIT의200ms/1s 충돌과 `>`/`>=` 동등 경계는 미결이며 임의로 택하지 않는다. command50ms1회 및 result/event20/40ms2회의 재시도 수치는 원 후보로만 남기고 Driver/소비자 합의 없이 켜지 않는다. driver 지연을 모르면서0으로 계산하지 않는다. S4C 공통 AGE가 원 시각·로컬 경과·포화 하한을 처리하며 송신 clock와 수신 clock를 직접 빼지 않는다.

접수 후 실패는 원 항목·generation·identity에 연결한다. DOMAIN 명령의 before-send 실패만 기존 세 실패 사유와 필수 sink 경로를 사용하며 이미 DISPATCHED인 항목에 AbortBeforeDispatch를 붙이지 않는다. BCM sender 실패는 BCM 소유자의 바인딩된 경로로 전달한다. 실제 sink 미확보는 기술 HOLD이며 optional observer로 대체하지 않는다. 늦은/중복 driver callback은 현재 slot 세대의 소유권을 훼손하지 않으며 최종결과와 참조해제 증거를 각각 한 번 처리한다.

정상·거부·후발 실패 사례는 각 메시지 문서의 표와 연결한다. 근거는 `S4C E2E_SCENARIO_18.md`의 SC01~04, SC08~09, SC11, SC16~18 및 `S4C FAILURE_RECOVERY_CONTRACT.md`이다.
