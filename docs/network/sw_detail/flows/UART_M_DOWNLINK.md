# UART-M 하향 — DOMAIN 보고에서 ESP32 소비 경계까지

[Network 개요](../01_NETWORK_OVERVIEW.md) → [UART-M 개요](../05_UART_M_OVERVIEW.md) → [L02 인계](../layers/L02_TYPED_HANDOFF.md) → [M03](../modules/M03_TYPED_HANDOFF.md) / [M04](../modules/M04_MESSAGE_SERVICE.md) → [M06](../modules/M06_UART_CODEC.md) → [Frame/Stream](../functions/UART_FRAME_STREAM.md) → [Router](../functions/UART_M_ROUTER.md).

하향19종은 DOMAIN 보고6종, 원 CAN 투영11종, private ALIVE/QUERY_END2종이다. 아래6종은 공개 Wrapper→Network TxPort 콜백 또는 Query의 성공 Getter→같은 private submit으로 모인다. 공개 본체/Manager는 DOMAIN, 콜백/Codec/사본·Driver 인계는 DOMAIN Network, ESP32 Core/Transport의 본체는 ESP32 Owner다. 원격 C 함수를 직접 호출하는 구조가 아니다.

| Wire / Rx | Function·Data | 생산자→소비자 |
|---|---|---|
| `WIRE-P02-M_RESULT` / `RX-P02-M_RESULT-ESP32` | [M_RESULT](../functions/uart_m/M_RESULT.md) · [필드](../data/UART_M_REPORT_FIELDS.md#m_result) | DomainIf_TxMobileRequestResult→txMobileRequestResult→DomainNetPriv_SubmitMobileRequestResult; 상대 Gateway_Interface_OnDomainMessage |
| `WIRE-P02-M_WARNING` / `RX-P02-M_WARNING-ESP32` | [M_WARNING](../functions/uart_m/M_WARNING.md) · [필드](../data/UART_M_REPORT_FIELDS.md#m_warning) | DomainIf_TxMobileWarning→txMobileWarning→DomainNetPriv_SubmitMobileWarning; 상대 Gateway_Interface_OnDomainMessage |
| `WIRE-P02-M_AVAILABILITY` / `RX-P02-M_AVAILABILITY-ESP32` | [M_AVAILABILITY](../functions/uart_m/M_AVAILABILITY.md) · [필드](../data/UART_M_REPORT_FIELDS.md#m_availability) | DomainIf_TxMobileFunctionAvailability→txMobileFunctionAvailability→DomainNetPriv_SubmitMobileFunctionAvailability; 상대 Gateway_Interface_OnDomainMessage |
| `WIRE-P02-M_DIGITAL_STATUS` / `RX-P02-M_DIGITAL_STATUS-ESP32` | [M_DIGITAL_STATUS](../functions/uart_m/M_DIGITAL_STATUS.md) · [필드](../data/UART_M_REPORT_FIELDS.md#m_digital_status) | DomainIf_TxMobileDigitalKeyState→txMobileDigitalKeyState→DomainNetPriv_SubmitMobileDigitalKeyState; 상대 Gateway_Interface_OnDomainMessage |
| `WIRE-P02-M_DIGITAL_RESULT` / `RX-P02-M_DIGITAL_RESULT-ESP32` | [M_DIGITAL_RESULT](../functions/uart_m/M_DIGITAL_RESULT.md) · [필드](../data/UART_M_REPORT_FIELDS.md#m_digital_result) | DomainIf_TxMobileDigitalKeyResult→txMobileDigitalKeyResult→DomainNetPriv_SubmitMobileDigitalKeyResult; 상대 Gateway_Interface_OnDomainMessage |
| `WIRE-P02-M_USER_SETTINGS` / `RX-P02-M_USER_SETTINGS-ESP32` | [M_USER_SETTINGS](../functions/uart_m/M_USER_SETTINGS.md) · [필드](../data/UART_M_REPORT_FIELDS.md#m_user_settings) | DomainIf_TxMobileUserSettings→txMobileUserSettings→DomainNetPriv_SubmitMobileUserSettings; 상대 Gateway_Interface_OnDomainMessage |

## 한 항목의 정상 조건부 흐름

1. DOMAIN 기능 Owner의 원 결과·경고·설정 평가 또는 성공 Getter 사본을 받는다. 실패 Getter OUT을 사용하지 않는다. 현재 recipient Session과 과거 REQUEST_SESSION을 분리한다. sourceGeneration·Boot·원 순번·AGE의 귀속을 검사한다.
2. 6개 Function의 C2 변환에서 표현 가능성을 확인한다. unsupported enum, simulated warning, NORMAL 색상/밝기 부재, 실제 unlock 전이 필드 손실은 아래 HOLD 분기로 처리한다. CF01 양단 선택 증거가 없으면 현재 실행은 정상 기능 경로로 올라가지 않는다.
3. 현재 recipient/DOMAIN Boot와 자발 QueryID0 또는 유효 원 QueryID를 함께 사본으로 고정한다. 대기 원 AGE·시간 기준점도 보존한다. 사본·실패 sink 준비가 끝나야 local OK다.
4. M04가 확인된 미반영 대기만 누적하고 M06 Encoder가 필드별 byte를 쓴다. Frame Builder가 정확 P/TYPE/VERSION·방향·CRC를 검사한다. immutable frame을 M07/M08에 인계한 뒤 accepted/outcome/released를 각각 추적한다.
5. ESP32의 기존 처리 경계는 byte stream→parser→FrameCRC→해당 typed Decoder→Session/Boot·LINK_SEQUENCE·의미검사→Gateway subtype convention 검증→OnDomainMessage다. nested ByteView의 수명을 지키고 raw UART를 그대로 BLE에 복제하지 않는다. 내부 Core/GATT 구현은 이 명세의 작업 범위 밖이다.
6. Query일 때만 같은 문맥의 최종 UART TX 완료수를 M02에 반영한다. 원격 수신/BLE/App 표시 성공으로 승격하지 않는다. End는 응답보다 먼저 성공 완료될 수 없다.

## 대표 결과와 실패 분기

M_RESULT의 원 Request Session 7/ID 9, 현재 recipient Session 12라면 B4..11은 12, B20..27은 7이다. 송신이 실패해도 과거 요청 identity는 변하지 않는다. Encoder OK→Driver accepted까지는 실제 UART 완료가 아니다. 로컬 접수 뒤 **아직 Driver에 제출하지 못한 항목의 최종 제출 실패**가 확정되면 원 REQUEST_RESULT에 필수 후발 실패 sink를 연결할 수 있다. Driver가 이미 참조하며 전송 중인 오류를 DRIVER_SUBMISSION_FINAL로 바꾸지 않는다. 차량 DONE/FAILED를 새로 만들지 않는다.

| 조건 | 해당 출력의 처리 | 독립 흐름 보존 |
|---|---|---|
| CF01 v2 채택 미확인 | 정상 기능 송수신 CONTRACT_HOLD, v1 자동 변환 금지 | 문서상23종 필드/CRC/반례는 계속 상세화 |
| Warning UNCONFIRMED·simulated 또는 원 generation↔Boot 미확인 | CLEAR/실측으로 바꾸지 않고 해당 보고 HOLD | 다른 의미 보고에 없는 필드를 추가하지 않음 |
| Availability UNSUPPORTED/9항목 누락·중복 | raw 0..2 정상 값으로 축약하지 않음 | functionId의 wire 순서를 C2 배열 순서와 명시 대응 |
| UserSettings hasNormalLightLevel 또는 hasNormalLightColor false | 이 composite 미제공; Query 부분 STATUS2 후보 | 확인된 OFF0·RGB0/255·다른 정상응답 보존 |
| DigitalResult 실제 Unlock 전이 표현 필요 | IF-O22 HOLD, Wire bool 추가 금지 | DONE+ALREADY_AT_TARGET은 기본 결과이며 실제 전이 증거가 아님 |
| Session 변경/Boot 변경/미확정 시간 | 새 문맥 재귀속·AGE 0·자동 재실행 금지 | 원 결과 이력의 소유자는 DOMAIN |
| Queue 부족·borrowed buffer 조기 해제 | 전체 접수 거부, 부분 성공 금지 | 기존 discrete 항목을 조용히 덮어쓰지 않음 |
| Driver outcome 미확정/구 callback/release 미확인 | 원 항목 보유, 같은 Query의 count·성공 End 보류 | 저장소 반환과 결과 identity 보존을 별도로 확인 |
| CRC 정상이나 enum/Session/consumer 거부 | 전체 frame 경계 소비, 정상 liveness 갱신 없음 | payload 안의 SYNC를 새 frame으로 재해석하지 않음 |

## 나머지 하향13종과 연결

원 CAN 투영11종의 동일 입력 증거와 WINDOW2 실행·표시·timeout HOLD는 [Query와 원본 투영 Flow](UART_M_QUERY_AND_PROJECTION.md)에서 확인한다. CAN used bytes만 보존하고 padding은 복제하지 않는다. VSS_STATUS 투영은 CF02 VSS_WARNING_STATE의 used51/60 충돌 해소가 아니다.

private [M_DOMAIN_ALIVE](../functions/uart_m/M_DOMAIN_ALIVE.md)와 [M_QUERY_END](../functions/uart_m/M_QUERY_END.md)는 일반 Gateway relay17종에 포함되지 않는다. 링크 alive는 차량/센서 건강의 증거가 아니고 Query End는 BLE/App 완료의 증거가 아니다. [상향4종](UART_M_UPLINK.md), [M02 Query](../modules/M02_QUERY_CONTROL.md), [공통 TX/RX 수명](../functions/UART_M_TX_RX_CONTROL.md), [보고 필드](../data/UART_M_REPORT_FIELDS.md)로 되돌아간다.

원문: B08 `UART_M_ROUTING.md#downlink`, `uart_m/domain_reports.md`, S4A `api/domain_tx.md`, `api/domain_submit.md`, `api/esp32.md`, B09 `TIME_IDENTITY_CONTRACT.md#session`, `FAILURE_RECOVERY_CONTRACT.md#late-failure`. 모든 숫자·원형은 후보 정본의 문서상 추적이며 실제품 시험/팀 채택이 아니다.
