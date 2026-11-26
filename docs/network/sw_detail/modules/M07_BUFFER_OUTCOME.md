# M07 전송 버퍼·결과 관측

Network 소유 저장소와 장치가 빌려 쓰는 구간을 분리한다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 입력 | 완성 TX 프레임, Driver RX/TX/error 관측 |
| 출력 | 상위에 수명이 보장된 자료와 귀속 가능한 결과 |
| 보유 상태와 수명 | 불변 TX 프레임과 제출 Boot/TX·장치 인수 상태(ST09), RX 프레임과 최초 수신 시각의 사본(ST10). 용량 수치는 실제 시스템에서 확인한다. |

F19의 송신 인수, F20의 RX 저장·소비, F21의 전송 결과 관측을 후방 경로에 적용한다. 하나의 함수가 M04 또는 M08 경계를 함께 설명하더라도 버퍼 수명 책임은 이 모듈에 둔다.

## 정상 처리

TX는 접수 전후를 구분하고 accepted 이후 프레임을 고정한다. RX는 데이터와 수신 시각을 함께 인수하며 소비 완료 전 저장소를 회수하지 않는다. terminal 관측은 제출 Boot/TX와 실제 Driver 수명 근거에 연결한다.

참조 종료가 확인되어 byte 저장소를 반환해도 결과 미확인 identity는 별도로 보존한다. 그 관측에 대한 M04의 제출 예약도 결과 확인 전에는 해제하지 않으며, 남는 버퍼를 같은 관측의 재제출 허가로 해석하지 않는다.

## 실패와 복구

overflow 때 읽지 못한 프레임을 정상으로 세지 않는다. 늦은 이전 기동 callback으로 새 프레임을 해제하지 않는다. 취소·참조 종료가 확인되지 않으면 저장소를 재사용하지 않는다.

버퍼 인수·반환과 제출 결과 기록은 같은 소유자 문맥에서 직렬화한다. 장치의 비동기 참조는 소유자 Task의 실행 여부와 별개로 계속될 수 있으므로 실제 참조 종료 근거가 있어야 주소를 재사용한다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetRearResult NetRear_Dispatch(uint32_t nowMs);` | 새 내부 함수. F19 역할: 장치가 사용할 안정 주소의 TX 프레임과 제출 identity를 확보한다. | [NetRear_Dispatch](../functions/CIS_REAR_TX_DISPATCH.md#dispatch) |
| `NetRearResult NetRear_ReceiveFrame(const NetRearFrame *frame, uint32_t nowMs);` | 새 내부 함수. F20 역할: Driver view에서 Network 소유의 RX 사본을 확보하고 소비 단계로 연결한다. | [NetRear_ReceiveFrame](../functions/CIS_REAR_RX_ADMISSION.md#receive-frame) |
| `NetRearResult NetRear_ObservePort(NetRearPortEvent event, const NetCanSource *submitted, bool referenceReleased, uint32_t nowMs);` | 새 내부 함수. F21 역할: 최종 전송 결과와 참조 해제를 나누어 반영하고 저장소를 한 번만 반환한다. | [NetRear_ObservePort](../functions/CAN_PORT_RECOVERY.md#observe-port) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L05_TRANSPORT_PORT.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F19/F20/F21

의미 사본/encoded frame/raw 수신 저장소의 수명을 분리한다. 참조 해제와 최종 결과는 별개이며 released 뒤에도 미확정 원 identity를 보존한다. 실제 Queue 용량/동기화는 미확정이다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#port) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `NetCanResult NetCan_ClassifyRx(const NetCanRxComparison *c, NetCanRxAction *action);` | 내부 제안; 원 입력/결과 단계와 TX의 수용 분류, 원장 직접변경 없음 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#rx-order) |
| `NetCanResult NetCan_ObservePort(NetCanTxRecord *r, const NetCanPortEvent *e);` | 내부 제안; 제출수명 식별로 admission/outcome/release를 원자 관측 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#port) |

## UART-M 연결 — F19~F21

ST09/ST10에서 borrowed 자료·완성 Frame·원 수신시각을 안정 소유하고 Driver 참조 종료와 최종 outcome을 별도로 기록한다. byte 저장소 해제 뒤에도 미확정 identity를 보존한다. Query count는 실제 final TX에서만 갱신하며 release/accepted는 count가 아니다.

[TX/RX 수명](../functions/UART_M_TX_RX_CONTROL.md) · [Frame Data](../data/UART_FRAME_DATA.md) · [Query 종료](../functions/UART_M_QUERY_CONTROL.md#terminal) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)

<a id="uart-c"></a>
## UART-C 추가 연결

F19~21/ST09~10은 UART-C 전체 Frame/DTO/meta 사본과 별도 원 identity를 소유한다. release가 먼저 관측되어도 미확정 outcome의 귀속을 보존한다. [함수/흐름](../functions/UART_C_TX_RX_CONTROL.md#reclaim) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)

## DOMAIN Service의 저장소와 결과 — F19/F20/F21

[공통 수명](../functions/NETWORK_SERVICE_LIFETIME.md)에서 입력 사본, Driver 참조 중 Frame, 해제 뒤에도 필요한 원 결과 identity의 보관을 연결한다. release가 terminal보다 먼저 도착해도 물리 저장소 반환과 결과 확정을 합치지 않는다. 오래된 callback이 재사용된 새 버퍼를 회수하지 못하도록 같은 제출 수명을 대조한다.

[공통 자료](../data/NETWORK_SERVICE_DATA.md) · [후발 실패](../functions/NETWORK_FAILURE_HEALTH.md) · [DOMAIN 명령 흐름](../flows/DOMAIN_COMMAND_TO_NETWORK.md)
