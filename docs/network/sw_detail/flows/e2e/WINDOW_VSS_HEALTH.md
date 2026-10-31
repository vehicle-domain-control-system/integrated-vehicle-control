# WINDOW·VSS — 명령, 보호, 실제 출력과 현재/최근 고장

[E2E 전체](../../09_E2E_FAILURE_RECOVERY.md) · [WINDOW 노드](../../nodes/WINDOW.md) · [VSS 노드](../../nodes/VSS.md) · [공통 실패·복구](COMMON_FAILURE_RECOVERY.md)

B17은 SC11~SC14의 원 기능 경로를 연결한다. SC11/SC12는 **DEFERRED / IMPLEMENTATION_HOLD**다. WINDOW 송수신·기능·표시·timeout 감시는 활성화하지 않는다. SC13/SC14는 A1 WARNING60·원 L과 A2 INPUT 단일 변환을 내부 선택했으며 실제 양단 지원·음향·같은 관측 연결 등 개별 경계는 검증 대기다.

## Layer·Module·Data/API 연결

N01 DOMAIN↔P01/R04↔N07 WINDOW와 N01↔P01/R03↔N04 VSS가 주 경로다. I01/I04/I07은 각 기능↔Network 경계다. 표시는 N01↔P02/R05↔N05 ESP32↔P04↔N08 MOBILE로 이어진다. 이 보조 표시 경로를 각 SC의 직접 Wire 목록에 덧붙이지 않는다.

L01/M01/F01~02는 원 Boot·관측·C2 반영의 연결, L02/M03/F07~10은 DOMAIN TxPort/Rx와 노드 callback, L03/M04/F11~13은 사본·AGE·순서, L04/M05/F14~15와 M06/F16~18은 CAN/UART 변환, L05/M07~09/F19~25는 Driver·수명·Health다. ST01~12의 해당 기존 책임을 사용한다. 별도 WINDOW/VSS E2E Manager·cache·공개 API를 추가하지 않는다.

| 기존 경계 | Data/API·Function 정본 | 인수·결과 의미 |
|---|---|---|
| DOMAIN→WINDOW | [WINDOW_COMMAND](../../functions/can/WINDOW_COMMAND.md), [노드 API](../../nodes/WINDOW.md#apis): `DomainIf_TxWindowCommand`, `WindowApp_OnDomainCommand` | 원 commandId/Boot/target/AGE 보존. callback void는 이동·정지 성공이 아님 |
| WINDOW→DOMAIN | [WINDOW_RESULT](../../functions/can/WINDOW_RESULT.md): `WindowNet_SendResult`, `DomainIf_RxWindowCommandResult`; [WINDOW_STATE](../../functions/can/WINDOW_STATE.md) | 실행 Owner가 생산한 실제 결과와 상태를 별도로 인계 |
| WINDOW 보호/진단 | [WINDOW_EVENT](../../functions/can/WINDOW_EVENT.md), [WINDOW_FAULT](../../functions/can/WINDOW_FAULT.md): `WindowNet_SendEvent`, `WindowNet_PublishFault`, `DomainNetPriv_OnWindowFault` | 끝단·끼임 발생과 현재 진단·이력을 구분 |
| DOMAIN→VSS | [VSS_EVENT](../../functions/can/VSS_EVENT.md): `DomainIf_TxVssOneShotEvent`, `VssApp_OnDomainEvent`; [VSS_WARNING_STATE](../../functions/can/VSS_WARNING_STATE.md): `VssApp_OnDomainWarningState` | occurrence 기반 일회 발생과 유효 현재 위험을 분리 |
| VSS→DOMAIN | [VSS_STATUS](../../functions/can/VSS_STATUS.md), [VSS Status 흐름](../../nodes/VSS.md#status-flow): `VssNet_PublishStatus`, `DomainNetPriv_OnVssStatus`, `DomainIf_RxVssState`, `DomainIf_RxVssFault` | 실제 서비스·현재/최근 고장. Event별 재생 ACK가 아님 |
| DOMAIN→표시 | [M_WARNING](../../functions/uart_m/M_WARNING.md), [M_AVAILABILITY](../../functions/uart_m/M_AVAILABILITY.md), [M_VSS_STATUS](../../functions/uart_m/M_VSS_STATUS.md), [M_WINDOW_STATE](../../functions/uart_m/M_WINDOW_STATE.md), [M_WINDOW_FAULT](../../functions/uart_m/M_WINDOW_FAULT.md) | DOMAIN 확정 의미/같은 CAN 관측만 투영. WINDOW 표시 HOLD |

정확한 전체 원형과 인자·enum·Byte 표는 위 정본을 따른다. `NetSubmitResult ... (const Net... *message)` 입력은 호출 중 borrowed, 로컬 OK 전에 전체 사본을 확보한다. Rx의 `message/meta`는 같은 검증 수신의 NULL 불가 borrowed IN이며 지연 소비자는 복사한다. 실패 OUT·Driver가 참조 중인 frame을 재사용하지 않는다. DOMAIN C2는 초기 Superloop의 ISR 밖 단일 담당 문맥에서 호출한다. void callback의 반환, Queue 접수, Driver terminal, release, 상대 typed 수용, 실제 기능 결과, App 소비는 각각 다른 관측이다.

<a id="sc11"></a>
## SC11 — 보류 WINDOW 이동·STOP·실제 결과·상태

직접 Wire/Rx **5/5**: DOMAIN_PERMISSION, WINDOW_COMMAND, WINDOW_STATE, WINDOW_RESULT, M_WINDOW_STATE. 다른 수신자의 Permission은 이 SC에 추가하지 않는다. 원 상세는 [WINDOW command/STOP](../../nodes/WINDOW.md#command-stop)이다.

<a id="sc11-normal"></a>
**조건부 정상 경로:** 별도 WINDOW 재개 승인과 Owner 바인딩 후, DOMAIN이 원 판단·대상 Boot·commandId를 확정 → 기존 TxPort가 명령 사본 접수 → M04가 원 AGE/문맥 검증 → CAN 직렬화·Driver → WINDOW Codec/admission → `WindowApp_OnDomainCommand` → WINDOW App가 실제 구동·로컬 보호 → 원 WINDOW_RESULT와 WINDOW_STATE 생산 → DOMAIN 수신/결과 귀속 → 같은 상태의 M_WINDOW_STATE 투영 → ESP32/BLE/App. 현재는 첫 실행 경계에서 HOLD하며 위 경로는 검증할 설계 추적이다.

<a id="sc11-failure"></a>
**실패:** 형식 정상이어도 현재 WINDOW 지원을 true로 만들지 않는다. WINDOW가 없는 현재 상태에서 timeout을 발생시키거나 차량 고장·움직임 완료·도어 결과를 합성하지 않는다. 향후에도 STOP은 Permission 소실만을 이유로 차단하지 않는 방향을 유지하되 원 Boot/target/command identity/AGE/schema 검증을 우회하지 않는다. STOP 송신 성공은 실제 모터 정지 증거가 아니다. `request_sequence↔commandId`, 수용한도 200ms↔1초, VENT70↔80, source_context 축약은 미합의다.

<a id="sc11-recovery"></a>
**복구:** WINDOW 부팅·단절 후 구 이동 명령을 새로운 기동으로 재발급하지 않는다. 원 Result가 오지 않았으면 결과 미확정이지 수행 실패 확정이 아니다. 회복한 실제 상태/결과의 원 식별이 확인될 때에만 기존 Owner가 판단한다. 로컬 보호·STOP은 CAN 왕복을 기다리는 Network 기능으로 옮기지 않는다. IF-O04/IF-O07/IF-O18 및 G05의 실행/표시/감시 HOLD를 보존한다.

<a id="sc12"></a>
## SC12 — 보류 WINDOW 끝단·끼임·Fault와 경고

직접 Wire/Rx **6/6**: WINDOW_STATE, WINDOW_EVENT, WINDOW_FAULT, VSS_WARNING_STATE, M_WARNING, M_WINDOW_FAULT. 이는 SC11의 명령 목록과 별도인 원 분모다.

<a id="sc12-normal"></a>
**조건부 정상 경로:** WINDOW 로컬 기능이 실제 끝단/끼임·고장·상태를 각각 생성 → SendEvent/PublishFault/PublishState의 기존 안정 사본 → CAN 검증/admission → DOMAIN이 같은 원 발생·품질·AGE를 수용 → 위험·현재 진단·가용성 판단 → VSS Warning 및 Mobile Warning/Fault로 전달한다. Network는 WINDOW_EVENT의 END를 ANTIPINCH로 바꾸지 않는다. 실제 보호 완료·정지 여부는 WINDOW 기능 Owner 자료로만 확인한다.

<a id="sc12-failure"></a>
**실패:** 보드 비활성, 보호 의미·결과 코드 표현 미합의, CF02 채택 미확인인 경계는 각각 HOLD다. 고장 code/현재 mask만으로 `movementStopped` 또는 `outputDisabled` 같은 원문에 없는 필드를 만들지 않는다. 반복 Frame은 새 occurrence가 아니고, callback 반환은 위험 CLEAR가 아니다. 원 Fault와 동일한 C2 반영/Getter 근거가 없으면 해당 M_WINDOW_FAULT 투영을 제한한다.

<a id="sc12-recovery"></a>
**복구:** 현재 Fault 해제와 최근 발생 이력은 따로 유지한다. 재기동 뒤 지난 끼임 경고를 새 발생으로 재생하지 않는다. 기능 재개·보호 정책·표시 재개는 별도 Owner 확인과 실행 승인 전 보류한다. WINDOW 미활성을 이유로 전체 차량을 fault로 만들거나 VSS의 독립 유효 입력까지 일괄 차단하지 않는다.

<a id="sc13"></a>
## SC13 — VSS 일회 Event·지속 경고·복구 경계

직접 Wire/Rx **4/4**: VSS_EVENT, VSS_WARNING_STATE, VSS_STATUS, M_WARNING. Event의 WELCOME/GOODBYE/LOCK_COMPLETE/UNLOCK_COMPLETE/LOCK_ERROR 의미를 보존한다. 발생 조건·음향 우선순위·실제 완료를 Network가 새로 정하지 않는다.

<a id="sc13-normal"></a>
**정상 조건부 흐름:** DOMAIN의 확정 occurrence 또는 유효 현재 Warning → 기존 public Tx/TxPort → Network 사본·선택 시 원 AGE → CAN Driver → VSS 검증/admission → `VssApp_OnDomainEvent` 또는 `VssApp_OnDomainWarningState` → VSS INPUT/App의 한도·품질·가용성 확인과 음향 중재 → 실제 서비스 Status 생산 → DOMAIN 수용. `VSS_STATUS` PLAYING/READY 또는 CAN TX 성공은 해당 Event의 실제 시작/완료 ACK가 아니다. M_WARNING의 App 표시도 별도 종단 소비다.

<a id="sc13-failure"></a>
**실패:** A1은 WARNING60 used60/frame64/DLC15를 선택했다. WARNING51은 이력이고 길이·DLC·B51로 자동 추정하지 않는다. 실제 producer/양단 지원 전 CF02 HOLD다. Event 최초 actual은 원 `L=min(양의 USE_LIMIT,500ms,2000ms)`를 사용하고0은 비사용·L 도달은 만료다. 적법 Started 이후 같은 Session 후속 cue에 최초 기한을 다시 적용하지 않는다. INVALID/STALE 입력으로 출력을 중단해도 위험 CLEAR를 만들지 않는다. VSS Permission 감독을 `VssRecoveryPermission`으로 변환하지 않는다.

<a id="sc13-recovery"></a>
**복구:** 입력 신뢰 상실 전 pending/중단된 일회 음향은 자동 재개하지 않는다. Event 무발생은 no-RX timeout이 아니며 실제 감독·명시 품질·계보/시간 상실과 구별한다. Warning은 새 유효 현재 근거 또는 A1의 동일 세대·원 Hold 기한·lane 자격이 남은 후보만 재중재한다. 출력 장치 Fault만인 경우 전체 NO_START/retirement·HEALTH 제한 해제·원 L·입력 신뢰를 모두 확인한 동일 occurrence 재평가만 가능하다. CAN 복구는 READY/실제 시작/새 재생권이 아니며 [A1 정본](../../../interface/02_NODES/VSS/10_DATA.md#a1-binding)의 실제 시각·오차 증거가 없으면 제한한다.

<a id="sc14"></a>
## SC14 — VSS 현재/최근 고장과 App 가용성

직접 Wire/Rx **4/4**: VSS_STATUS, M_WARNING, M_AVAILABILITY, M_VSS_STATUS. 현재 서비스·현재 fault·최근 fault·Network Health·DOMAIN 기능 가용성은 서로 다른 자료다.

<a id="sc14-normal"></a>
**정상 조건부 흐름:** VSS HEALTH의 완전 CREATED→FLOW→기존 INPUT 한 곳에서 NetVssStatus 명시 변환→PublishStatus의 전체 사본→CAN→DOMAIN private OnVssStatus→A2 State/Fault 독립 수용→DOMAIN 경고/가용성→기존 Mobile schema→ESP32/App이다. 내부 선택과 E-S01~05 실제 생산·sink·설치 profile/같은 입력 연결 대기를 구분한다. 원 current/이력·occurrence·품질·시각 근거가 없으면 READY/mask0로 채우지 않는다.

원 service `source_age_ms`와 활성 최근이력 `last_fault_age_ms`는 각자의 나이다. 현재 mask0이어도 최근 fault/occurrence는 남을 수 있다. `last_fault_quality` raw0/3을 서비스 전체 품질로 승격하지 않는다. Link Rx timeout은 Network Health이며 VSS 기능 고장을 직접 만들어 내지 않는다.

<a id="status-age-callsite"></a>
### 기존 Function 호출 예 — 두 AGE의 선택 시 누적

[B16 기존 함수](../../nodes/VSS.md#status-age-function)의 호출 위치를 연결하는 C형 의사코드다. 새 원형·제품 함수가 아니다. `source`는 이미 검증되어 Network가 소유한 불변 NetVssStatus 사본이다. 두 AGE에 **동일한 로컬 기준점까지 반영했다는 기록**과 그 이후 `elapsedMs`의 epoch/wrap 검증이 확보된 경우에만 아래를 실행한다. 기준점이 다르면 이 한 경과값을 적용하지 않고 TIME_HOLD를 유지한다.

```c
NetVssStatus selected;
NetCanResult ageRc = NetVssBoundary_AdvanceStatusAge(
    &source, elapsedMs, elapsedKnown, &selected);
if (ageRc == NET_CAN_OK) {
    // selected만 기존 M04→VSS_STATUS Encode→Driver 경로의 입력으로 사용한다.
    // 성공은 AGE 산술만 뜻한다. Queue 접수·서비스 정상·최종 TX를 기록하지 않는다.
} else {
    // selected를 읽지 않는다. 원 사본/평가 identity를 보존하며 시간 오류를 제한한다.
}
```

`elapsedKnown`은 확인된 로컬 시간 산술 증거를 전달하는 기존 인자다. 설정값 하나로 원 Boot·품질·소비 준비·Owner 승인을 대신하지 않는다. 예를 들어 원 두 AGE10/1000에 미반영20을 더하면30/1020이며 occurrence/update는 그대로다. 이력 부재3이면 이력 값을 새 발생으로 누적하지 않는다. 포화65535는 하한이며 fresh가 아니다.

<a id="sc14-failure"></a>
**실패:** State Core 반영 뒤 Fault 또는 optional Observer만 실패한 경우 이미 반영한 Core를 재호출하지 않는다. 실제 반영 단계가 관측되지 않으면 재시도 판단을 HOLD하고 전체 성공도 선언하지 않는다. 같은 값의 최신 raw와 Snapshot을 붙여 M_VSS_STATUS를 만들지 않는다. 원 보고 유실을 이력 삭제·정상0으로 보정하지 않는다.

<a id="sc14-recovery"></a>
**복구:** 새 Status에서 확인된 현재 고장 해제만 반영하며 최근 고장 이력은 원 occurrence/AGE로 유지한다. 정상 Frame 한 건으로 local Health counter를 지우지 않는다. VSS 서비스 회복·DOMAIN 가용성 재평가·App 실제 표시를 각각 관측한다. IF-O06/IF-O15/IF-O17, IF-O09/IF-O24, D-05/G06과 CF02는 회신 대기다.

## 자체검수 반례

| 입력·사전 상태 | 경계·부작용 제한 | 관측·복구 |
|---|---|---|
| WINDOW 미설치, 주기 timeout 경과 | 실행·표시·감시 HOLD; fault 생성 없음 | SC11/12 DEFERRED 유지 |
| 향후 이동 중 Permission 소실 후 STOP | Boot/target/AGE 검증 유지; 로컬 보호 Owner 유지 | 실제 정지 보고 이전 완료 없음 |
| END event, 원 끼임 근거 없음 | ANTIPINCH 경고 생성 없음 | 원 끝단 발생만 조건부 수용 |
| Warning64B/DLC15, WARNING60 설계 선택·실제 양단 증거 없음 | WARNING51 이력/자동 판별 금지 | 실제 producer/ABI·시간은 CF02 CONTRACT_HOLD |
| Event queued 뒤 단절·회복 | pending 일회 음향 자동 재생 없음 | 새 유효 Warning만 별도 중재 |
| current fault0, 과거 occurrence5/Age1000 | 이력 삭제·Age0 갱신 없음 | 새 service 평가와 과거 이력 분리 |
| RxVssState 반영 후 RxVssFault 실패 | A2 State 수용 유지·State 중복 호출0, Fault 불명 결과 자동 재시도0 | 실제 profile/부분 결과·old K fence 증거는 E-S05 대기 |

모든 반례는 문서 정적 추적이며 Driver/보드/실제 음향·보호 시험은 NOT_EXECUTED다. 기존 S6 이후 협의 큐의 ID·이력과 G6 당시 Interface39 봉인본을 보존한다. 현재 별도 후보는 Interface40개 후보의 출처·해시 기준을 따라 참조하며, 이 문서 동기화로 Owner35·외부 HOLD를 변경하지 않는다.
