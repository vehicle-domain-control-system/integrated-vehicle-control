# UART-M 상향 4종 — ESP32에서 DOMAIN까지

[UART-M 개요](../05_UART_M_OVERVIEW.md) · [M02](../modules/M02_QUERY_CONTROL.md) · [M06](../modules/M06_UART_CODEC.md) · [Router](../functions/UART_M_ROUTER.md)

ESP32의 실제 Gateway Core/Transport·BLE 구현은 ESP32 담당이다. Network 문서는 기존 공개13/콜백·private 경계와 Wire를 추적한다. `CF01` VERSION2 후보 채택·현재 peer/Session·Driver·소비 바인딩 근거가 없는 현재 상태에서는 정상 기능 소비를 제한한다.

| Wire / Rx | 송신 생산·제출 | DOMAIN typed/기능 경계 | 현재 주요 제한 |
|---|---|---|---|
| WIRE-P02-M_CONTEXT / RX-P02-M_CONTEXT-DOMAIN | PublishRegistrationConnection/AppActivity/Proximity의 별도 원 관측 → EspNetPriv_PublishContext | DomainNetPriv_OnGatewayContext → RxDigitalKeyConnection·RxProximity | 세 원 평가 보존, CF01·공동 문맥·부분 인계/quality 바인딩 HOLD |
| WIRE-P02-M_REQUEST / RX-P02-M_REQUEST-DOMAIN | OnMobileMessage → 기존 Core → SendToDomain | NetMobileRequest → 기존 8 type DomainIf_RxMobileRequest | 원 RequestID/현재 Session 보존; 원 SENT AGE·meta 정책 미해소 |
| WIRE-P02-M_QUERY / RX-P02-M_QUERY-DOMAIN | 동일 Gateway Core/Transport 경계 | DomainNetPriv_ProcessQuery → passive 검사·18 Getter | 최초 SESSION_READY0 허용성 개별검사; scope·권한·final TX/End HOLD |
| WIRE-P02-M_WARNING_ACK / RX-P02-M_WARNING_ACK-DOMAIN | 동일 Gateway Core/Transport 경계 | NetMobileWarningAck → DomainIf_RxWarningAck | 원 Warning Boot/Kind/Occurrence를 확인한 뒤 C2 4필드 인계 |

1. 생산 Owner는 원 Device/Session·Request/Query/Warning 또는 세 독립 평가의 품질/번호/AGE를 제공한다. ESP32 Adapter는 subtype·현재 peer·정확한 schema를 확인한다. broad category나 payload 길이만으로 subtype를 추정하지 않는다.
2. borrowed DTO/ByteView·중첩 bytes 전체와 meta를 사본으로 확보하고 로컬 OK를 반환한다. 준비/용량/형식 거부는 기존 반환으로 보고하며 차량 FAILED를 만들지 않는다. M_REQUEST 재접속 자동 재전송은 금지한다.
3. [메시지별 codec](../functions/uart_m/M_CONTEXT.md#codec)이 정확한 payload를 만든 뒤 공통 envelope·CRC를 작성한다. M04는 방향별 LINK_SEQUENCE를 보존된 제출 수명에 연결하고 M07은 immutable Frame을 M08 Driver에 넘긴다.
4. Driver accepted·최종 TX·참조 해제를 따로 관측한다. 로컬 TX가 DOMAIN의 유효 수신을 증명하지 않는다. 미확정 TX의 사본을 시간 경과만으로 덮어쓰지 않는다.
5. DOMAIN ISR은 장치 관측과 bounded 입력 확보/Owner 전달까지만 맡는다. ISR 밖 단일 소유자 문맥의 [Stream](../functions/UART_FRAME_STREAM.md)은 split/noise/CRC를 처리하고 정확한 profile·TYPE·방향·P를 검사한다.
6. 각 [Decoder](../functions/uart_m/M_REQUEST.md#codec)는 임시 객체에 모든 field를 읽고 전 검사 후 DTO/meta를 commit한다. callback 현재시각을 received_at_ms로 바꾸지 않는다. 말단 field/문맥 거부 시 신뢰된 전체 frame을 소비하고 내부 SYNC를 다시 frame으로 해석하지 않는다.
7. [Router](../functions/UART_M_ROUTER.md)는 Boot·Session·순서·원 AGE·상대 소비 경계를 확인하고 표의 기존 C2/private 함수로 인계한다. Rx OK가 DOMAIN cache commit·차량 실행·Session 발급을 보장하지 않는다.

공통 반례: v1 Context31B를 v2로 채우지 않는다. 같은 Device라도 Session이 다르면 이전 Request/Query를 재귀속하지 않는다. 형식 유효한 UNKNOWN/INVALID는 정상0/FAR/CLEAR와 다르다. duplicate/wrapped LINK_SEQUENCE의 수용 근거가 없으면 ORDER_HOLD이며 새로운 요청 identity를 발급하지 않는다. current context와 Driver 참조의 수명은 링크 복구 이후에도 분리한다.

[M_CONTEXT 상세](../functions/uart_m/M_CONTEXT.md) · [M_REQUEST 상세](../functions/uart_m/M_REQUEST.md) · [M_QUERY 상세](../functions/uart_m/M_QUERY.md) · [M_WARNING_ACK 상세](../functions/uart_m/M_WARNING_ACK.md) · [Query→응답→End](UART_M_QUERY_AND_PROJECTION.md) · [Data 정본](../data/UART_M_DATA.md)

출처: B08 `UART_M_ROUTING.md#uplink/#gateway`, S4A ESP32/domain_private/domain_rx 원 선언; S4C 시간·자원·실패 계약. 바인딩·제품 통신 성공·Interface 채택 증거는 아니다.
