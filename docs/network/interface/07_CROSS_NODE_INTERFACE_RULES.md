# 노드 간 인터페이스 규칙

Interface v2 · r1 · [기술 제약과 연동 조건](06_TECHNICAL_CONSTRAINTS.md)

DOMAIN·Network·CIS·Pi의 생산·전달·소비 경계와 시간·품질·기동 규칙을 정의한다. 노드별 Data/API와 CAN/UART Wire를 함께 적용한다.

<a id="status"></a>
## 규격과 연동 조건

정의된 계약과 권장 프로필, 미확정 정책, 구현 검증 전 조건을 구분한다. 권장 프로필의 기능 적용에는 각 절의 전제조건이 필요하며 문서의 선언만으로 구현 동작을 보장하지 않는다.

| 표시 | 적용 의미 |
|---|---|
| DECLARED_SOURCE | 참조 헤더의 선언. 실제 빌드의 동일 헤더·ABI 확인 필요 |
| PROPOSED_DESIGN / WIRE_PROPOSED | 설계 또는 Wire 권장 프로필. 양단 지원 조건 확인 필요 |
| CONDITIONAL | 명시한 전제조건이 충족된 범위에서만 적용 가능 |
| POLICY_OPEN / POLICY_OR_CONTRACT_OPEN | 기능 정책 미확정. 해당 사용 조건 확정 전 정상 동작으로 취급하지 않음 |
| BINDING_OPEN / BINDING_NOT_VERIFIED | 실제 Port·Queue·Manager 연동 검증 전 |
| DEFERRED | 설계 유지·구현 보류 |

<a id="roles"></a>
## 1. 구현과 데이터 소유권

이 문서의 v0.6 후보 계약에서 DOMAIN은 공개 DomainIf_* 함수 72개, 생명주기 관리, Rx/Tx 래퍼, Query Getter 18개와 기존 Manager 연결을 구현한다. Network는 TxPort 콜백 26개, CAN/UART Codec·Queue·Adapter·프레임 검증·송수신 및 통신 원자료를 담당한다. SetTxPort 공개 함수 자체는 DOMAIN이 구현하고 Network가 실제 Callback을 구성해 등록한다. 미지원 Port는 성공으로 처리하지 않는다.

차량 상태·요청·명령·결과의 단일 관리 주체는 DOMAIN이다. Network가 관리하는 원 수신 객체·품질 근거·Queue·통신 Health는 두 번째 차량 상태 Manager가 아니다. Rx는 검증→디코딩→C2 인자 전달→DOMAIN Manager, Tx는 DOMAIN 래퍼→Network TxPort→통신, Query는 Network가 DOMAIN Getter를 호출한 뒤 응답을 전송한다.

C2 Rx·Query·Private Transition은 ISR이 아닌 **단일 담당 실행 문맥**에서 직렬 처리한다. 초기에는 짧은 수신 ISR과 Network Superloop를 사용하며 DOMAIN은 해당 호출 경계에서 FreeRTOS를 필수로 요구하지 않는다. Manager→C2 Init→Observer→TxPort→통신 시작 순서를 적용하고 실제 연결은 통합 시 확인한다.

CIS·Pi 기능 모듈은 기존 API·UART-C v1·C2 v0.6 후보에 맞춰 연결한다. S32K344/144 Network 코드는 Network, CIS 센서·power_gate·vision_logic는 CIS, Pi 비전과 Python Port 및 실제 비전 Permission 적용은 Pi 기능 모듈의 범위다. ESP32 내부 통신 구현과 MOBILE 직접 연동은 해당 모듈의 범위이며 Network는 ESP32–DOMAIN 외부 API/Wire를 담당한다. VSS 기능과 통신의 내부 연결은 하나의 구현 범위다.

VSS A1은 WARNING60(used60/frame64/DLC15)과 Event의 최초 실제 출력 시작에 적용할 `L=min(양의 USE_LIMIT,500ms,2000ms)`를 내부 선택했다. format0 정규 부재·RX 기준 AGE·개별 Warning lane·제한 Hold는 [VSS 입력 정본](02_NODES/VSS/10_DATA.md#a1-binding)을 따른다. 실제 양단 메시지 생산·시간원·음향 출력·연결은 미검증이며 Permission/CIS Epoch 계약을 변경하지 않는다.
<a id="query"></a>
## 2. 최근 요청 이력과 Query

DOMAIN은 Device별 최근 최대 8건을 보관하고, 최근 결과 Getter는 최신순 최대 5건을 제공한다. Network는 기존 M_RESULT를 기록당 1프레임으로 순차 전송하고 M_QUERY_END로 종료한다. 새 UART Frame은 없다. 5건은 최근 요청 결과 조회 범위이며 모든 상태 Query 응답의 총량 제한이 아니다.

동일 Device/Session/Request ID에 속한 기록 1건의 갱신 방식과 내부 저장 정책은 미확정이며 실제 ResultManager 연결 검증이 필요하다. 같은 요청의 중복 응답으로 새 표시 항목을 만들지 않는다. 진행 중인 요청을 완료 결과만으로 대체하거나 누락하지 않는지 ResultManager 연결에서 확인해야 한다. 이력 8건·조회 5건이라는 합의만으로 내부 저장·갱신 동작 전체가 정해진 것은 아니다.

ITEM_COUNT를 실제 로컬 UART 송신 완료 건수로 집계하는 것과 종결의 부분 실패·0건 의미는 Network–ESP32 연결의 권장 프로필이다. 이력 8건·조회 5건 규칙은 MOBILE 전달 완료 또는 모든 항목의 최신성을 보장하지 않는다. Getter 실패를 성공적인 빈 목록으로 표현하지 않는다.


<a id="permission"></a>
## 3. Permission

DOMAIN은 후방 활성 Power를 약 200ms마다 실제 재평가하며 원본 판단의 사용 기한은 500ms다. 값 변경은 즉시 전달하고 동일 값이라도 실제 새 평가가 성공하면 그 평가 순번·시각을 전달한다. CAN 50ms 송신/150ms 수신 중단과 UART-C 500ms 반복/1500ms 수신 중단을 적용한다. 단순 반복으로 원 AGE를 0으로 만들거나 기한을 연장하지 않는다. 미확인·만료·재기동 이후 이전 ALLOW를 유효한 값으로 복원하지 않는다.

CIS는 같은 검증된 NetDomainPermission과 수신 메타를 기존 App Callback과 private Pi relay에 전달한다. App은 후방 기능을, Network relay는 검증·제한·중계를 담당한다. 별도 App용 허용 생성 API를 만들지 않는다.

P_PERMISSION.USE_LIMIT는 1500ms 후보이며, 같은 후방 Power 원 판단을 Pi까지 전달함이 확인되면 500ms로 싣는 조건부 권장 프로필이다. 실제 송신값은 양단 지원·만료 동작 검증 전에는 확정되지 않는다. 확인된 원본 Permission의 사용 기한과 USE_LIMIT 중 더 짧은 기한을 적용한다. Pi 카메라 중지·비전 게이트의 기능 적용과 통신 수신·만료 검사는 별개다.

정책 미결: POWER_OPERATION_STATE의 생성원, Power/Operation/Alive의 필수 입력, 단일 UPDATE_SEQUENCE/SOURCE_AGE 합성. 기존 합성 후보를 보존하며 확인되지 않은 품질·원 시간을 생성하지 않는다. WINDOW 미구현 때문에 확인된 Power를 자동 불허로 바꾸지 않는다. 원 품질·공통 AGE를 입증하지 못하면 그 입력으로 정상 ALLOW를 주장하지 않는다. WINDOW 관련 작동을 재개하거나 새로운 DTO·Wire 필드를 자동 추가하지 않는다.


<a id="permission-delta"></a>
### Permission Adapter의 기본 전달

기존 C2의 Power 값·Quality·원 평가 시각을 사용한다. 확정된 ALLOW/DENY만 `OK`로 다루고, 미관측·FAILED·NO_DATA·만료는 허용 상태로 변환하지 않는다. `UNKNOWN+OK`를 기존 `false+OK`로 단순 변환하면 확정 DENY와 구분되지 않으므로 이 경우의 최종 C2 투영은 **DOMAIN 확인 전**이다.

DOMAIN 담당자 보고상 평가 FAILED/NO_DATA에서 이전 ALLOW를 즉시 철회하되, 성공 평가 Sequence는 증가하지 않을 수 있다. Network는 마지막 Sequence 값만으로 데이터를 중복 폐기하지 않고 **값과 Quality가 실제로 바뀌었는지**도 확인한다. 새 유효 평가와 단순 프레임 재송신은 구분하며, 오래된 ALLOW를 반복 송신으로 갱신하지 않는다.

Power/Operation/Alive의 전체 입력 합성과 공통 UPDATE_SEQUENCE/SOURCE_AGE의 세부 갱신 조건, 원인별 UNKNOWN Quality 투영은 기능 담당자와 양단 계약이 정해졌을 때 연결한다. 기존 Wire와 API는 유지하며 미확정값을 정상 허용으로 구성하지 않는다. [CAN Permission 메시지 계약](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)

<a id="timing"></a>
## 4. 원본 사용 기한과 통신 시간 기준

아래 경과 시간은 수신 프레임 도착 시각만이 아니라 원본 생성 시점부터 실제 소비 시점까지 검증하여 누적한 AGE를 뜻한다. 원본 사용 기한 도달에 따른 데이터 사용 중지와 통신 수신 중단 감시는 별도로 판단한다.

| 대상 | 보고/송신 | 수신 중단 | 원 사용 기한·현재 지위 |
|---|---|---|---|
| 후방 활성 Permission | CAN 50ms; 실제 DOMAIN 평가 약 200ms | CAN 150ms | 원본 평가 기준 500ms |
| CIS→Pi Permission | 새 평가·제한 시 가능한 즉시, 반복 500ms | UART 1500ms | 확인된 원본 사용 기한 이내. USE_LIMIT 1500 후보/500 조건부 |
| 온도·습도 | CAN 200ms 후보, 센서 약 2000ms 참고 | CAN 600ms 후보 | 원본 관측 기준 각각 2500ms |
| 실내 조도 | 환경 CAN 200ms 후보 | CAN 600ms 후보 | 1000ms 후보 미확정; 로컬 코드 1000과 구분 |
| 후방 거리 | CAN 50ms 후보 | CAN 150ms 후보 | Network 원본 사용 기한 150ms 후보; SysRS 원본 500ms 잠정안과 구분 |
| Pi 판정 | UART 100ms 후보, CAN 100ms 후보 | 각각 300ms 후보 | 원본 판정 사용 기한 1000ms 기존 후보 |
| Pi 상태 | UART 1000ms 후보 | UART 3000ms 후보 | 상태와 관측의 수명은 독립적이며, 상태 전송으로 새 관측을 만들지 않음 |

후방 거리 원본의 사용 기한 150ms는 Network 권장 후보이며, 후방 활성 Permission의 500ms 사용 기한과는 별개다. SysRS 최대 500ms보다 일찍 사용을 제한하는 조건이지만 실제 측정·전송·소비 가용성은 미검증이다. 거리 소비 정책 변경이 필요하면 해당 한도와 센서·전달 주기를 함께 검토해야 한다.

경계값 예: 거리 유효조건이 모두 같을 때 effective_age 149ms는 시간 조건만 만족하고, 150/180/500ms에서는 거리를 사용할 수 없다. Permission은 별도로 499ms에서 시간 조건만 만족하며, 500ms부터는 사용할 수 없다. 어느 경우든 품질·기동·프레임 수신 상태가 불량이면 시간조건을 기다리지 않는다. 이 예는 설계의 기대값이며 실제 시험 결과가 아니다.

CIS 참고 코드의 ENV_STALE_MS 값 5000ms는 로컬 값 유효성 검사 후보이다. 진단 전용 정책으로 확정하지 않았고 DOMAIN의 2500ms 사용 기한을 연장하지 않는다. UART 50ms/CAN 5ms 홉 여유는 기존 후보이며 실측 상한을 보증하지 않는다.

<a id="identity"></a>
## 5. 기동·전송 순서·원 관측·현재 품질

CAN TX_SEQUENCE는 송신 출처·수용한 BOOT·CAN ID/메시지별로 비교한다. UART LINK_SEQUENCE는 기존 방향별 순서다. 원 센서 UPDATE_SEQUENCE, Pi VISION_SEQUENCE/STATUS_SEQUENCE, DOMAIN 성공 평가 Sequence, CAN 합성 UPDATE_SEQUENCE와 CIS relay UPDATE_SEQUENCE는 생성 주체·갱신 사건이 다른 번호 공간이다. CIS relay 순번은 DOMAIN 원 성공 평가 번호가 아니며 전송 순서로도 바꾸어 쓰지 않는다. 수치가 우연히 같아도 동일성의 근거가 되지 않는다.

올바른 프레임을 수용한 뒤 메시지 수신 감시를 반영하고 원 관측 신규성을 판단한다. 이 연결안은 정상 반복에도 기존 CIS Vision Result/Status Callback을 호출한다. 같은 관측의 반복으로 원 AGE를 초기화하지 않는다. 중복 Callback을 생략하는 최적화는 기존 수신 감시 경로가 같은 사실을 유지한다는 증거가 있을 때만 한다.

obs42/VALID 뒤 새 CAN TX_SEQUENCE의 obs42/INVALID가 오면 원 관측은 그대로지만 현재 품질 저하는 반영한다. 원 관측 순번 중복만으로 폐기하지 않는다. 이전 전송 프레임이나 단순 READY 상태만으로 과거 VALID 품질을 복구하지 않는다. 복원은 새 유효 관측과 기능 복구 규칙으로 한다. 원 AGE를 줄이지 않고 기존 C2 Rx→저장→Getter까지 검증한다.

Pi 원 INVALID 사유는 원 기록에 보존하되, 이후 실제 통신 단절·만료·기동 변경·STATUS 고장은 현재 사용 품질을 제한한다. Presence/Count 상호 모순은 같은 판정에서 두 필드가 각각 사용 가능한 경우에만 검사한다. Count255/INVALID 때문에 정상 Presence를 양수 인원수와 모순으로 판단하지 않는다. 한 필드 실패로 다른 정상 센서·필드를 일괄 무효화하지 않는다.

BOOT0 예약과 C2 sourceGeneration0은 같은 규칙이 아니다. 후보 C2의 유효 관측은 nonzero이며 알려진 원 세대는 무효 품질에서도 보존한다. 세대를 모르는0, 특히 STALE+sourceGeneration0의 허용/거부는 C2 후보에서 OPEN이다. 이를 임의 nonzero나 다른 품질로 치환하지 않는다. BCM 원 생산 세대와 CAN BOOT의 동일 lifecycle은 아직 확인되지 않았다.

같은 CIS BOOT에서 센서만 재초기화해 원 순번을 되감지 않는다. 수용한 새 BOOT의 낮은 순번은 이전 세대의 큰 순번과 단순 비교하지 않는다. 이미 종료한 Pi BOOT로 복귀하는 지연 A→B→A를 차단하는 규칙과 기억 범위는 검증 대상이다. 무작위 32비트 BOOT를 보안 인증이나 영구 replay 차단으로 표현하지 않는다.

<a id="environment"></a>
## 6. 환경 C2와 후방 상태

환경은 온도·습도·조도별 품질·순번·AGE를 독립 보존한다. 기존 `DomainIf_RxCisCabinEnvironment`는 환경 값 수신 경계다. CIS BOOT 변경에 따른 이전 환경값 무효화는 아래 DOMAIN 소유의 단일 동기 Private 경계에서 처리하며 전환용 NO_DATA를 공개 Rx에 중복 전달하지 않는다. 같은 BOOT의 일반 NO_DATA·만료·수신 중단은 기존 Rx·품질 정책을 유지하고, 링크 단절의 STALE/NO_DATA 선택은 별도 미결이다. VSM·독립 Meta·Getter까지의 실제 연동 검증은 완료 전이다.

현재 C2 구조를 우선 사용한다. 필요한 무효화·세대 전이를 표현하거나 연결할 수 없는 경우에만 최소 계약 변경을 검토한다. sourceGeneration 필드 추가는 현재 계약에 포함되지 않는다. 실제 연결 근거가 필요하며 표현 한계에 따른 향후 최소 변경 가능성은 남아 있다.

후방 raw0.1cm는 mm와 같다. 65535를 실제 거리로 읽지 않고 측정 종류·품질로 해석한다. INACTIVE=5는 불허/확인 불가/만료 사유와 함께 사용하며 NO_OBJECT로 가짜 안전을 만들지 않는다. 재활성 뒤 새 측정 전에는 이전 거리를 정상으로 복원하지 않는다. R단·위험·CLEAR 판단은 DOMAIN 기능이다.

<a id="environment-delta"></a>
### 환경 BOOT 변경 처리

CIS 환경 수신은 기존 C2 Rx에 온도·습도·조도별 값/품질/원 Sequence/AGE를 전달한다. 온도·습도 데이터의 사용 기한은 원본 관측 시점부터 각각 2500ms다. CIS BOOT가 바뀌면 Network는 해당 환경 경로에서 이전 BOOT에 속한 대기 데이터를 새 관측값처럼 전달하지 않는다.

DOMAIN이 동기 Private Transition을 제공하는 경우에는 이전 환경값과 순번 비교 문맥의 무효화가 완료됐음을 확인하고 새 BOOT 데이터를 전달한다. BOOT 무효화를 이유로 기존 공개 환경 Rx에 NO_DATA를 다시 보내지 않는다. Private 호출 경로가 연결되지 않았거나 완료 여부가 불명확하면 **해당 환경 입력만 제한**하고 오류 상태를 남긴다. DOMAIN에 저장된 이전 정상값까지 사라졌다고 가정하지 않으며, 실제 VSM·Query 반영은 연결 시험에서 확인한다.

DOMAIN Production Private Core는 최초 Epoch 1, 완료된 Epoch 이후 정확히 N+1, 완료된 동일 요청의 중복, 동일 Epoch의 ID 충돌, 과거(`STALE`)·건너뜀(`ORDER_ERROR`)을 실제 코드로 처리한다. `EPOCH_ORDER_UNAVAILABLE` 열거자는 호환용으로 남지만 현재 순차 Epoch 계약에서 반환하지 않는다. 다만 **Network→DOMAIN 실제 Private 호출·초기 상태 정합·완료 Receipt 대조·환경 C2/Query 연동은 미검증**이므로 해당 환경 경로의 HOLD는 유지한다. 연결 시 기존 동기 Private API와 검증된 원 BOOT·순번을 이용하고, 새 공개 C2 필드·Wire·영속 저장소를 추가하지 않는다. [DOMAIN 동기 Private 전환 경계](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#delta-environment-routing) · [Private 경계](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#delta-environment-routing)

<a id="failures"></a>
## 7. 접수 실패와 후발 실패

`NET_SUBMIT_OK`와 `DOMAIN_IF_OK`는 각각 해당 로컬 경계의 성공 반환이며 실제 CAN/UART TX·상대 ECU 수용·차량 DONE을 뜻하지 않는다. 특히 현재 `DomainIf_RxCisRear()`는 VehicleStateManager의 중복 입력도 `DOMAIN_IF_OK`로 변환하므로 이 반환값만으로 새 상태 반영을 단정하지 않는다. 신규성은 원 입력과 연결한 `DomainIf_QueryGetCisRear()` 전후 결과·`updateSequence`·`sourceGeneration`·값/품질로 별도 확인한다. 접수 전 INVALID_ARGUMENT/NOT_READY/NO_CAPACITY는 미접수 반환이며 그 항목의 후발 실패 사건을 만들지 않는다.

정상 접수된 필요한 명령·결과·일회성 이벤트는 driver 제출의 확정 최종 실패, 송신 전 만료, 송신 전 문맥 무효화가 기존 세 reason에 정확히 맞을 때만 DOMAIN LocalFailure에 원 식별자를 전달한다. 미분류 in-flight 오류·callback 부재를 세 reason으로 확장하지 않고 원 식별자·phase·진단과 영향 경로 HOLD를 남긴다. [실패 범위 정본](04_COMMON/20_API_USAGE_AND_FAILURE.md#r1-local-failure-reasons)을 따른다. 주기 상태·Query 실패는 각 통신/Query 경로를 사용하고 CAN 성공 Callback·CIS/Pi용 C2 함수를 추가하지 않는다. BCM_RESULT 수신/DONE만으로 CONFIRMED나 차량 FAILED/DONE을 합성하지 않는다. 실제 확인 근거와 binding은 검증 대기다. Door·Climate·Interior Light별 원 Command ID·세대·새 관측 시각·상태·품질/Fault의 충분조건은 BCM/DOMAIN 확인 대기이며, 근거가 부족한 기능 결과만 UNCONFIRMED로 제한한다.

<a id="scenarios"></a>
## 8. 노드 간 처리 흐름

다음 흐름은 양측 API와 Data의 연결을 설명한다. WINDOW 관련 흐름과 WINDOW 의존 가지는 구현 보류다.

| 흐름 | 메시지 연결 | 종료·복구 경계 | 연동 조건 |
|---|---|---|---|
| <a id="scenario-sc01"></a>수동 도어 요청·실행 결과·상태·음향 | [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [BCM_DOOR_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [BCM_DOOR_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_BCM_DOOR_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-door-state) · [VSS_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-vss-event) | 거부/실패 결과는 원 결과로 반환. 결과를 못 받으면 UNCONFIRMED이며 임의 FAILED/DONE 금지. 중복 전달은 새 구동/새 음향의 근거가 아님. 원 Result confirmation과 실제 전이 입증은 해당 결과 확인 조건. 유효한 새 세대/현재 상태를 확인하고 필요한 경우 조회. 이전 pending/명령 자동 재실행 없음. | 연동 검증 전 |
| <a id="scenario-sc02"></a>공조 설정·센서 입력·실제 출력 및 보호 | [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [CIS_ENVIRONMENT](03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) · [CIS_OCCUPANT](03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) · [BCM_CLIMATE_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [BCM_CLIMATE_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-state) · [BCM_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) · [M_USER_SETTINGS](03_COMMUNICATION/20_UART_M.md#message-m-user-settings) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_BCM_CLIMATE_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-climate-state) | 센서 일부 실패는 독립 정상값과 분리. 설정 DONE은 실내 목표온도 달성 증거가 아니다. 과열/불일치 등은 BCM 원 관측을 전달; local TX 실패로 ECU 고장을 만들지 않음. RECOVERY_CONFIRMED는 이전 목표의 자동 재구동 지시가 아니다. 새 유효 관측/세대 및 기능 Owner의 동작 판단 필요. | 연동 검증 전 |
| <a id="scenario-sc03"></a>실내 조명 설정·적용·표시 | [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [BCM_LIGHT_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [BCM_LIGHT_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-state) · [M_USER_SETTINGS](03_COMMUNICATION/20_UART_M.md#message-m-user-settings) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_BCM_LIGHT_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-light-state) | 적용 실패/미확인은 그대로 유지. APPLIED를 실제 점등 피드백으로 확대하지 않음. NORMAL 설정과 알림 출력은 구별; 부분 설정 존재 정보는 [표현·소비 조건](06_TECHNICAL_CONSTRAINTS.md#부분-usersettings-존재-정보)을 따른다. 현재 유효 적용 상태를 재확인하며 과거 명령을 자동 재실행하지 않음. | 연동 검증 전 |
| <a id="scenario-sc04"></a>디지털 키 문맥·자동 Unlock 상태/결과 | [M_CONTEXT](03_COMMUNICATION/20_UART_M.md#message-m-context) · [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [BCM_DOOR_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [M_DIGITAL_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-digital-status) · [M_DIGITAL_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-digital-result) | 단순 연결을 ACTIVE/권한/근접으로 합성하지 않음. 무효 session/문맥/만료는 새 자동 동작을 차단한다는 문서 관찰; 실제 전이를 성공 결과만으로 확정하지 않음. 재접속 시 새 유효 문맥과 원 관측을 확인; 과거 접근/결과를 새 사건으로 재생하지 않음. | 연동 검증 전 |
| <a id="scenario-sc05"></a>환경 센서 원값·부분 품질 수집과 표시 | [CIS_ENVIRONMENT](03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [M_CIS_ENVIRONMENT](03_COMMUNICATION/20_UART_M.md#message-m-cis-environment) | 한 센서 실패로 독립 정상 센서값 전체를 무효화하지 않음. 원 AGE가 지난 값·반복 프레임을 fresh로 만들지 않음. 원 사용 한도는 [센서 시간 규칙](#timing)을 따른다. 실제 새 측정이 들어와야 원 최신성 회복. 새 프레임/READY만으로 이전 원값을 승격하지 않음. | 연동 검증 전 |
| <a id="scenario-sc06"></a>Pi 카메라 판정·CIS 경유 전달과 잔류 탑승자 경고 연결 | [P_VISION_RESULT](03_COMMUNICATION/30_UART_C.md#message-p-vision-result) · [P_VISION_STATUS](03_COMMUNICATION/30_UART_C.md#message-p-vision-status) · [CIS_OCCUPANT](03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [M_CIS_OCCUPANT](03_COMMUNICATION/20_UART_M.md#message-m-cis-occupant) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 카메라 무응답/INVALID를 0명·없음으로 만들지 않음. CIS 일반 센서 정상 경로는 독립 유지. 원 Pi 품질과 CIS 제한의 단일 이유 표현은 [품질 규칙](02_NODES/CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)을 따른다. 잔류 탑승자 위험의 미확인 품질을 CLEAR로 바꾸지 않으며 미표현 의미는 [C2 enum 투영 제약](06_TECHNICAL_CONSTRAINTS.md#c2-enumwire-의미-보존) 확인 필요. Pi/runtime/process 세대 변경 후 실제 새 원 관측 필요. 이전 record를 새 Boot로 다시 stamp하지 않음. | 연동 검증 전 |
| <a id="scenario-sc07"></a>후방 관측→DOMAIN 위험 판단→음향·표시 | [CIS_REAR](03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_CIS_REAR](03_COMMUNICATION/20_UART_M.md#message-m-cis-rear) | NO_OBJECT, UNAVAILABLE, INACTIVE를 구별. 무효/만료를 정상 CLEAR로 바꾸지 않음. 원 활성 정보의 합성은 [합성 조건 미확정](06_TECHNICAL_CONSTRAINTS.md)이며 후방 활성 Permission 원 500ms와 분리한다. 새 유효 관측/활성 의미의 현재 경고로 재중재. 이전 음향 사건 재생 금지의 좁은 정책은 [VSS 경고 처리](#scenario-sc13). | 연동 검증 전 |
| <a id="scenario-sc08"></a>원본 Permission의 다중 전달과 Pi 기능 적용 | [DOMAIN_PERMISSION](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [P_PERMISSION](03_COMMUNICATION/30_UART_C.md#message-p-permission) | 독립 C2 입력 합성·누적 AGE·허용 생성원은 미확정이다. 만료/NO_DATA/기동변경은 새 허용으로 합성하지 않음. 전달 후보가 원 기한을 연장하거나 연속 허용을 보장한다는 주장 금지. 새 유효 DOMAIN 판단·수용 세대가 확인된 뒤 사용 여부를 기능 Owner가 판단. 이전 기동의 허용 복원 금지. | 연동 검증 전 |
| <a id="scenario-sc09"></a>BCM 보호 발생·현재 진단·가용성 | [BCM_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) · [BCM_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) · [M_BCM_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-bcm-status) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 통신 단절을 허구의 ECU FAILED/실제 고장으로 바꾸지 않음. current fault와 과거 발생 기록을 구별. bit/helper 대응은 [Fault 투영 제약](06_TECHNICAL_CONSTRAINTS.md#fault-bithelper-projection)에 따라 미확정이다. BCM 실제 복구 근거 전달. 복구 확인이 이전 동작의 자동 재구동 허가를 뜻하지 않음. | 연동 검증 전 |
| <a id="scenario-sc10"></a>CIS·Pi 일부 고장 및 통신 Health 전달 | [P_VISION_STATUS](03_COMMUNICATION/30_UART_C.md#message-p-vision-status) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [M_CIS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-cis-status) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 부분 비전·센서 실패와 건강한 경로를 분리. 새 CIS_FAULT 프레임을 발명하지 않음. 실제 Network health→진단 입력 연결 미확인. 실제 새 원 관측 및 기능 복구로 해당 부분을 재평가. 최근 고장 이력은 현재 정상과 별개. | 연동 검증 전 |
| <a id="scenario-sc11"></a>보류 WINDOW 이동·STOP·실제 결과·상태 | [DOMAIN_PERMISSION](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [WINDOW_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-window-command) · [WINDOW_STATE](03_COMMUNICATION/10_CAN_FD.md#message-window-state) · [WINDOW_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-window-result) · [M_WINDOW_STATE](03_COMMUNICATION/20_UART_M.md#message-m-window-state) | 허용 상실 후 새 이동 제한과 적법 STOP의 조건을 구분. local AntiPinch/스위치가 상위 목표보다 우선. 수신 성공/timeout을 실제 정지 확인으로 만들지 않음. WINDOW/DOMAIN의 별도 재개·실물 구성·보호/결과/표시 근거 확인 필요. 현재는 DEFERRED, 과거 이동 재생하지 않음. | WINDOW 구현 보류 |
| <a id="scenario-sc12"></a>보류 WINDOW 끝단·끼임·Fault와 경고 | [WINDOW_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-window-event) · [WINDOW_STATE](03_COMMUNICATION/10_CAN_FD.md#message-window-state) · [WINDOW_FAULT](03_COMMUNICATION/10_CAN_FD.md#message-window-fault) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_WINDOW_FAULT](03_COMMUNICATION/20_UART_M.md#message-m-window-fault) | END를 AntiPinch로 바꾸지 않음. enum·수신 여부만으로 movementStopped/outputDisabled를 true로 합성하지 않음. 별도 WINDOW 재개 확인 후 다룰 보류 경로. 현재 구현 활성·통합시험 완료로 승격하지 않음. | WINDOW 구현 보류 |
| <a id="scenario-sc13"></a>VSS 일회 Event·지속 경고·복구 경계 | [VSS_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-vss-event) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [VSS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-vss-status) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 만료/무효 입력은 새 음향 근거가 아님. 기존 지속 경고도 유효 조건까지만 처리; 오디오 정지를 위험 해제로 바꾸지 않음. WINDOW 유래 경고 입력은 DEFERRED. [상위 설계의 VSS 복구 정책](../network_design_draft_v0.1.md): 이전 pending/interrupted 일회 음향은 자동 재개하지 않고, 복구 후 새 유효 현재 경고로 재중재. 실제 HW/시간은 NOT_VERIFIED. | 연동 검증 전 |
| <a id="scenario-sc14"></a>VSS 현재/최근 고장과 App 가용성 | [VSS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-vss-status) · [M_VSS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-vss-status) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | current fault·LAST_FAULT·link health·원 품질은 동치가 아님. 미확인 status를 READY/FULL/정상으로 합성하지 않음. 실제 fresh status와 기능 복구 근거를 확인하며 최근 이력은 유지 가능. 원 세대/품질의 축약 동등성은 미결. | 연동 검증 전 |
| <a id="scenario-sc15"></a>기존 상태·결과·경고 Query와 부분 응답·종결 | [M_QUERY](03_COMMUNICATION/20_UART_M.md#message-m-query) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_DIGITAL_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-digital-result) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_USER_SETTINGS](03_COMMUNICATION/20_UART_M.md#message-m-user-settings) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_DIGITAL_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-digital-status) · [M_BCM_DOOR_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-door-state) · [M_BCM_CLIMATE_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-climate-state) · [M_BCM_LIGHT_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-light-state) · [M_BCM_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-bcm-status) · [M_CIS_ENVIRONMENT](03_COMMUNICATION/20_UART_M.md#message-m-cis-environment) · [M_CIS_OCCUPANT](03_COMMUNICATION/20_UART_M.md#message-m-cis-occupant) · [M_CIS_REAR](03_COMMUNICATION/20_UART_M.md#message-m-cis-rear) · [M_CIS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-cis-status) · [M_VSS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-vss-status) · [M_WINDOW_STATE](03_COMMUNICATION/20_UART_M.md#message-m-window-state) · [M_WINDOW_FAULT](03_COMMUNICATION/20_UART_M.md#message-m-window-fault) · [M_QUERY_END](03_COMMUNICATION/20_UART_M.md#message-m-query-end) | 원 데이터 부재, 존재하나 INVALID, 송신 실패/부분 누락을 구별. local queue OK는 actual TX count/완료가 아님. 원 CAN↔C2 연결 미증명 항목·부분 설정은 영향 projection HOLD. 재연결은 새 현재 문맥으로 보관 값/결과를 재조회; Query로 차량 동작을 재실행하거나 잃은 ECU 결과를 만들어내지 않음. | 연동 검증 전 |
| <a id="scenario-sc16"></a>경고 읽음 및 사용자 이탈 후 열린 도어 | [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_WARNING_ACK](03_COMMUNICATION/20_UART_M.md#message-m-warning-ack) · [BCM_DOOR_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) | App 링크 상실을 사용자 이탈로 해석하지 않음. 읽음은 위험 CLEAR/음향 정지 지시가 아님. 근거 없음/만료를 정상 해제로 만들지 않음. 새 유효 상태/현재 경고를 조회·표시. 기존 occurrence와 새로운 발생을 구별하며 stale ACK로 새 경고를 읽음 처리하지 않음. | 연동 검증 전 |
| <a id="scenario-sc17"></a>부팅·세대 변경·단절/재접속·결과 미확인 | [M_CONTEXT](03_COMMUNICATION/20_UART_M.md#message-m-context) · [M_DOMAIN_ALIVE](03_COMMUNICATION/20_UART_M.md#message-m-domain-alive) · [M_QUERY](03_COMMUNICATION/20_UART_M.md#message-m-query) · [M_QUERY_END](03_COMMUNICATION/20_UART_M.md#message-m-query-end) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [DOMAIN_PERMISSION](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [BCM_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [VSS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-vss-status) | 이전 세대 pending/cache/결과 상관을 새 정상 근거로 사용하지 않음. BLE/UART loss로 새 요청/자동해제 제한; 없어지는 결과를 FAILED/DONE으로 합성하지 않음. CAN bus-off 후 현재 상태 기반 전달 회복, 과거 명령/일회 이벤트 재생 없음. 모든 노드 일괄 reset이나 손실 결과 복구 보장은 문서 근거 없음. 실제 관측/무효화 binding 미검증. | 연동 검증 전 |
| <a id="scenario-sc18"></a>기존 전송 경계의 거부·부분 수신·용량·후발 실패 | [전체 기존 메시지 경계](05_MESSAGE_INDEX.md) · [접수·실패](04_COMMON/20_API_USAGE_AND_FAILURE.md) · [CAN](03_COMMUNICATION/10_CAN_FD.md) · [UART-M](03_COMMUNICATION/20_UART_M.md) · [UART-C](03_COMMUNICATION/30_UART_C.md) | 부분/CRC·길이 불일치 프레임은 정상 수신/원 최신성을 갱신하지 않음. 용량 부족·순서/중복 오류를 성공으로 꾸미지 않고 discrete 결과를 조용히 덮어쓰지 않음. BLE 병목·부분 종결의 실제 바인딩은 미확인. 다음 정상 프레임으로 수신 경계를 회복하되 원 ID/세대/AGE를 재생성하지 않음. 기존 문서가 허용한 원 수명 내 재제출 경계만 기록; 새로운 retry 횟수/주기/메시지 없음. | 연동 검증 전 |

<a id="test-plans"></a>
<a id="delta-acceptance-cases"></a>
## 9. 경계 입력과 처리 조건

아래는 구현 시 확인할 입력·결과 조건이다. 권장 프로필의 적용 조건과 실제 연동 검증은 별도로 충족해야 한다.

| 입력·경계 | 처리 조건 |
|---|---|
| <a id="plan-ncp-t01"></a>UART-C 0x60/61/62 byte·LEN·CRC·TYPE/방향 | 19/16/15+overhead10, 예약값·오류 프레임 거부 |
| <a id="plan-ncp-t02"></a>UART 스트림 분할·연결·CRC 오류 후 재동기화 | 형식이 잘못된 프레임으로 센서 최신성을 갱신하지 않음 |
| <a id="plan-ncp-t03"></a>Pi의 새 비전 결과와 STATUS 단독 수신 구분 | 실제 새 원본 결과에만 VisionSeq/AGE 갱신을 적용 |
| <a id="plan-ncp-t04"></a>Pi BOOT A->B->delayed A | A rejected once retired; producer collision acknowledged |
| <a id="plan-ncp-t05"></a>무작위 BOOT 충돌 또는 지속 근거가 없는 재시작 | 오래된 자료의 재생을 절대적으로 차단한다고 보장하지 않음 |
| <a id="plan-ncp-t06"></a>Pi VALID/INVALID per field | separate quality preserved; no UNKNOWN->0 |
| <a id="plan-ncp-t07"></a>Pi의 무효 관측 후 통신 단절 | current COMMUNICATION_FAULT/STALE, origin Pi reason private |
| <a id="plan-ncp-t08"></a>Pi STATUS 고장이 일부 기능에만 영향 | only indicated affected function invalidated |
| <a id="plan-ncp-t09"></a>Pi Permission 원본 AGE 499/500/501 | validity at <500 only if same accepted source |
| <a id="plan-ncp-t10"></a>CAN 50ms 반복 송신과 200ms 새 평가 구분 | no fake seq/age0 on periodic transmit |
| <a id="plan-ncp-t11"></a>UART 새 입력 중계와 500ms 주기 송신 구분 | immediate on actual new valid eval, repeated retains revision |
| <a id="plan-ncp-t12"></a>DOMAIN/CIS 재시작 시 Permission | old ALLOW cannot be reanimated by stale Frame |
| <a id="plan-ncp-t13"></a>Power 원본은 유효하지만 Operation 원본은 UNKNOWN | power not fabricated/blanket DENIED; shared AGE conservative |
| <a id="plan-ncp-t14"></a>온도·습도 원본 AGE 2499/2500/2501 | 2500 and beyond unusable at Domain consumer |
| <a id="plan-ncp-t15"></a>sensor local stale 4999/5000/5001 | local expiry boundary explicit, separate Domain2500 |
| <a id="plan-ncp-t16"></a>CIS 환경 BOOT 변경과 이전 프레임의 지연 도착 | VSM previous good invalidates; accept new Boot seq reset |
| <a id="plan-ncp-t17"></a>Rear ALLOW DENIED UNKNOWN NO_DATA | INACTIVE with distinct reasons, no NO_OBJECT on fault |
| <a id="plan-ncp-t18"></a>CAN Rear raw mm / C2 mm and 65535 cases | distance unchanged; no absent/CLEAR confusion |
| <a id="plan-ncp-t19"></a>CIS_STATUS Health vs fault history | link evidence separated from genuine sensor fault |
| <a id="plan-ncp-t20"></a>로컬 Publish 성공과 실제 전송·큐 실패 구분 | bounded copy not DONE/peer RX; error preserved |
| <a id="plan-ncp-t21"></a>S32K144의 Pi UART·센서 처리 동시 수행 시 차단 여부 | no loss/permission deadline violations measured |
| <a id="plan-ncp-t22"></a>S32K144↔344 CAN의 세대·Power 전달 | real FDF/BRS/DLC/Boot/Codec and C2 observer |
| ALLOW/OK, DENY/OK, UNKNOWN/NO_DATA, UNKNOWN/OK | 확정 DENY와 UNKNOWN 구분. UNKNOWN+OK 무손실 필요 시 기존 C2 한계·원인별 품질 투영의 DOMAIN Adapter 수용 대기. Core 실패 철회는 담당자 구현 완료 보고 |
| quality OK / meta.quality INVALID | OK를 골라 허용 생성 금지; 기존 충돌 후보대로 제한 |
| 성공17 ALLOW/OK 뒤200ms 평가 실패 | Core ALLOW 철회 완료 보고·원 성공17 유지·내부 stateRevision 증가. 품질 투영 수용 및 Network 전달 검증 대기 |
| 마지막 성공 반복 뒤 실패 / 성공 이력 없음 | 실패 시각으로 AGE0·성공 순번 생성 금지; 미확인 시간 표현 결정 대기 |
| 평가가 정상 수행되어 DENY 결정 | false/OK는 확정 불허이며 평가 실패가 아님 |
| 동일 원 평가 반복, 품질 변경, 새 성공 평가 | 원 평가/합성/프레임 번호의 생성 의미 분리, 반복 TX로 원 평가 재발급 금지 |
| A900 대기 중 B0 수용 | Network의 이전 대기 입력 차단→동기 Private 전환→DOMAIN의 이전 쓰기 배제·무효화 완료 확인→B의 원본 순번 0 전달. DOMAIN Core 순차 Epoch 지원 확인, Network 실제 Private 호출·Receipt/Query 연동 확인 대기, 리매핑 금지 |
| A 콜백 실행 중 B 수용 | 이전 BOOT(A)의 쓰기가 무효화 이후 끼어들지 않음; 임의 취소·owner 교착 없이 직렬 경계 확인 |
| 콜백 반환 후 A 비동기 쓰기 대기 | Network 콜백 반환만으로 완료 판정 금지; DOMAIN이 동기 private 완료 뒤 구 쓰기의 재저장을 막는 경계 확인 |
| Private 무효화 실패·완료 미확인 | 새 환경 전달 차단·오류 보고, 자동 재시도 없음. 과거 OK가 이미 지워졌다고 판정 금지 |
| Core commit 뒤 optional Observer 실패 | 저장 실패와 구분·중복 commit 금지; 실제 완료 근거 확인 |
| B 저장 뒤 A 지연 프레임·대기 콜백 | 기억된 종료 BOOT 범위에서 A 재반영 차단 |
| 같은 BOOT의 일반 NO_DATA | 재부팅 추론·순번 리셋 금지; 단절 STALE/NO_DATA 선택 미결 |
| B 관측이 저장 대기 중 AGE2500 도달 | 온도·습도 원본 사용 기한 2500ms 적용, 정상 복원 금지 |
| BOOT 재사용·Network/DOMAIN 단독 재시작·기억 상실 | 무조건 최신/영구 차단·이전 완료 승계 보장 금지; 양쪽 초기 정합 전 환경 보류, 자동 복구 없음 |
