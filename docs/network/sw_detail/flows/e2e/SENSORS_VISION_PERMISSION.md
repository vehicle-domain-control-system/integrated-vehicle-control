# 센서·Vision·Permission — SC05~SC08·SC10의 기능·실패·복구

[B17 전체](../../09_E2E_FAILURE_RECOVERY.md) · [공통 실패·복구](COMMON_FAILURE_RECOVERY.md) · [CIS·Pi 경계](../../nodes/CIS_PI.md) · [DOMAIN](../../nodes/DOMAIN.md) · [ESP32·MOBILE](../../nodes/ESP32_MOBILE.md) · [VSS](../../nodes/VSS.md)

센서·Vision·Permission 경계의 정적 설계다. 각 화살표는 다음 Owner에게 넘길 계약이며 실제 실행·채택 증거가 아니다. 아래 조건부 정상 경로의 `HOLD_CONTRACT / BINDING_OPEN`을 통과한 것으로 표시하지 않는다. G6 당시 Interface39 봉인본과 현재 검토 중인 Interface40개 후보의 출처·해시 기준 참조를 구분한다. Owner35, Wire48/Rx51, C2 v0.6·Vehicle v0.3 후보와 외부 HOLD를 보존한다.

## 공통 인계와 결과의 독립성

L01/M01은 원 세대·관측을 연결하고 L02/M03은 typed 인계, L03/M04는 안정 사본·시간·순서, L04/M05·M06은 CAN/UART Codec, L05/M07·M08·M09는 저장소·Driver·실제 관측을 맡는다. [Layer5/Module9/F01~F25 정본](../../03_LAYER_MODULE_INDEX.md)을 재사용한다. 새 Queue·타이머·공통 FSM·공개 API를 만들지 않는다. Pi·CIS·DOMAIN·VSS·MOBILE 기능 본체는 각 Owner 범위다.

| 단계와 관측자 | 해당 성공이 뜻하는 것 | 다음 단계에 아직 필요한 증거 |
|---|---|---|
| Producer → `*Net_Publish*` / 해당 Network | OK 전 전 필드·원 identity·AGE 기준의 own 사본과 원자적 게시 | 실제 Driver 수용·전송·상대 Rx. 실패는 INVALID_ARGUMENT / NOT_READY / NO_CAPACITY이며 부분 새 접수 없음 |
| Driver / 해당 노드 M08 | accepted, terminal 성공·실패·미확인, reference release를 각각 관측 | terminal은 상대 typed 인수가 아님. release가 늦으면 frame 불변·재사용 금지; release가 먼저여도 결과 귀속·원 예약 유지 |
| Frame/Codec / 수신 M05·M06 | 정확 profile·방향·ID/Type·LEN/DLC·CRC/오류·필드 검사 | Boot/원 순서·소비시점 AGE·동일 DTO/meta·실제 Owner 문맥. malformed는 정상 liveness/소비 갱신 없음 |
| `CisApp_*`, `PiApp_*`, `VssApp_*` / 수신 App | `void` 통지의 호출 종료 | 실제 안전한 복사·기능 소비·출력 완료는 반환에 없음. 반환 뒤 borrowed pointer 보관 금지 |
| `DomainIf_Rx*` / DOMAIN Core·선택 Observer | 로컬 C2 반환 및 확인 가능한 반영 단계 | OK를 동일 원 CAN→Getter 연결 증명으로 대체하지 않음. 일부 반영/불명확한 실패 뒤 자동 재호출 금지 |
| `DomainIf_Tx*` / DOMAIN→Network TxPort | 기존 typed callback의 로컬 접수 | VSS 음향·Gateway BLE·MOBILE 표시 완료. 공개 본체는 DOMAIN이 구현 |
| Gateway→MOBILE / ESP32·App | 실제 채택된 BLE·현재 recipient 계약에 따른 각각의 수용·표시 | UART TX 또는 `Gateway_Interface_SendToMobile` 반환만으로 App 화면 확인 불가. 외부 ACK/API를 추가하지 않음 |

실제 App callback은 ISR이 아닌 허용된 Task/Superloop 문맥에서 호출한다. RTOS 방식·clock·wrap·Queue 깊이·재시도·WCET는 미결이다. 원 AGE는 각 홉의 **아직 포함하지 않은** 경과만 한 번 더하고 포화65535는 하한으로 보존한다. 다른 노드 timestamp를 빼거나 미측정 Driver 대기를0으로 채우지 않는다. 동일 원 관측 재수신은 같은 소비시각으로 환산한 나이를 비교하며 최신 수신시각만으로 과거 자료를 젊게 만들지 않는다.

아래 `NetCan_*`, `NetUartC_*`, `NetNodeCis_*` 함수는 선행 문서의 private 설계 후보다. C형 블록은 기존 함수의 **호출 지점 설명**이며 새로운 함수·제품 코드를 설치하지 않는다. `sourceBindingVerified` 등은 실제 원 사건·clock·Owner 채택 증거의 판독 결과다. 설정값을 true로 바꿔 실행을 승인하는 스위치가 아니다(G56-N02).

<a id="sc05"></a>
## SC05 — 환경 센서 원값·부분 품질 수집과 표시

**원 직접 집합: 3 Wire / 3 Rx.**

- Wire: `WIRE-P01-CIS_ENVIRONMENT`, `WIRE-P01-CIS_STATUS`, `WIRE-P02-M_CIS_ENVIRONMENT`
- Rx: `RX-P01-CIS_ENVIRONMENT-DOMAIN`, `RX-P01-CIS_STATUS-DOMAIN`, `RX-P02-M_CIS_ENVIRONMENT-ESP32`
- Node / Link: `N03;N01;N05;N08` / `P01;P02;P04;R02;R05;I03;I01;I05`.
- 원 제목·집합은 S4C `evidence/s4c_trace.json`에서 그대로 인수했다.

**시작과 원 생산자.** CIS 온도·습도·조도 담당의 실제 새 측정 또는 그 값의 품질 변경이다. [NetCisEnvironment](../../../interface/02_NODES/CIS/10_DATA.md#type-netcisenvironment)의 15필드는 세 값 각각 raw·sequence·AGE·validity·reason으로 구성된다. -123은 -1.23°C, 조도70000은 70000lx다. 습도65535·INVALID는 정상 형식의 열화 보고이며 습도0이나 전체0으로 바꾸지 않는다. CIS_STATUS는 별도 평가로 원 측정의 갱신 근거가 아니다.

| 기능 연결 순서 | 정확한 API·단일 Function 정본 | 정상·거부·완료 조건 |
|---|---|---|
| CIS 환경 생산 → 로컬 접수 → P01/R02 | `NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message);` / [접수·Codec·Deliver](../../functions/can/CIS_ENVIRONMENT.md) | 세 채널 원 근거를 함께 복사. 대기 중 새 조도만 갱신돼도 온습도 sequence/AGE를 다시 시작하지 않음 |
| DOMAIN CAN Rx → C2 환경 Owner | `DomainIf_Status_t DomainIf_RxCisCabinEnvironment(const CisCabinEnvironmentState_t *state);` / 같은 Function의 [수신](../../functions/can/CIS_ENVIRONMENT.md#receive) | raw/C2 품질·세대·사용 TTL 변환 근거가 없으면 해당 투영 HOLD. 원 C2 Environment 선언에 없는 sourceGeneration을 추가하지 않음 |
| 보조 진단 입력 CIS_STATUS → DOMAIN | `CisNet_PublishStatus` → `DomainIf_RxCisStatus` / [Status](../../functions/can/CIS_STATUS.md) | 통신 생존·부분 기능 상태를 별도로 소비. 환경 세 값의 품질을 덮지 않음 |
| DOMAIN 환경 snapshot → TxPort → P02/R05 | `DomainIf_Status_t DomainIf_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state);` / [M_CIS_ENVIRONMENT](../../functions/uart_m/M_CIS_ENVIRONMENT.md), [동일 원 관측](../../functions/DOMAIN_QUERY_PROVENANCE.md) | 실제 같은 CAN 관측과 snapshot의 결합이 필요. 동일한 숫자 값만으로 최신 CAN meta를 붙이지 않음 |
| ESP32 valid Rx → BLE → MOBILE 표시 | [Gateway 경계](../../nodes/ESP32_MOBILE.md), [UART-M CAN 투영](../UART_M_CAN_PROJECTION.md) | 현재 recipient Session과 세 값의 원 품질·AGE를 각각 보존. App 단위 표시는 App Owner. 표시 확인·재조회는 별도이며 이 상태에 명령 Result를 요구하지 않음 |

직접 Wire3/Rx3 중 CIS_STATUS는 원 시나리오에 포함된 진단 보조 입력이다. Query/M_CONTEXT는 필요할 때 쓰는 **간접/보조** 경로이며 위 직접 집합을 늘리지 않는다. 공조의 환경 소비는 SC02에서 별도로 연결한다.

### SC05의 수신 호출 지점

`acceptedEnvironment`와 `acceptedMeta`는 동일 CAN frame을 검증·수용하여 안정 보관한 IN 사본이다. 소비시점 미반영 AGE·C2 품질 변환 근거는 기존 Deliver 계약에서 확인한다. 아래는 DOMAIN의 허용된 소유자 문맥이며 실제 M09 저장·직렬화 연결이 확보된 경우의 설명이다.

```c
NetCanResult delivered = NetCan_DeliverCisEnvironment(
    &acceptedEnvironment, &acceptedMeta);
switch (delivered) {
case NET_CAN_OK:
    // 확인된 C2 호출 결과만 기록. 각 센서 TTL과 Core/Observer 반영은 별도다.
    // 원 snapshot 결합이 미확인인 상태에서 Mobile 송신을 연쇄 호출하지 않는다.
    break;
case NET_CAN_NO_CAPACITY:
case NET_CAN_CONSUMER_REJECTED:
    // 원 C2 반환·반영 단계·이 세 센서의 원 식별을 보존한다.
    // Core가 이미 반영됐는지 불명확하면 같은 입력 재호출을 보류한다.
    break;
case NET_CAN_TIME_HOLD:
case NET_CAN_ORDER_HOLD:
case NET_CAN_CONTRACT_HOLD:
case NET_CAN_NOT_READY:
case NET_CAN_INVALID_ARGUMENT:
default:
    // 미확인 수신을 새 환경값으로 게시하거나 다른 센서 품질을 낮추지 않는다.
    // 다음 실제 관측/계약 근거를 기다리며 정상 표시 완료를 생성하지 않는다.
    break;
}
```

### SC05의 frame 없는 로컬 무효화 호출 지점

실제 온습도 만료·해당 메시지 무수신·수용된 CIS Boot 변경은 새 CAN frame을 만들지 않는다. M09가 보관 중인 동일 raw/meta/C2와 사건을 연결하고 M03의 기존 직렬화 문맥으로 [로컬 무효화](../../functions/can/CIS_ENVIRONMENT.md#local-invalidation)를 호출한다. `observedEnvironmentEvent`는 [NetCisEnvironmentLocalEvent](../../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)의 실제 사본이다. 원 `retainedRaw/retainedMeta/retainedC2`, 현재 record/event revision, 수용 Boot, 관측시각·미반영 경과, 해당 메시지의 마지막 정상 수신시각·채택된 중단 한도와 affectedChannels를 모두 보존한다. 시간이나 중단 정책 근거가 없으면 가짜 사건을 채우지 않는다.

```c
/* PSEUDOCODE_ONLY: 실제 M09 사건과 기존 Network 내부 함수의 분기 예 */
switch (observedEnvironmentEvent.kind) {
case NET_ENV_ORIGIN_EXPIRED:
case NET_ENV_RX_STOPPED: {
    NetCanResult invalidated = NetCan_InvalidateCisEnvironment(&observedEnvironmentEvent);
    // 기존 공개 환경 Rx 품질 경로. 원 반환·Core/Observer를 보존하며 자동 재호출하지 않는다.
    // TIME_HOLD/ORDER_HOLD/CONTRACT_HOLD 등은 새 정상·수신시각·원seq를 만들지 않는다.
    break;
}
case NET_ENV_BOOT_CHANGED: {
    // M01이 자기 구 환경 대기/콜백·미시작 Query 재게시를 막고 자체 callback을 직렬화.
    NetCanResult invalidated = NetCan_InvalidateCisEnvironment(&observedEnvironmentEvent);
    // 이 분기는 공개 Rx NO_DATA를 호출하지 않는다. 현재 ABI/Binding 미확인으로 CONTRACT_HOLD.
    // 향후 DOMAIN 소유 동기 private 한 번 제출·미래 commit 봉쇄/무효화/구 Query 차단 후
    // 현재 요청 내용·수용 BOOT·적용 Epoch·명확한 완료를 대조해야 B 정상 gate 검토 가능.
    // 현재 다른 Epoch EPOCH_ORDER_UNAVAILABLE를 성공/다음 요청으로 바꾸지 않는다.
    // 원 결과·불명 상태를 보존하고 자동 재시도하지 않는다.
    break;
}
default:
    // 기존 INVALID_ARGUMENT 경계. 정상 인계나 가짜 BOOT 사건을 생성하지 않는다.
    break;
}
```

온습도 만료는 기존 유효 채널의 품질을 INVALID/STALE로 제한하고 기존 비유효 채널의 원 사유를 보존한다. 일반 같은 BOOT 수신중단의 STALE/NO_DATA 선택은 별도 미결이다. BOOT 무효화는 단일 동기 private 전환 목표이며 같은 전환을 공개 Rx NO_DATA로 중복 전달하지 않는다. 원 raw·sequence를 보존하고 AGE는 입증한 미반영 경과만 한 번 더한다. 새 유효 온도만 온도를 회복시키며 조도 정책을 온습도2500ms에서 추정하지 않는다. **BG-R05-BOOT / BLOCKED_DESIGN:** [해당 분기](../../functions/can/CIS_ENVIRONMENT.md#boot-binding-gap)는 담당자 부분 Core/Host 완료 보고와 별개로 실제 private ABI·연속 Epoch 수용·Rx/Query Binding 미검증 때문에 현재 HOLD다. 일반 NO_DATA로 BOOT/순번 reset이나 새 Epoch를 만들지 않는다.

BOOT A/seq900 → 검증된 새 B/seq0 → 늦은 A/seq901에서는 DOMAIN private 경계가 A의 미래 commit을 봉쇄하고 실제 환경 무효화·구 GOOD Query 방지를 완료해야 한다. 구 객체가 남아도 그 보장과 요청/Epoch/완료 대조가 성립하면 가능하다. B0을901로 바꾸거나 A900과 전역비교하지 않는다. 현재 미바인딩 상태에서 B 정상 소비·회복을 성공으로 만들지 않으며 기억된 지연 A는 재인계·정상수신/복구 근거로 쓰지 않는다.

**실패·복구.** 온도와 습도의 원 사용기한은 각각 합의된 2500ms다. 각 채널의 현재 누적 AGE가 2499ms이면 시간 필요조건만 통과하고, 2500ms와 2501ms에서는 그 채널을 사용할 수 없다. 조도1000ms와 CIS 로컬5000ms는 별도 미확정 정책이며 온습도 합의로 확정하지 않는다. 보고200ms·수신 중단600ms 후보로 원 TTL을 대신하지 않는다. 늦은 frame·동일 sequence 재전송·CIS 재기동·bus-off 복귀는 새 측정이 아니다. 새 유효 온도만 들어오면 온도만 회복하고 습도·조도는 기존 원 품질/AGE로 남긴다. DROP/Queue full은 전체 새 접수 실패이며 원 순서장부를 먼저 진전시키지 않는다. 표시용 열화 사실 보존과 기능 제어 사용 제한은 구별한다.

**HOLD와 종점.** 온습도2500ms와 기존 C2 품질을 통한 무효화 방향은 합의된 하위 조항이다. IF-O19/IF-O20/IF-O24의 나머지 값별 변환·같은 관측 연결, 실제 C2/VSM/ESP32/App 바인딩은 OPEN이다. 상태형 흐름의 종점은 Owner가 원 품질을 유지해 소비·표시한 실제 근거이며 Network 전송만으로 이를 완료 처리하지 않는다.

<a id="sc06"></a>
## SC06 — Pi 카메라 판정·CIS 경유 전달과 잔류 탑승자 경고 연결

**원 직접 집합: 7 Wire / 7 Rx.**

- Wire: `WIRE-P01-CIS_OCCUPANT`, `WIRE-P01-CIS_STATUS`, `WIRE-P01-VSS_WARNING_STATE`, `WIRE-P02-M_WARNING`, `WIRE-P02-M_CIS_OCCUPANT`, `WIRE-P03-P_VISION_RESULT`, `WIRE-P03-P_VISION_STATUS`
- Rx: `RX-P01-CIS_OCCUPANT-DOMAIN`, `RX-P01-CIS_STATUS-DOMAIN`, `RX-P01-VSS_WARNING_STATE-VSS`, `RX-P02-M_WARNING-ESP32`, `RX-P02-M_CIS_OCCUPANT-ESP32`, `RX-P03-P_VISION_RESULT-CIS`, `RX-P03-P_VISION_STATUS-CIS`
- Node / Link: `N06;N03;N01;N05;N08;N04` / `P03;P01;P02;P04;R06;R02;R05;I06;I03;I01;I05;R03;I04`.
- 원 제목·집합은 S4C `evidence/s4c_trace.json`에서 그대로 인수했다.

**시작과 계보.** Pi 비전 Owner가 새 Presence/Count 판정을 만든다. Pi runtime은 그 관측의 실제 생성 세대와 `pi_boot_id`를 결합한다. PiBoot/VisionSequence/원AGE, 두 독립 validity/reason, 동반 Pi 상태/고장은 한 원 record다. 독립 `P_VISION_STATUS.status_sequence`는 새 카메라 판정 번호가 아니다.

| 기능 연결 순서 | 정확한 기존 API·Function | 기능 결과와 HOLD |
|---|---|---|
| Pi 판정/상태 → P03 | `NetSubmitResult PiNet_PublishVisionResult(const NetVisionResult *message);`, `NetSubmitResult PiNet_PublishVisionStatus(const NetVisionStatus *message);` / [Result](../../functions/uart_c/P_VISION_RESULT.md), [Status](../../functions/uart_c/P_VISION_STATUS.md) | 원 판정과 상태는 독립 접수·AGE. 제출 OK는 CIS 수신이 아님 |
| CIS UART 완전 frame → 취합/진단 Owner | `void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta);`, `void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta);` / [Pi→CIS](../UART_C_PI_TO_CIS.md) | 반환 전 필요한 원 record/meta 복사. CRC/부분 frame은 typed 인계 없음; 정상 INVALID는 원 사실로 보존 |
| CIS 같은 판정의 제한 평가 → P01 | `NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message);` / [B16 연결 검사](../../nodes/CIS_PI.md#origin-check), [CAN Occupant](../../functions/can/CIS_OCCUPANT.md) | Pi 원 품질/사유와 CIS 추가 제한을 혼동하지 않음. 확정된 같은 관측의 실제 제한만 원자료와 함께 검사; O13의 나머지 사유 우선·상태 mapping 근거가 없으면 그 투영은 `BLOCKED_BY_CONTRACT` |
| DOMAIN Rx → 탑승자 기능 Owner | `DomainIf_Status_t DomainIf_RxCisOccupant(const CisOccupantState_t *state);` / [Rx Adapter](../../functions/DOMAIN_RX_ADAPTER.md) | Pi 원 sourceGeneration 연결·UNKNOWN255 비사용 표현 미확인 HOLD. CIS CAN Boot/TX로 Pi 계보 대체 금지 |
| DOMAIN 잔류 위험 판단 → 음향과 표시의 형제 출력 | `DomainIf_TxVssWarningState`, `DomainIf_TxMobileWarning`, `DomainIf_TxMobileCisOccupant` / [출력 연결](#hazard-output) | 실제 OCCUPANT_HAZARD·해제 조건은 DOMAIN. Network가 과온 결합 임계값·새 위험 판단을 만들지 않음 |

`CIS_STATUS`는 이 시나리오의 직접 진단 경로이며 상세는 [SC10](#sc10)을 따른다. Permission 소비는 기능 사용의 간접 조건([SC08](#sc08))으로 연결하되 SC06 직접 집합에 P_PERMISSION을 추가하지 않는다.

### SC06의 같은 원 관측 연결과 접수

아래 입력은 CIS Owner가 안정 보관한 원 `NetVisionResult`, 그에 대응하는 완전한 `NetCisOccupant` 후보, 각 연결에 기록한 실제 UART meta다. 불변성·같은 사건·원 Pi 세대·시간·품질 mapping 증거는 기존 B16 계약을 따른다. 현재 미합의 증거는 false/HOLD이며 아래 bool을 임의 true로 설정하지 않는다. visionRestrictions는 Presence/Count 각각의 원 관측과 실제 현재 제한 사건을 결합한 NetVisionRestrictionEvidence 두 기록이다. 같은 처리 구간 동안 원 record/meta와 함께 안정 보관하며, 없으면 NULL을 전달해 원 품질·사유 그대로의 경로만 허용한다. 구체 생성·거부 조건은 기존 [Vision 투영 검사](../UART_C_PI_TO_CIS.md#projection)를 따른다.

```c
NetUartCResult check = NetNodeCis_CheckOccupantTransfer(
    &sourceVision, &candidateOccupant, &capturedMeta, &projectionMeta,
    unaccountedCisElapsedMs, unaccountedHopMs,
    sourceBindingVerified, timeBasisVerified,
    qualityMappingAdopted, cisStateAssociationProven, visionRestrictions);
if (check == NET_UART_C_OK) {
    NetSubmitResult local = CisNet_PublishOccupant(&candidateOccupant);
    switch (local) {
    case NET_SUBMIT_OK:
        // CIS own 사본 접수만 확인. CAN terminal·DOMAIN 인수·위험 판단은 각각 대기다.
        break;
    case NET_SUBMIT_INVALID_ARGUMENT:
    case NET_SUBMIT_NOT_READY:
    case NET_SUBMIT_NO_CAPACITY:
        // 원 Pi record·품질·AGE를 보존한다. 제출 실패를 Count0/정상 부재로 바꾸지 않는다.
        // 새 부분 게시와 과거 항목 덮어쓰기는 없으며 이번 시도의 실패만 관측한다.
        break;
    default:
        // 정의 밖 반환은 성공으로 해석하지 않고 실제 바인딩 확인 대상으로 둔다.
        break;
    }
} else {
    // 형식/동일 관측/시간/품질 계약 중 실패한 지점 보존. 원 자료는 변경하지 않는다.
    // STATUS READY만으로 check를 우회하거나 candidate를 VALID로 고치지 않는다.
}
```

검사 OK도 해당 관측의 최신성·차량용 사용 허용을 별도로 요구한다. `unaccounted*`는 source에 아직 포함되지 않은 구간이며 현재 AGE에 RX 이후 전체 경과를 재가산하지 않는다. Source/후보는 호출 동안 borrowed이며 접수 OK 뒤 Network는 caller 포인터를 보관하지 않는다.

**실패·복구.** Presence INVALID/사유 보존과 Count VALID2는 양립한다. Count255는0명이 아니며 CIS READY로 두 품질을 정상화하지 않는다. Pi Status만 수신한 동안에는 원 관측이 없거나 늙은 상태를 유지한다. UART-C 부분 frame·overflow·CRC 오류는 완전 검증 전 Owner 상태를 갱신하지 않고 기존 Router의 재동기화 절차를 따른다. Pi 또는 CIS reset 뒤 과거 관측에 새 Boot를 찍지 않는다. Pi 새 원 관측·현재 사용 허용·수용된 계보를 확인한 때 해당 기능 Owner가 재평가한다. App 읽음·VSS 오디오 정지는 위험 해제 증거가 아니다.

**HOLD와 종점.** IF-O13/IF-O09/G02/IF-O24/IF-O02/CF02를 유지한다. Pi 원 판정, CIS 제한, DOMAIN 위험 판단, VSS 실제 출력, App 표시는 독립 결과다. 어느 한 단계의 성공으로 전체 성공을 집계하지 않는다.

<a id="sc07"></a>
## SC07 — 후방 관측→DOMAIN 위험 판단→음향·표시

**원 직접 집합: 4 Wire / 4 Rx.**

- Wire: `WIRE-P01-CIS_REAR`, `WIRE-P01-VSS_WARNING_STATE`, `WIRE-P02-M_WARNING`, `WIRE-P02-M_CIS_REAR`
- Rx: `RX-P01-CIS_REAR-DOMAIN`, `RX-P01-VSS_WARNING_STATE-VSS`, `RX-P02-M_WARNING-ESP32`, `RX-P02-M_CIS_REAR-ESP32`
- Node / Link: `N03;N01;N04;N05;N08` / `P01;P02;P04;R02;R03;R05;I03;I01;I04;I05`.
- 원 제목·집합은 S4C `evidence/s4c_trace.json`에서 그대로 인수했다.

**시작과 입력 인계.** CIS 초음파 Owner가 새 후방 관측을 만든다. `NetSubmitResult CisNet_PublishRear(const NetCisRear *message);`부터 `DomainIf_Status_t DomainIf_RxCisRear(const CisRearState_t *state);`까지의 [대표 수직 흐름](../CIS_REAR_144_TO_344.md)과 [Ingress](../../functions/DOMAIN_CIS_REAR_INGRESS.md)가 유일한 상세 정본이다. B11의 입력 구간 종점을 B17의 기능 완료로 오인하지 않는다.

raw `rear_distance_deci_cm`의 0.1cm는1mm이므로123raw→123mm다. 정수 `/10`으로12mm를 만들거나12cm→120mm로 절삭하지 않는다. 0은 유효 거리 표현이며65535는 거리 sentinel이다. `VALID/NO_OBJECT`와 `FAULT/UNAVAILABLE/RECOVERING/INACTIVE`를 구별하며 사용 불가를 위험 CLEAR로 바꾸지 않는다. 같은 CIS Boot/update와 원 품질/소비 AGE를 DOMAIN에 전달하며 DOMAIN sourceGeneration 후보를 원 CAN Boot와 자동 동치로 확정하지 않는다.

**기능 종점까지의 연결.** DOMAIN의 위험 Owner가 실제 원 거리·품질·사용 한도와 별도 후방 활성 근거를 소비하여 위험/활성 의미를 산출한다. Network는 위험 임계값이나 활성 결정 본체를 작성하지 않는다. Owner가 만든 VSS Warning과 Mobile Warning은 아래 공통 출력으로 각각 인계한다. 같은 후방 상태의 `DomainIf_Status_t DomainIf_TxMobileCisRear(const CisRearState_t *state);` → [M_CIS_REAR](../../functions/uart_m/M_CIS_REAR.md)는 App 관측 표시 경로다. C2 Rx 성공만으로 그 snapshot이 해당 raw CAN의 것인지 증명할 수 없으므로 [동일 관측 연결](../../functions/DOMAIN_QUERY_PROVENANCE.md)을 먼저 확인한다.

<a id="hazard-output"></a>
### SC06·SC07 — DOMAIN 판단 이후 음향·표시의 분기

| 출력과 실제 소비 Owner | 기존 API·Function 정본 | 확인할 종료·실패 의미 |
|---|---|---|
| DOMAIN 위험 Owner → VSS 의미 입력 | `DomainIf_Status_t DomainIf_TxVssWarningState(const DomainIf_VssWarningState_t *state);` → [TxPort/Service](../../functions/DOMAIN_TX_PORT.md) → [VSS_WARNING_STATE](../../functions/can/VSS_WARNING_STATE.md) → `void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);` | A1 WARNING60 used60/frame64/DLC15 선택; WARNING51은 이력. 실제 양단 producer/ABI·시간 검증 전 CF02 HOLD. CAN TX/void 반환은 실제 음향 시작·완료가 아님 |
| VSS 현재 유효 Warning → 실제 음향 | [VSS Owner 경계](../../nodes/VSS.md#event-warning), [SC13/14](WINDOW_VSS_HEALTH.md) | VSS가 의미·원 AGE·품질·활성 조건으로 중재. 위험 재판정·실행 완료 생성은 Network 범위 밖. 소리 중단으로 DOMAIN 위험을 CLEAR하지 않음 |
| DOMAIN 위험 Owner → Mobile 경고 | `DomainIf_Status_t DomainIf_TxMobileWarning(const DomainIf_WarningInfo_t *warning);` → [M_WARNING](../../functions/uart_m/M_WARNING.md) → [ESP32](../../nodes/ESP32_MOBILE.md) → BLE → App | 원 DOMAIN 경고 occurrence/품질/AGE와 현재 recipient Device/Session 보존. UART/BLE 접수는 화면 인지·읽음이 아님; [SC16](QUERY_WARNING_SESSION.md)로 읽음/위험 해제를 분리 |
| DOMAIN 원 관측 → Mobile 상태 | `DomainIf_Status_t DomainIf_TxMobileCisOccupant(const CisOccupantState_t *state);` 또는 `DomainIf_TxMobileCisRear` → [M_CIS_OCCUPANT](../../functions/uart_m/M_CIS_OCCUPANT.md) / [M_CIS_REAR](../../functions/uart_m/M_CIS_REAR.md) | 관측 표시와 경고 의미는 별도. 한 경로 송신 실패가 다른 경로의 Owner 결과를 소급 변경하지 않음 |

외부 Owner가 산출한 `DomainIf_VssWarningState_t`와 `DomainIf_WarningInfo_t` 전체 사본은 각각의 실제 API에 전달한다. 두 API를 원자적 동시 성공으로 보장하는 새 wrapper는 없다. 한 쪽 로컬 접수 OK·다른 쪽 QUEUE_FULL이면 두 원 반환과 원 경고 identity를 각각 남긴다. 이미 받은 쪽의 Core나 음향을 자동 재실행하지 않으며 복구 후 현재 유효 Warning에 대해 각 Owner의 기존 정책으로 재평가한다. VSS_STATUS는 출력 서비스의 간접 근거([SC13/14](WINDOW_VSS_HEALTH.md))이며 특정 경고의 음향 완료 ACK가 아니다.

**실패·복구.** 후방 누락/만료·bus-off·오래된 update의 역전은 위험 CLEAR나 음향 성공을 생성하지 않는다. 원 Driver terminal과 release를 독립 마무리하고, 새 유효 후방 관측·현재 후방 활성/경고로만 기능 재평가·음향 재중재한다. 이전 일회 Event 자동 재생은 하지 않는다. UART Session 교체는 Mobile recipient만 바꾸며 원 CIS 관측을 새 관측으로 만들지 않는다. CF02가 남으면 음향 경로는 계속 HOLD이고 독립 원 상태 표시 계약까지 합의됐다고 확대하지 않는다.

**HOLD.** CF02/IF-O19/IF-O02/IF-O17 및 공통 시간·C2 동일 관측 바인딩. 위험 해제의 종점은 DOMAIN 기능 판단이며 App 읽음·출력 정지·CAN ACK와 다르다.

<a id="sc08"></a>
## SC08 — 원 Permission fan-out과 Pi 사용 허용 경유

**원 직접 집합: 2 Wire / 5 Rx.**

- Wire: `WIRE-P01-DOMAIN_PERMISSION`, `WIRE-P03-P_PERMISSION`
- Rx: `RX-P01-DOMAIN_PERMISSION-BCM`, `RX-P01-DOMAIN_PERMISSION-CIS`, `RX-P01-DOMAIN_PERMISSION-WINDOW`, `RX-P01-DOMAIN_PERMISSION-VSS`, `RX-P03-P_PERMISSION-PI`
- Node / Link: `N01;N02;N03;N04;N06;N07` / `P01;P03;R01;R02;R03;R04;R06;I01;I02;I03;I04;I06;I07`.
- 원 제목·집합은 S4C `evidence/s4c_trace.json`에서 그대로 인수했다.

**시작과 직접 분모.** DOMAIN의 독립 power·operation·alive 판단을 기존 합성 경계에서 연결한다. 한 `DOMAIN_PERMISSION` CAN Wire에 BCM/CIS/WINDOW/VSS의 독립 Rx4가 있고, CIS가 제한 중계하는 `P_PERMISSION` UART-C Wire/Rx1을 더해 **2 Wire / 5 Rx**다. WINDOW 수신 의무는 추적하되 실행·표시·감시를 활성화하지 않는다. MOBILE Session은 두 메시지에 직접 적용되지 않는다.

| fan-out 또는 relay 경계 | 정확한 기존 API·정본 | 소비 책임·출력 한계 |
|---|---|---|
| DOMAIN 원 독립 입력 → CAN 합성 | `NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);` / [Permission 부분 입력·합성](../../functions/DOMAIN_TX_PORT.md#txCisPowerPermission), [CAN leaf](../../functions/can/DOMAIN_PERMISSION.md) | C2 부분 입력3을 메시지3으로 만들지 않음. 합성 근거 없는 독립 입력 AGE/품질을0/OK로 채우지 않음 |
| CAN → BCM | `void BcmNetPriv_OnDomainPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);` / [BCM private 감독](../../nodes/BCM.md) / `RX-P01-DOMAIN_PERMISSION-BCM` | DOMAIN liveness·원 기동 감독. BCM 동작 완료나 새 기능 허용 생성 없음 |
| CAN → VSS | `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` / [VSS](../../nodes/VSS.md) | Network-private 감독이며 새 VSS App 필수 API·복구 권한이 아님 |
| CAN → WINDOW | `void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` / [WINDOW](../../nodes/WINDOW.md) / `RX-P01-DOMAIN_PERMISSION-WINDOW` | `IMPLEMENTATION_HOLD / DESIGN_PRESERVED`. inactivity를 차량 고장으로 만들지 않고 STOP도 자동 활성화하지 않음 |
| 같은 CIS CAN pair → App와 private relay | `void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` 및 `NetSubmitResult CisNetPriv_RelayPiPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);` / [기존 relay](../UART_C_CIS_TO_PI_PERMISSION.md) | 동일 검증 입력의 형제 소비. 둘의 직렬 호출 순서·성공 종속을 새로 정하지 않음 |
| CIS 원 제한 보고 → Pi 실제 소비 | [P_PERMISSION](../../functions/uart_c/P_PERMISSION.md) → `void PiApp_OnCisPermission(const NetPiPermission *message, const NetUartRxMeta *meta);` → 기존 `NetUartC_CheckPermissionUse` | CIS Boot/revision만으로 payload에 없는 원 DOMAIN Boot/update를 복원할 수 없음. 원 association·기한·품질 근거 없으면 허용 HOLD |

CAN source Boot/TX, DOMAIN 원 성공평가/시각 근거와 별도 CAN 합성 UPDATE/AGE, CIS relay Boot/revision, UART LinkSequence, Pi 로컬 link epoch를 따로 보관한다. [Permission 번호 공간](../CAN_BCM_AND_PERMISSION.md#permission-sequences)의 생성 주체·변경 사건을 따르며 CAN 합성 번호를 DOMAIN 원 성공 번호로 복원하지 않는다. 원 `NetDomainPermission`에는 relay reason이 없으므로 reason=NONE·USE_LIMIT=1500·revision0을 임의 기본값으로 생성하지 않는다. 두 입력 품질의 enum 숫자 크기로 합성 우선순위를 만들지 않는다.

### SC08의 Pi 사용 경계 호출 지점

`admittedPermission`은 정확 schema/CRC·수용된 CIS 세대·순서·현재 receiver epoch를 확인한 같은 UART record의 stable 사본이다. `useEvidence`는 [기존 원 한도/relay 한도·동등 경계·원 association·AGE 근거 구조](../UART_C_CIS_TO_PI_PERMISSION.md)의 실제 증거이며 별도 새 Port·설정 스위치가 아니다. 아래 코드는 Pi 기능 본체나 전원 제어를 구현하지 않는다.

```c
NetUartCPermissionUse use = NetUartC_CheckPermissionUse(
    &admittedPermission, &useEvidence);
switch (use) {
case NET_UART_C_PERMISSION_ALLOW_CANDIDATE:
    // 계약 검사의 필요 조건만 통과했다. 실제 차량용 결과 사용은 Pi Owner가 결정한다.
    // 이 결과로 프로세스 시작·전원 인가·새 Permission을 생성하지 않는다.
    break;
case NET_UART_C_PERMISSION_CONFIRMED_DENIAL:
    // 현재 출처·수명까지 확인된 원 0/OK 불허 사실을 보존한다.
    break;
case NET_UART_C_PERMISSION_UNCONFIRMED:
    // 원1/INVALID,255,포화/만료는 현재 허용 근거가 아니다. raw를0/OK로 바꾸지 않는다.
    break;
case NET_UART_C_PERMISSION_FORMAT_ERROR:
case NET_UART_C_PERMISSION_CONTRACT_HOLD:
default:
    // 정상 허용 소비 없음. 기존 원 보고를 보존하고 누락된 실제 결합 근거를 기록한다.
    break;
}
```

**실패·복구.** 후방 활성 Power의 실제 원 판단은 DOMAIN이 약200ms마다 재평가하며 원 사용기한은 합의된500ms다. 같은 값이라도 실제 새 평가가 완료됐을 때만 원 평가순번·시각을 갱신하고 가능한 즉시 전달한다. 단순 UART-C 반복500ms나 재수신은 새 평가가 아니다. 원 AGE499ms는 다른 출처·Boot·품질·association 조건이 모두 유효할 때 시간 필요조건만 통과하며,500ms 도달부터 사용하지 않는다. CAN 송신50ms/수신 중단150ms, UART-C 반복500ms/수신 중단1500ms와 후방 거리150ms 후보는 별개다. relay USE_LIMIT1500ms 후보는 원500ms를 연장하지 않으며, 조건부 USE_LIMIT500ms도 같은 후방 Power 원이라는 양단 근거 없이 송신값으로 채택하지 않는다. O05의 power/operation/alive 합성, 원 association·clock/미반영 경과가 불명확하면 해당 경로를 HOLD한다. Quality 비유효 보고의 raw 보관과 허용 사용은 분리한다.

CAN bus-off 또는 원 DOMAIN 재기동으로 과거 허용을 Pi의 현재 근거로 복원하지 않는다. CIS 또는 Pi만 재기동해도 이전 cache·half-frame과 새 epoch를 섞지 않는다. 실제 새 유효 DOMAIN 판단·현재 수용 세대와 원 무효화 연결을 확인한 뒤 기능 Owner가 사용을 재평가한다. Pi 프로세스 종료·물리 전원 차단 정책을 새로 만들지 않는다. IF-O05/G02/IF-O09/G07 및 실제 supervision/Driver 바인딩은 OPEN이다.

<a id="sc10"></a>
## SC10 — CIS·Pi 일부 고장 및 통신 Health 전달

**원 직접 집합: 5 Wire / 5 Rx.**

- Wire: `WIRE-P01-CIS_STATUS`, `WIRE-P02-M_WARNING`, `WIRE-P02-M_AVAILABILITY`, `WIRE-P02-M_CIS_STATUS`, `WIRE-P03-P_VISION_STATUS`
- Rx: `RX-P01-CIS_STATUS-DOMAIN`, `RX-P02-M_WARNING-ESP32`, `RX-P02-M_AVAILABILITY-ESP32`, `RX-P02-M_CIS_STATUS-ESP32`, `RX-P03-P_VISION_STATUS-CIS`
- Node / Link: `N06;N03;N01;N05;N08` / `P03;P01;P02;P04;R06;R02;R05;I06;I03;I01;I05`.
- 원 제목·집합은 S4C `evidence/s4c_trace.json`에서 그대로 인수했다.

**시작.** Pi의 실제 새 비전 상태/고장 평가 또는 CIS의 실제 센서·진단·제공 경로 변화다. P_VISION_STATUS의 READY/ActiveFault/LastFault/StatusSequence/StatusAGE는 원 판정과 독립이다. Network UART/CAN 관측은 통신 사실이며 CIS 진단 Owner가 근거·영향 범위를 판정하기 전 센서 자체 고장이나 새 occurrence로 바꾸지 않는다.

| 연결과 Owner | 정확한 기존 API·정본 | 결과·회복 경계 |
|---|---|---|
| Pi 상태 → UART-C → CIS 진단 | `PiNet_PublishVisionStatus` → `CisApp_OnPiVisionStatus` / [Status leaf](../../functions/uart_c/P_VISION_STATUS.md), [상태 mapping](../UART_C_PI_TO_CIS.md#status) | Pi affected_function bit0/1은 Presence/Count, CIS bit0/1은 비전/온도. raw 복사·enum cast 금지 |
| 실제 CIS 부분 진단 → CAN | `NetSubmitResult CisNet_PublishStatus(const NetCisStatus *message);` / [CIS_STATUS](../../functions/can/CIS_STATUS.md) | 현재 fault/최근 fault/발생번호·센서별 상태·UART/CAN 상태 독립. 새 CIS_FAULT Wire 없음 |
| DOMAIN C2 → 기능 제한/가용성 Owner | `DomainIf_Status_t DomainIf_RxCisStatus(const DomainIf_CisStatusReport_t *status);`, 고장 부분의 기존 `DomainIf_Status_t DomainIf_RxCisFault(const FaultInfo_t *fault);` / [고장 별도 조건부 정본](../../functions/DOMAIN_RX_ADAPTER.md#api-DomainIf_RxCisFault) | 같은 raw 사건의 부분 반영을 각각 추적. 필수 소비 지원·원 meta/fault 변환 미확인 시 호출 전 HOLD. 일부 Core 반영 뒤 전체 재호출 금지 |
| DOMAIN의 실제 상태·가용성·경고 → App | `DomainIf_TxMobileCisStatus`, `DomainIf_TxMobileFunctionAvailability`, `DomainIf_TxMobileWarning` / [M_CIS_STATUS](../../functions/uart_m/M_CIS_STATUS.md), [M_AVAILABILITY](../../functions/uart_m/M_AVAILABILITY.md), [M_WARNING](../../functions/uart_m/M_WARNING.md) | 각각 Owner가 만든 의미를 TxPort로 제출. 현재 recipient로 BLE/App 전달하되 접수·표시 완료를 혼동하지 않음 |

SC10의 상태 송신 정확 선언은 `DomainIf_Status_t DomainIf_TxMobileCisStatus(const DomainIf_CisStatusReport_t *state);`, 가용성은 `DomainIf_Status_t DomainIf_TxMobileFunctionAvailability(const DomainIf_FunctionAvailabilitySet_t *availability);`이다. 위 공개 API 본체를 Network가 구현하지 않는다. 실제 Interface의 [DOMAIN 원형](../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txmobilefunctionavailability)과 [CIS API](../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishstatus)를 유지한다.

**실패·복구.** UART-C 무응답과 CAN bus-off는 서로 다른 링크 관측이다. 한쪽 정상 frame이 다른 링크·센서 품질을 회복시키지 않는다. Pi만 reset하면 CIS 독립 환경·후방 센서의 원 세대를 같이 바꾸지 않는다. 현재 ActiveFault0·READY를 새 평가로 보고해도 LastFault 이력은 별도로 남을 수 있고, 이전 INVALID Vision Result는 실제 새 판정 전까지 그대로다. 과거 Pi Fault가 새 CIS 평가 뒤 도착하면 원 Pi/CIS 세대·원 순서를 확인하고 현재/이력의 역할을 분리한다. 단순히 가장 늦게 수신한 frame을 최신 기능 상태로 사용하지 않는다.

**HOLD와 종점.** IF-O13/IF-O15/IF-O19의 진단/영향 기능 mapping, 실제 Health→Owner 바인딩, 공통 원 세대·AGE·C2 provenance를 보존한다. 정상 frame 수신은 통신 회복 관측이며 기능 회복은 새 실제 상태/측정과 Owner 판단에 따른다. 진단 표시·가용성·경고 중 하나의 UART 접수가 다른 둘의 성공을 뜻하지 않는다.

<a id="counterexamples"></a>
## 주요 반례의 전문 자체검수

아래는 원 S4C 각 시나리오의 거부·지연·누락·중복·역전·재기동·Session·bus-off·용량·복구10종을 읽고, 위 연결과 기존 Function에 대입한 **문서 검토 / NOT_RUNTIME**다. 입력 숫자는 정적 반례이고 제품 실측 결과가 아니다. 후방 Power500ms와 온습도2500ms의 사용기한은 위에서 인수한 확정 합의를 따른다. 실제 제품/Host/보드·BLE/음향 시험은 수행하지 않았다.

| 반례 / SC | 입력·이전 상태 | 경계 함수·부작용 제한 | 관측값·사후 상태 / 원 근거·HOLD |
|---|---|---|---|
| B17-SEN-01 / SC05 | 기존 온도-123·조도70000 정상, 새 습도65535/INVALID | CisNet_PublishEnvironment → NetCan_DeliverCisEnvironment, 정상 형식 열화의 해당 값만 제한 | 온도/조도 값·각 순번/AGE 보존, 습도0 합성 없음. CIS Data·SC05/IF-O19·20 |
| B17-SEN-02 / SC05 | 같은 원 관측 A: AGE20/RX100, B: AGE15/RX130, 소비150 | 기존 동일 관측 AGE 수용. A70/B35를 같은 로컬 시점에서 비교 | B를 새 fresh로 선택하지 않음. 최소70ms 근거 유지하되 모순 수용은 별도 HOLD. S4C AGE/IF-O24 |
| B17-SEN-03 / SC06 | Presence INVALID/VISION_FAULT, Count VALID2, 뒤 Pi Status READY | NetNodeCis_CheckOccupantTransfer·CheckVisionProjection, 원 record 불변 | Count 독립·Presence 원 INVALID/사유 보존. O13 미합의 투영 BLOCKED. Pi/CIS Data·SC06 |
| B17-SEN-04 / SC06 | UART29B 중 일부만 도착 또는 CRC오류, 기존 occupant 정상 | UART_C_FRAME_ROUTER, OUT/Owner 정상 갱신 없음 | 새 완전 검증 전 typed callback0회. link reset 시 이전 half-frame 섞지 않음. SC06/UART-C profile HOLD |
| B17-SEN-05 / SC07 | raw123을12 또는120mm로 변환하려 함; 별도 raw0/65535 | DOMAIN_CIS_REAR_INGRESS의 단위·품질 변환 | 123mm 보존,0 유효와65535 sentinel 구별, FAULT→CLEAR 없음. SC07/IF-O19 |
| B17-SEN-06 / SC06·07 | A1 WARNING60 선택, 실제 양단 producer/지원·시간 증거 없음 | VSS_WARNING_STATE, 실제 Binding 미확인 | CF02 CONTRACT_HOLD. 길이 자동선택·음향 완료·위험 CLEAR 생성 없음. SC06/07·VSS node |
| B17-SEN-07 / SC08 | 원 Permission을 CIS500ms 대기 뒤 Pi 수신, USE_LIMIT1500 후보 | ProjectPermission·CheckPermissionUse, 원 AGE 누적 | 후방 Power 원500ms 도달로 비사용. relay1500 후보로 연장하지 않음; 원 association과 USE_LIMIT 채택 근거는 별도 HOLD. SC08/IF-O05·G02 |
| B17-SEN-08 / SC08 | power1/INVALID 또는255, 원 DOMAIN association 미확인 | CheckPermissionUse, source raw 불변 | 허용 생성 없음; UNCONFIRMED와 CONTRACT_HOLD를0/OK 불허로 합성하지 않음. Pi Data/IF-O05·09 |
| B17-SEN-09 / SC08 | CAN 복구, WINDOW 무수신, 다른 수신자는 새 원 판단 수용 | 각 DOMAIN_PERMISSION Rx 의무 독립 | 2Wire/5Rx 추적 유지·WINDOW DEFERRED, 무수신 차량 Fault/동작 재개 없음. SC08/G07 |
| B17-SEN-10 / SC10 | Pi mask1·READY/active0/last2, 이전 Result INVALID | CisApp_OnPiVisionStatus → CIS 진단 mapping | mask1을 CIS 온전한 새 진단으로 cast하지 않음; Result 불변·최근2 별도. SC10/IF-O13·19 |
| B17-SEN-11 / SC05·06·10 | 새 DTO 사본 준비 중 Queue full, 순번 장부는 기존값 | 각 Publish/Admission 전체 게시 경계 | NO_CAPACITY·새 부분 게시 없음, 장부 선행 진전 금지. 해당 leaf·S4C 자원 계약 |
| B17-SEN-12 / 전체5SC | 로컬 접수 OK, release 먼저·terminal 미정 또는 terminal 먼저·release 미정 | 기존 M07/M08·NETWORK_SERVICE_LIFETIME | 원 identity/예약 유지 또는 frame 재사용 금지. 후발 결과를 새 Boot/epoch 항목에 귀속하지 않음. 공통 SC17/18·Driver HOLD |
| B17-SEN-13 / SC06·07 | Mobile 경고 로컬 OK, VSS 제출 거부; 뒤 BLE 재접속 | DOMAIN TxPort 각각의 원 반환·원 경고 identity | 한쪽 성공을 전체 완료로 합성하지 않음. 현재 Warning 재평가만, 옛 음향 Event 재생 없음. CF02·App 바인딩 |
| B17-SEN-14 / SC10 | Pi 무응답인데 CIS 온도는 새 유효 관측, CAN만 bus-off 후 복구 | 실제 link Health→진단 Owner·CisNet_PublishStatus | 온도 원 품질 유지, CAN 복구로 Pi 기능 READY 합성 없음. SC10/IF-O15·19 |

각 SC의 전체10종 원 검토와 원 SHA·읽기 범위는 결과 ZIP의 `B17_E2E_SCENARIO_18_COVERAGE.csv`, 원 S4C 대장 및 제출 자체검수 로그에 보존한다. 이 문서는5SC의 논리 추적을 완결하며 전체 기능의 생산 승인이나 런타임 성공을 판정하지 않는다.

## S6 이후 협의 인계

환경 O20, Pi/CIS 품질 O13·진단 O19, Permission O05/G02와 원 세대 O09, 동일 관측 O24, CF02·Driver/Health 바인딩의 기존 ID를 유지한다. 후방 Power500ms·실제 재평가 약200ms 및 온습도2500ms의 확정 합의와 남은 조건부 정책·실제 바인딩 미검증을 분리한다. Owner35를 늘리거나 닫지 않으며 Interface의 채택되지 않은 변화는 S6 완료 후 별도 승인·통합 반영 대상으로 남긴다. G5-0 게시 HOLD를 보존하며 실제 구현·채택은 별도 근거를 요구한다.

<a id="sc05-r1-storage-order"></a>
## PG6-SW02 SC05 — Boot 전이의 실제 저장 순서

[r1 조건부 저장 순서](../../functions/can/CIS_ENVIRONMENT.md#environment-storage-order)를 SC05의 기존 M01→M03/F08→M09→DOMAIN 동기 private 목표→무효화/Query 완료 대조→새 원 관측의 공개 환경 Rx→Getter 경로에 적용한다. DOMAIN 첫 전환·동일 Epoch 중복/충돌·동기 무효화 Host 완료는 담당자 보고이며 현재 다른 Epoch는 EPOCH_ORDER_UNAVAILABLE다. 연속 운영안은 발송된 Network 제안/DOMAIN 회신 대기이고 실제 private ABI·Rx/Query Binding이 없어 BG-R05-BOOT를 유지한다. 새 공개 API/완료 bool·callback·환경 generation을 만들지 않는다.

| 입력·경계 | 연결된 실제 분기와 남길 사실 |
|---|---|
| A900 queued, 검증된 B0 수용 | M01이 자기 A 환경 대기·미실행 callback·미시작 Query 재게시를 차단하고 B 원자료 보존. M03/F08가 고정 요청을 단일 동기 private에 한 번 제출하는 목표이며 ABI 미확인은 HOLD |
| A callback 실행 중 | Network 자기 callback을 owner 문맥에서 직렬화. 이미 DOMAIN에 인계된 A 후속 commit 봉쇄는 DOMAIN 책임. 단순 callback 반환/빈 Queue로 완료를 대신하지 않음 |
| A callback 반환, async VSM write pending | 객체가 남아도 미래 commit이 확실히 봉쇄되고 무효화/Query/완료 대조가 충족되면 가능. 봉쇄 여부 불명이면 B HOLD. 물리적 소멸 무한 대기/임의취소 없음 |
| DOMAIN 단일 동기 private 무효화 완료 | 고정 요청 내용/수용 BOOT/적용 Epoch/명확한 완료를 대조. VSM/Meta·순서 문맥과 구 GOOD Query 차단 근거 필요. BOOT용 공개 Rx NO_DATA를 중복 호출하지 않음 |
| 무효화 거부·부분 적용·완료불명/불일치 | 관련 환경 B 차단·원 요청/결과·오류 보존. A GOOD 제거를 추정하지 않고 자동 재호출·다음 Epoch 발급 없음 |
| Core·독립 Meta 실제 완료 후 optional Observer 실패 | 저장 실패와 구분하고 동일 commit 재실행 금지. 실제 저장/원본 연결이 충족된 범위에서 다음 gate 검토; Observer의 원 오류도 보존 |
| 완료 후 B0 인계 / 대기 AGE2499→2500→2501 | 원0 그대로,901 리매핑 금지. 실제 대기 경과를 원 기준에 한 번 더하고 온습도2500 이상 정상복원 금지. 채널 독립 |
| B 뒤 지연 A frame/callback | 기억된 종료 Boot의 전달·재저장을 차단. 처음 본 임의 Boot를 최신으로 수용하지 않으며 Boot 재사용/Network 재시작/이력상실은 재동기화 근거별 HOLD |
| 같은 Boot 일반 NO_DATA | 재부팅/seq reset/새 Epoch로 해석하지 않음. 일반 만료·수신중단은 기존 공개 Rx·시간·품질 규칙이며 STALE/NO_DATA 선택 미결을 BOOT private로 확정하지 않음 |

이 단계는 조건부 목표 설계이며 실제 Host/Board 시험 결과가 아니다. 현재 다른 Epoch 미지원·실제 private 선언/결과 ABI와 Rx/Query Binding 미검증을 유지한다. BG-R05-BOOT, RV-R05-03/07 및 IM-026/027/028은 같은 차단 이력이며 문서 정합화만으로 닫지 않는다.
