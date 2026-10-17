# M09 Network 관측 자료

형식 검증, 실제 전송 결과와 소비 결과를 구별해 통신 사실을 남긴다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 입력 | M04/M05/M06/M07/M08의 실제 관측 |
| 출력 | Network 진단 자료와 기능 담당자가 해석할 원 사실 |
| 보유 상태와 수명 | peer/메시지별 정상 수신, 거부·overflow·Driver 실패·인계 결과의 원 관측(ST12). 차량 Fault나 기능 가용성을 생성하지 않는다. |

F24의 후방 관측 책임을 기존 처리 지점에 연결한다. 관측만을 위한 별도 공개 함수·Queue를 만들지 않는다. F25의 현재 Health 조회는 [실패·Health 정본](../functions/NETWORK_FAILURE_HEALTH.md)과 아래 링크별 연결을 따른다.

## 정상 처리

CIS_REAR의 정상 프레임과 C2 소비 성공을 별도로 기록한다. 다른 ECU 메시지는 CIS 타이머를 갱신하지 않는다. 같은 오래된 관측이 반복돼도 source freshness는 새로 시작하지 않는다.

## 실패와 복구

잘못된 CRC·길이·enum은 정상 수신으로 승격하지 않는다. Driver 접수를 최종 TX로, C2 호출을 차량 기능 완료로 바꾸지 않는다. Health Getter 연결은 실제 바인딩이 필요한 별도 범위다.

동일 사건의 관측은 소유자 문맥에서 한 번만 반영한다. 정상 수신·접수·소비·장치 결과를 한 counter로 합치지 않으며 실제 count 단위와 포화·초기화 조건을 확인한다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetRearResult NetRear_Admit(const NetCisRear *message, const NetCanRxMeta *meta, uint32_t nowMs);` | 새 내부 함수. F24 수신 관측 부분: 형식상 유효 프레임, 중복·순서 제한, 실제 접수를 구분한다. | [NetRear_Admit](../functions/CIS_REAR_RX_ADMISSION.md#admit) |
| `NetRearResult NetRear_Deliver(uint32_t nowMs);` | 새 내부 함수. F24 인계 관측 부분: 원 C2 반환을 보존하며 로컬 호출 성공과 기능 완료를 분리한다. | [NetRear_Deliver](../functions/DOMAIN_CIS_REAR_INGRESS.md#adapter) |
| `NetRearResult NetRear_ObservePort(NetRearPortEvent event, const NetCanSource *submitted, bool referenceReleased, uint32_t nowMs);` | 새 내부 함수. F24 장치 관측 부분: 실제 전송·오류·해제·복구 사실만 원 identity와 연결한다. | [NetRear_ObservePort](../functions/CAN_PORT_RECOVERY.md#observe-port) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L05_TRANSPORT_PORT.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F24/F25

u32 원 관측 counter를 포화 보존하고 기능 정책과 분리한다. 상대 reboot·정상수신으로 counter를 초기화하지 않으며 actual public Health 연결·동기화가 없으면 바인딩을 보류한다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#recovery-health) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `NetCanResult NetCan_ObservePort(NetCanTxRecord *r, const NetCanPortEvent *e);` | 내부 제안; 제출수명 식별로 admission/outcome/release를 원자 관측 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#port) |
| `DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot(NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);` | DOMAIN 본체; Network raw 관측과 조회 경계 연결은 지원표 확인 필요 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#recovery-health) |
| `DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);` | DOMAIN 본체; Network가 원 identity에 귀속하여 필수 sink 호출 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#failure) |

## UART-M 연결 — F24/F25·ST12

ST12는 malformed header/CRC/조립 timeout/byte loss·정상 frame·의미 거부·중복·실제 소비·TX outcome/참조 해제를 각각 원 관측으로 보존한다. counter는 u32 포화이고 상대 재부팅/정상 수신으로 초기화하지 않는다. 다른 메시지로 원 source freshness를 갱신하거나 Window2의 정상 liveness를 만들지 않는다. 기존 DomainIf_GetNetworkHealthSnapshot의 성공 OUT만 소비하며 실제 Network raw→DOMAIN Health 공급·동기화가 미검증이면 바인딩을 HOLD로 둔다.

[관측/반환 정본](../functions/UART_M_TX_RX_CONTROL.md) · [parser counter](../data/UART_FRAME_DATA.md#type-NetUartMParser) · [상위 L05](../layers/L05_TRANSPORT_PORT.md) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)

<a id="uart-c"></a>
## UART-C 추가 연결

F24~25/ST12는 UART-C raw 통신 관측과 기능 품질을 분리한다. UART에 CAN bus-off를 만들지 않고 peer 재시작으로 counter를 0으로 만들지 않는다. [함수/흐름](../functions/UART_C_TX_RX_CONTROL.md#recovery) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)

## DOMAIN 로컬 실패·Health 연결 — F24/F25

[Network 실패·Health](../functions/NETWORK_FAILURE_HEALTH.md)는 동일 로컬 시도의 실제 실패를 원 command/request/occurrence identity에 연결하고, 기존 kind/reason으로 표현할 수 있는 경우에만 DOMAIN sink로 전달한다. 동기 거부를 후발 실패로 중복 집계하거나 Query 실패를 차량 결과로 바꾸지 않는다.

Network는 실제 계측 입력을 보존하고 DOMAIN 소유 `DomainIf_GetNetworkHealthSnapshot`의 공급·동기화 경계를 확인한다. 관측되지 않은 Peer 생존·차량 기능을 정상으로 채우지 않는다. [공통 자료](../data/NETWORK_SERVICE_DATA.md), [전송 수명](../functions/NETWORK_SERVICE_LIFETIME.md), [DOMAIN 경계 개요](../07_DOMAIN_ADAPTER_SERVICE.md)에서 인자·관측의 수명을 확인한다.

## PG6-SW01 — 환경 제한 사건과 인계 결과

F24/F25는 기존 메시지별 관측에서 [NetCisEnvironmentLocalEvent](../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)를 만든다. 원 DTO/meta/각 채널 품질·AGE 기준/recordRevision을 고정하고 동일 clock의 실제 경과로 온도·습도 각각2500ms 만료 mask를 계산한다. 수신중단은 CIS_ENVIRONMENT의 실제 마지막 유효 수신과 채택된 중단 정책, Boot 변경은 M01 수용 전이를 사용한다. 새 frame이 없는 이 사건을 정상 수신 counter·TX/seq로 만들어 넣지 않는다.

M03/F08 직렬 문맥의 `NetCan_InvalidateCisEnvironment(const NetCisEnvironmentLocalEvent *event)`는 일반 ORIGIN_EXPIRED/RX_STOPPED에서 [기존 환경 C2](../functions/can/CIS_ENVIRONMENT.md#local-invalidation)를 호출하고 BOOT_CHANGED는 단일 동기 DOMAIN private 목표로 분리한다. BOOT용 Rx NO_DATA 중복 호출은 하지 않는다. 같은 recordRevision/eventRevision에 Network 자기 대기/콜백·미시작 환경 Query 차단, 고정 전환 요청·시도, 실제 private 원 응답, 현재 내용/수용 BOOT/적용 Epoch/명확한 완료 대조, DOMAIN 후속 commit 봉쇄·VSM/Meta·순서 문맥/구 Query 무효화, 이후 B Rx/Getter 연결을 구분해 남긴다. CALL_IN_PROGRESS는 기존 시도 기록이며 진행 중·불명·실패를 완료된 중복으로 만들지 않는다. 실제 선언/ABI 미제공·다른 Epoch EPOCH_ORDER_UNAVAILABLE는 HOLD 근거로 기록한다. 구 객체 잔존만으로 실패라 하지 않지만 확실한 후속 commit 차단·무효화/완료 증거가 없으면 B를 열지 않는다. 원 rc·내용·미확인을 보존하고 새 반환 enum·완료 bool을 만들지 않는다. 일반 만료/중단의 공개 Rx·Core/Observer 기록은 유지하며 명확한 완료 뒤 Observer 오류로 재commit하지 않는다. [r1 조건부 저장 순서](../functions/can/CIS_ENVIRONMENT.md#environment-storage-order)를 따르고 실제 선언·연속 전환/Binding 미검증의 [BG-R05-BOOT](../functions/can/CIS_ENVIRONMENT.md#boot-binding-gap)는 유지한다.

CIS_OCCUPANT는 실제 제한 사건과 같은 원 Pi 관측을 [전용 증거](../data/CAN_NODE_DATA.md#type-NetCanOccupantRestrictionEvidence)에 연결하며 raw 이유 한 바이트만으로 참인 증거를 만들지 않는다. R08은 기존 NOT_STARTED의 최종 Driver 제출 실패/송신 전 만료/문맥 무효화만 필수 sink로 보낸다. terminal·참조해제·Core 반영은 별도 관측이고 busy/무callback/미분류 in-flight는 기존 세 사유로 치환하지 않는다.
