# M04 링크별 메시지 서비스

CAN·UART-M·UART-C 자료의 안정 접수, 전송 선택, 수신 기동·순서를 관리한다. 아래 후방 자료는 대표 경로이며 링크별 차이는 각 정본을 따른다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 대표 입력 | 기능의 후방 관측, Codec의 완성 수신 값, Driver 결과 |
| 출력 | Codec 입력, 장치 제출, DOMAIN 인계 후보 |
| 보유 상태와 수명 | 미전송 의미 자료(ST05), 송신 선택/기동·순번(ST06), peer 수신 순서와 인계 여부(ST07). 한 자료의 복사와 상태 반영은 직렬 처리한다. |

F11의 안정 접수, F12의 송신 선택, F13의 수신 순서 수용을 후방 경로에 적용한다. 프레임 byte 저장소는 M07, 필드 변환은 M05 책임이다.

## 정상 처리

미전송 현재값은 최신의 검증된 관측으로 교체할 수 있지만 전송 중 프레임은 바꾸지 않는다. 송신 선택 때 AGE를 계산하고 TX_SEQUENCE와 UPDATE_SEQUENCE를 분리한다. 수신은 형식 검증 후 기동·순서를 판단하고 저장소 인수 성공 시 인계 상태를 반영한다.

제출 결과가 불명확한 원 관측은 해당 시도에 예약한다. 새 프레임 공간이 있어도 같은 관측을 다시 선택하지 않으며, byte 저장소 해제만으로 예약을 풀지 않는다. 기존 시도의 결과와 참조 상태가 확인된 뒤에도 원 AGE·기동·순서를 다시 평가한다.

## 실패와 복구

소비자 미준비와 Queue 부족이 발생했는데 순번만 완료 처리하면 재수신 기회를 잃는다. 실패 시 정상 프레임 관측과 기능 인계 여부를 별도로 유지한다. 재전송으로 관측을 젊게 만들지 않는다.

접수·송신 선택·수신 장부 갱신은 각 노드의 담당 문맥에서 직렬 처리한다. 실제 Lock·Queue 깊이·스케줄 방식은 노드 바인딩에서 확인하며 같은 상태를 Task와 ISR가 무보호로 수정하지 않는다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 기존 `CisNet_PublishRear`는 제공된 `PROPOSED_DESIGN/BINDING_OPEN` 경계다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetSubmitResult CisNet_PublishRear(const NetCisRear *message);` | 기존 설계 경계. F11 접수 역할: 완전한 의미 사본과 접수 기준점을 함께 반영한다. | [CisNet_PublishRear](../functions/CIS_REAR_TX_SUBMIT.md#publish-rear) |
| `NetRearResult NetRear_Dispatch(uint32_t nowMs);` | 새 내부 함수. F12 송신 선택: 대기·AGE·기동·순번을 확인하고 Codec 및 M07/M08로 넘긴다. | [NetRear_Dispatch](../functions/CIS_REAR_TX_DISPATCH.md#dispatch) |
| `NetRearResult NetRear_Admit(const NetCisRear *message, const NetCanRxMeta *meta, uint32_t nowMs);` | 새 내부 함수. F13 수신 수용: 원 관측과 프레임 순서를 판정하고 사본 확보 뒤 장부를 진전시킨다. | [NetRear_Admit](../functions/CIS_REAR_RX_ADMISSION.md#admit) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L03_MESSAGE_SERVICE.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F11/F12/F13

모든 CAN 메시지의 안정 접수·선택·순서 분류에 공통 정본을 적용한다. latest state와 개별 identity 항목을 구분한다. 동일 Boot·TX에서는 raw SOURCE_AGE를 포함한 동일성을 검사하므로 AGE만 달라도 duplicate가 아니다. 같은 UPDATE의 높은 TX에서는 일반적으로 값·validity·reason·status를 보존한다. 유일한 현재 제한 예외는 CIS_OCCUPANT의 [실제 제한 근거 검사](../functions/can/CIS_OCCUPANT.md#restriction-check)다. [NetCanRxComparison](../data/CAN_COMMON_DATA.md#type-NetCanRxComparison)의 messageKind는 모든 caller가 실제 메시지 종류로 채운다. occupantRestriction은 CIS_OCCUPANT 경로에서만 실제 제한 근거를 연결하고 다른 caller는 NULL을 유지한다. 일반 semanticExactlyEqual에서 품질을 제외하지 않는다. 저장 AGE와 새 AGE를 같은 소비 시점으로 각각 누적한 뒤 더 오래된 값을 유지한다. 최신 TX·raw 사본·보수적 AGE를 함께 반영하며 Permission 원 판단 한도를 재시작하지 않는다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#rx-order) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `NetCanResult NetCan_AccumulateAge(uint16_t sourceAgeMs, uint32_t elapsedMs, bool elapsedKnown, uint16_t *outAgeMs, bool *saturated);` | 내부 제안; 확인한 elapsed를 포화 누적 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#age) |
| `NetCanResult NetCan_ClassifyRx(const NetCanRxComparison *c, NetCanRxAction *action);` | 내부 제안; 원 입력/결과 단계와 TX의 수용 분류, 원장 직접변경 없음 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#rx-order) |

## UART-M 연결 — F11~F13

현재 의미 사본·개별 요청/결과/Query identity·Driver 제출 예약을 보존한다. 원 AGE와 LINK_SEQUENCE를 분리하고 문맥·순서·시간 근거를 확인한 뒤 소비한다. 중복·wrap·구 Session을 최신 관측으로 발급하지 않는다.

[TX/RX 제어](../functions/UART_M_TX_RX_CONTROL.md) · [Router](../functions/UART_M_ROUTER.md) · [M02 종료](../functions/UART_M_QUERY_CONTROL.md#terminal) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)

<a id="uart-c"></a>
## UART-C 추가 연결

F11~13/ST05~07은 Pi/CIS 원 평가와 LINK_SEQUENCE를 분리하고 원 AGE에 미반영 대기만 한 번 누적한다. F13은 분류 당시 안정 comparison·raw·typed/meta·AGE를 유지한다. 같은 Link/raw의 정확한 중복은 정상 감시/callback을 갱신하지 않고 수용된 새 Link의 같은 Vision 관측은 원 번호·원 시각을 유지하며 해당 Result/Status 감시와 기존 callback에 연결한다. 원자 사본·consumer 연결 미준비이면 장부 일부만 게시하지 않는다. void 반환은 외부 저장 ACK가 아니다. Pi Permission callback 규칙은 별도 Flow를 유지한다. 수용·순서·원 판단 한도의 근거가 없으면 영향을 받는 소비를 제한한다. [함수/흐름](../functions/UART_C_TX_RX_CONTROL.md) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)

## DOMAIN 제출의 공통 수명 — F11/F12/F13

[공통 Service 수명](../functions/NETWORK_SERVICE_LIFETIME.md)은 TxPort의 안정 사본·대기 공간 확보, 송신 선택, 원 관측·AGE와 후발 결과를 연결한다. 명령·결과·사건을 최신 상태처럼 덮어쓰지 않으며 참조 해제만으로 미확정 시도의 재제출을 허용하지 않는다. 기존 CAN/UART별 형식·순서 계약은 각 정본을 재사용한다.

[Service Data](../data/NETWORK_SERVICE_DATA.md) · [TxPort](../functions/DOMAIN_TX_PORT.md) · [DOMAIN 명령 흐름](../flows/DOMAIN_COMMAND_TO_NETWORK.md)
