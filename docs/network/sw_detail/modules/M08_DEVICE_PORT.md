# M08 장치 Port 경계

Driver의 관측과 Network의 저장·소비 문맥을 연결한다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 입력 | Network 제출 및 HW RX/TX/error interrupt 관측 |
| 출력 | M07 인수 결과, 오류·기동·참조 종료 근거 |
| 보유 상태와 수명 | 실제 장치 준비·동작 기동·중단/복구 근거(ST11). 원 센서 관측 세대나 DOMAIN의 로컬 Boot를 대신 생성하지 않는다. |

F22의 Driver 경계와 F23의 기동·정지·복구를 후방 경로에 적용한다. 아래 함수는 장치 경계와 연결되는 Network 절차이며 실제 HAL 함수나 IRQ callback 선언이 아니다.

## 정상 처리

Driver 어댑터는 flags·실제 획득 길이·최초 수신 시각을 전달한다. ISR은 최소 복사/알림 경계까지만 담당하고 Codec와 C2 API는 소비 문맥에서 실행한다.

## 실패와 복구

Bus-off·CRC 오류·중단은 실제 관측으로만 보고한다. 자동 복구 지연이나 재시도 횟수를 고정하지 않는다. 재기동 뒤 기존 DMA/Driver 참조 종료 근거 없이 송신 저장소를 덮지 않는다.

ISR이 제공하는 최소 관측과 소비 문맥의 처리를 분리하고 장치별 동기화로 입력 수명을 보장한다. Driver callback의 제출 Boot/TX 대응·실제 Driver 수명 근거·중단 후 참조 종료 조건은 실제 장치 계약에서 확인한다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetRearResult NetRear_Dispatch(uint32_t nowMs);` | 새 내부 함수. F22 송신 접점: Network 프레임을 실제 장치 제출 경계에 연결한다. 실제 HAL 원형은 별도다. | [NetRear_Dispatch](../functions/CIS_REAR_TX_DISPATCH.md#dispatch) |
| `NetRearResult NetRear_ReceiveFrame(const NetRearFrame *frame, uint32_t nowMs);` | 새 내부 함수. F22 수신 접점: 장치가 보장하는 view 수명·flags·길이·수신시각을 M07 소비 경계로 연결한다. | [NetRear_ReceiveFrame](../functions/CIS_REAR_RX_ADMISSION.md#receive-frame) |
| `NetRearResult NetRear_ObservePort(NetRearPortEvent event, const NetCanSource *submitted, bool referenceReleased, uint32_t nowMs);` | 새 내부 함수. F22/F23: 실제 장치 통지를 전달하고 기동·참조 수명·복구 조건을 확인한다. | [NetRear_ObservePort](../functions/CAN_PORT_RECOVERY.md#observe-port) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L05_TRANSPORT_PORT.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F22/F23

실제 Driver frame·수신 시각·accepted/terminal/release를 전달한다. SDK API와 MB/FIFO/핀/clock 설정을 발명하지 않고 오래된 callback과 현재 항목을 로컬 수명 식별로 구분한다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#recovery-health) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `NetCanResult NetCan_ObservePort(NetCanTxRecord *r, const NetCanPortEvent *e);` | 내부 제안; 제출수명 식별로 admission/outcome/release를 원자 관측 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#port) |

## UART-M 연결 — F22/F23

ST11의 실제 장치/연결 세대와 callback 제출 수명을 확인한다. byte timestamp·timerCheckPermitted·overflow/reset은 실제 Driver 근거가 필요하며 Worker 현재시각으로 원 수신시각을 대체하지 않는다. reset은 Session/Boot 발급이나 Driver 참조 해제 증거가 아니다.

[Stream/Reset](../functions/UART_FRAME_STREAM.md) · [Driver 관측](../functions/UART_M_TX_RX_CONTROL.md) · [입력 자료](../data/UART_FRAME_DATA.md) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)

<a id="uart-c"></a>
## UART-C 추가 연결

F22~23/ST11은 Pi/CIS의 실제 byte 수신 시각·최종 TX·참조 해제·과거 callback의 세대 근거를 요구한다. 구체 SDK/DMA/FIFO/Task를 발명하지 않는다. [함수/흐름](../functions/UART_C_TX_RX_CONTROL.md#port-observe) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)

## DOMAIN Service와 실제 Driver 연결 — F22/F23

[공통 Service 수명](../functions/NETWORK_SERVICE_LIFETIME.md)은 실제 Driver의 접수·최종 결과·참조 해제와 Port 재기동을 구분하는 입력 조건을 설명한다. Task/Superloop 및 IRQ 처리의 구체 구현은 장치 Owner가 바인딩하며 이 문서가 SDK 함수·취소 성공·고정 timeout을 새로 확정하지 않는다.

[등록·준비 경계](../functions/DOMAIN_REGISTRATION.md) · [후발 관측](../functions/NETWORK_FAILURE_HEALTH.md) · [공통 자료](../data/NETWORK_SERVICE_DATA.md)
