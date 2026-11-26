# M05 CAN 필드 Codec

CAN22의 정해진 offset·폭·코드를 명시적으로 부호화하고 역변환한다. 아래 CIS_REAR는 대표 경로이며 나머지21종은 메시지별 정본을 따른다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 대표 입력 | NetCisRear 및 송신 Boot/TX 메타 / Driver 프레임·길이·flags |
| 대표 출력 | 20바이트 프레임 / 검증된 NetCisRear와 원 출처 |
| 보유 상태와 수명 | 영속 수신 순서·Manager 상태 없음. 호출 중 임시 출력만 소유한다. |

F14의 decode와 F15의 encode를 후방 대표 경로에 적용한다. 순수 필드 변환이며 전송 시도·수신 순서·기능 상태를 진전시키지 않는다.

## 정상 처리

송신 Boot/TX 메타와 의미 필드를 LE로 기록하고 padding을 0으로 만든다. decode는 프레임 길이를 확인한 후 각 필드를 검증하며 완성 전 출력 인계를 하지 않는다. 수치 및 enum 매핑은 Data 정본만 참조한다.

## 실패와 복구

DLC·실제 길이·padding·예약 enum이 잘못되면 부분 출력과 정상 callback이 없다. Driver CRC 오류 프레임을 복구하지 않는다. CAN 응용 CRC·버전 필드를 발명하지 않는다.

Codec는 호출별 임시 출력만 사용한다. 공유 버퍼의 수명은 M07이, 순서 수용 상태는 M04가 관리한다. 서로 독립된 입력·출력 객체를 사용하는 호출에 새 전역 순서 상태나 별도 Queue를 추가하지 않는다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetRearResult NetRear_Encode(const NetCisRear *message, const NetCanSource *source, NetRearFrame *out);` | 새 내부 함수. F15: Net 의미 필드와 송신 Boot/TX 메타를 고정 offset의 완전한 CAN 프레임으로 작성한다. | [NetRear_Encode](../functions/CIS_REAR_CODEC.md#encode) |
| `NetRearResult NetRear_Decode(const NetRearFrame *frame, NetCisRear *out, NetCanRxMeta *meta);` | 새 내부 함수. F14: 실제 프레임 조건과 필드를 확인하고 같은 수신 건의 Net 값·메타를 함께 반환한다. | [NetRear_Decode](../functions/CIS_REAR_CODEC.md#decode) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L04_FRAME_CODEC.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F14/F15

후방 전용 Codec는 보존하고 나머지21종에는 공통 프레임 검사와 메시지별 Encode/Decode 제안을 적용한다. 원 field offset·enum·품질을 검증하며 struct 복사로 Wire를 만들지 않는다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#age) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `NetCanResult NetCan_CheckFrame(const NetCanFrame *frame, NetCanMessageKind kind, NetCanNode receiver, NetCanProfile profile);` | 내부 제안; 실제 frame 경계 검증 | [함수 상세](../functions/CAN_COMMON_CODEC.md#check-frame) |
| `NetCanResult NetCan_EncodeBcmClimateCommand(const NetBcmClimateCommand *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_CLIMATE_COMMAND.md#codec) |
| `NetCanResult NetCan_DecodeBcmClimateCommand(const NetCanFrame *frame, NetCanProfile profile, NetBcmClimateCommand *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_CLIMATE_COMMAND.md#codec) |
| `NetCanResult NetCan_EncodeBcmClimateState(const NetBcmClimateState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_CLIMATE_STATE.md#codec) |
| `NetCanResult NetCan_DecodeBcmClimateState(const NetCanFrame *frame, NetCanProfile profile, NetBcmClimateState *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_CLIMATE_STATE.md#codec) |
| `NetCanResult NetCan_EncodeBcmDoorCommand(const NetBcmDoorCommand *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_DOOR_COMMAND.md#codec) |
| `NetCanResult NetCan_DecodeBcmDoorCommand(const NetCanFrame *frame, NetCanProfile profile, NetBcmDoorCommand *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_DOOR_COMMAND.md#codec) |
| `NetCanResult NetCan_EncodeBcmDoorState(const NetBcmDoorState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_DOOR_STATE.md#codec) |
| `NetCanResult NetCan_DecodeBcmDoorState(const NetCanFrame *frame, NetCanProfile profile, NetBcmDoorState *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_DOOR_STATE.md#codec) |
| `NetCanResult NetCan_EncodeBcmEvent(const NetBcmEvent *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_EVENT.md#codec) |
| `NetCanResult NetCan_DecodeBcmEvent(const NetCanFrame *frame, NetCanProfile profile, NetBcmEvent *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_EVENT.md#codec) |
| `NetCanResult NetCan_EncodeBcmLightCommand(const NetBcmLightCommand *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_LIGHT_COMMAND.md#codec) |
| `NetCanResult NetCan_DecodeBcmLightCommand(const NetCanFrame *frame, NetCanProfile profile, NetBcmLightCommand *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_LIGHT_COMMAND.md#codec) |
| `NetCanResult NetCan_EncodeBcmLightState(const NetBcmLightState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_LIGHT_STATE.md#codec) |
| `NetCanResult NetCan_DecodeBcmLightState(const NetCanFrame *frame, NetCanProfile profile, NetBcmLightState *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_LIGHT_STATE.md#codec) |
| `NetCanResult NetCan_EncodeBcmResult(const NetBcmResult *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_RESULT.md#codec) |
| `NetCanResult NetCan_DecodeBcmResult(const NetCanFrame *frame, NetCanProfile profile, NetBcmResult *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_RESULT.md#codec) |
| `NetCanResult NetCan_EncodeBcmStatus(const NetBcmStatus *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/BCM_STATUS.md#codec) |
| `NetCanResult NetCan_DecodeBcmStatus(const NetCanFrame *frame, NetCanProfile profile, NetBcmStatus *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/BCM_STATUS.md#codec) |
| `NetCanResult NetCan_EncodeCisEnvironment(const NetCisEnvironment *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/CIS_ENVIRONMENT.md#codec) |
| `NetCanResult NetCan_DecodeCisEnvironment(const NetCanFrame *frame, NetCanProfile profile, NetCisEnvironment *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/CIS_ENVIRONMENT.md#codec) |
| `NetCanResult NetCan_EncodeCisOccupant(const NetCisOccupant *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/CIS_OCCUPANT.md#codec) |
| `NetCanResult NetCan_DecodeCisOccupant(const NetCanFrame *frame, NetCanProfile profile, NetCisOccupant *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/CIS_OCCUPANT.md#codec) |
| `NetCanResult NetCan_EncodeCisStatus(const NetCisStatus *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/CIS_STATUS.md#codec) |
| `NetCanResult NetCan_DecodeCisStatus(const NetCanFrame *frame, NetCanProfile profile, NetCisStatus *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/CIS_STATUS.md#codec) |
| `NetCanResult NetCan_EncodeDomainPermission(const NetDomainPermission *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#codec) |
| `NetCanResult NetCan_DecodeDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile, NetDomainPermission *message, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#codec) |
| `NetCanResult NetCan_EncodeVssEvent(const NetVssEvent *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/VSS_EVENT.md#codec) |
| `NetCanResult NetCan_DecodeVssEvent(const NetCanFrame *frame, NetCanProfile profile, NetVssEvent *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/VSS_EVENT.md#codec) |
| `NetCanResult NetCan_EncodeVssStatus(const NetVssStatus *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/VSS_STATUS.md#codec) |
| `NetCanResult NetCan_DecodeVssStatus(const NetCanFrame *frame, NetCanProfile profile, NetVssStatus *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/VSS_STATUS.md#codec) |
| `NetCanResult NetCan_EncodeVssWarningState(const NetVssWarningState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/VSS_WARNING_STATE.md#codec) |
| `NetCanResult NetCan_DecodeVssWarningState(const NetCanFrame *frame, NetCanProfile profile, NetVssWarningState *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/VSS_WARNING_STATE.md#codec) |
| `NetCanResult NetCan_EncodeWindowCommand(const NetWindowCommand *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/WINDOW_COMMAND.md#codec) |
| `NetCanResult NetCan_DecodeWindowCommand(const NetCanFrame *frame, NetCanProfile profile, NetWindowCommand *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/WINDOW_COMMAND.md#codec) |
| `NetCanResult NetCan_EncodeWindowEvent(const NetWindowEvent *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/WINDOW_EVENT.md#codec) |
| `NetCanResult NetCan_DecodeWindowEvent(const NetCanFrame *frame, NetCanProfile profile, NetWindowEvent *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/WINDOW_EVENT.md#codec) |
| `NetCanResult NetCan_EncodeWindowFault(const NetWindowFault *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/WINDOW_FAULT.md#codec) |
| `NetCanResult NetCan_DecodeWindowFault(const NetCanFrame *frame, NetCanProfile profile, NetWindowFault *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/WINDOW_FAULT.md#codec) |
| `NetCanResult NetCan_EncodeWindowResult(const NetWindowResult *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/WINDOW_RESULT.md#codec) |
| `NetCanResult NetCan_DecodeWindowResult(const NetCanFrame *frame, NetCanProfile profile, NetWindowResult *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/WINDOW_RESULT.md#codec) |
| `NetCanResult NetCan_EncodeWindowState(const NetWindowState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *out);` | 내부 제안; typed→고정필드 인코딩 | [함수 상세](../functions/can/WINDOW_STATE.md#codec) |
| `NetCanResult NetCan_DecodeWindowState(const NetCanFrame *frame, NetCanProfile profile, NetWindowState *out, NetCanRxMeta *meta);` | 내부 제안; 실제 frame→typed/meta 역변환 | [함수 상세](../functions/can/WINDOW_STATE.md#codec) |

## UART-M 연결 — F14/F15 보존

CAN22의 원형·필드·패딩 정본을 유지한다. UART-M의20B DOWN_CONTEXT 뒤에는 동일 원 CAN used 필드만 투영하고 CAN DLC padding을 복사하지 않는다. UART frame·CRC·stream은 M06 책임이다.

[M06 UART](M06_UART_CODEC.md) · [CAN 투영 필드](../data/UART_M_PROJECTION_FIELDS.md) · [원본 흐름](../flows/UART_M_CAN_PROJECTION.md) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)
