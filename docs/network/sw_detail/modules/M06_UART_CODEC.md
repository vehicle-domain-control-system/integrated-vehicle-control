# M06 — UART-M/C Frame·Stream·Codec

[L04](../layers/L04_FRAME_CODEC.md) · [UART-M23](../05_UART_M_OVERVIEW.md) · [기존9 Module](00_MODULE_INDEX.md) · [M02 Query](M02_QUERY_CONTROL.md)

기존 F16/F17/F18은 UART-M의 byte 조립·재동기화, 정확한 frame/필드 검증, 명시 byte 변환을 맡는다. 별도 Task·Queue·공개 API를 만들지 않는다. UART-M과 UART-C의 Type/schema·소비 경로는 각 절의 독립 계약을 따른다. DOMAIN의 초기 실행 문맥은 Network Superloop의 ISR 밖 단일 소유자 문맥, ESP32는 담당자가 확인한 소유자 문맥이며 ISR에서 parser/CRC/C2를 수행하지 않는다.

| 책임 | 입력 → 출력 | 보관/실패 |
|---|---|---|
| F16 | 실제 byte+원 수신시각 → 완성 Frame 최대1 | ST08의 부분 후보/시각; noise/헤더·CRC·기한 오류에서 시작+1 suffix 재검사 |
| F17 | exact Type/방향/P·CRC frame → 완전 DTO+동일 meta | 임시 OUT 완성 후 commit; enum·reserved 거부로 정상 경계 내부 SYNC 재해석 없음 |
| F18 | 검증된 DTO+승인된 해당 방향 LINK_SEQUENCE → 명시 payload+frame | sizeof/struct 복사 없음; Driver 제출/새 원 관측 발급 없음 |

ST08은 byte 조립 상태이며 차량 cache·Session·원 Boot/수용순서 장부가 아니다. M04는 순서/AGE, M07은 안정 frame 수명, M08은 실제 Driver/timestamp를 맡는다. parser reset으로 진행 TX 참조나 M09/ST12 관측을 임의해제·초기화하지 않는다.

## 공통 함수 전체 원형과 효과

다음15개는 모두 `PROPOSED_PRIVATE_FUNCTION`이며 제품 헤더/SDK 실제 존재 주장이 아니다. 구체 전제·예외·C형 의사코드는 링크된 단일 정본에서 읽는다. 인자 범위·NULL·OUT 겹침·실제 사본 수명은 각 계약을 따른다.

| 전체 원형 | 입력 → 출력 | 효과 | 상세 |
|---|---|---|---|
| `NetUartMResult NetUartM_Lookup(uint8_t type, NetUartMDirection direction, NetUartMRegistryEntry *out);` | Type/물리 방향 → 등록값 | OK 때만 OUT; profile 승인 아님 | [정본](../functions/UART_FRAME_STREAM.md#registry) |
| `NetUartMResult NetUartM_Crc16(const uint8_t *bytes, uint16_t byteCount, uint16_t *outCrc);` | 실제 byteCount 범위 → CRC | 실패 OUT 불변; CRC 계산만 수행 | [정본](../functions/UART_FRAME_STREAM.md#crc) |
| `NetUartMResult NetUartM_CheckFrame(const NetUartMFrame *frame, NetUartMDirection direction, uint8_t expectedType, NetUartMProfile profile);` | 안정 frame/정확 Type/방향/profile → 형식 판정 | OUT/상태 변경 없음; 필드 의미 미판정 | [정본](../functions/UART_FRAME_STREAM.md#check-frame) |
| `NetUartMResult NetUartM_BuildFrame(const uint8_t *payload, uint16_t payloadLength, uint8_t type, uint16_t linkSequence, NetUartMDirection direction, NetUartMProfile profile, NetUartMFrame *out);` | 명시 직렬화 P byte → 완성 frame | 실패 OUT 불변; Driver 제출 없음 | [정본](../functions/UART_FRAME_STREAM.md#build-frame) |
| `NetUartMResult NetUartM_StreamNext(NetUartMParser *parser, const NetUartMStreamInput *input, NetUartMDirection direction, NetUartMProfile profile, uint16_t *consumed, NetUartMFrame *out);` | borrowed byte/시각 → 완성 frame 최대1 | parser/consumed 변경; OUT은 OK일 때만 변경 | [정본](../functions/UART_FRAME_STREAM.md#stream-next) |
| `void NetUartM_DiscardPrefix(NetUartMParser *parser, uint16_t count);` | 소유 parser의 확인 범위 → 보관 suffix | 원 byte 시각도 이동; private 선행 범위 확인 필수 | [정본](../functions/UART_FRAME_STREAM.md#stream) |
| `NetUartMResult NetUartM_ResetStream(NetUartMParser *parser, uint32_t newLinkEpoch, bool bytesLost);` | 실제 reset/손실 근거 → 부분 후보 폐기 | parser만 변경; pending TX/Driver 참조 해제 없음 | [정본](../functions/UART_FRAME_STREAM.md#reset) |
| `uint16_t NetUartM_ReadLe16(const uint8_t *bytes);` | 확보한2B → u16 | 필드 범위 확인 후 호출; 상태 변경 없음 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `void NetUartM_WriteLe16(uint8_t *bytes, uint16_t value);` | u16 → 소유 임시2B | 확인 범위에만 쓰기; 공개 중 버퍼 금지 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `uint32_t NetUartM_ReadLe32(const uint8_t *bytes);` | 확보한4B → u32 | 필드 범위 확인 후 호출; 상태 변경 없음 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `void NetUartM_WriteLe32(uint8_t *bytes, uint32_t value);` | u32 → 소유 임시4B | 확인 범위에만 쓰기; 공개 중 버퍼 금지 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `uint64_t NetUartM_ReadLe64(const uint8_t *bytes);` | 확보한8B → u64 | 필드 범위 확인 후 호출; 상태 변경 없음 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `void NetUartM_WriteLe64(uint8_t *bytes, uint64_t value);` | u64 → 소유 임시8B | 확인 범위에만 쓰기; 공개 중 버퍼 금지 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `int16_t NetUartM_ReadI16(const uint8_t *bytes);` | 확보한2B → i16 | 2의 보수 명시 변환; 센서 범위 판정 없음 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |
| `void NetUartM_WriteI16(uint8_t *bytes, int16_t value);` | i16 → 소유 임시2B | modulo65536; 새 enum 변환 없음 | [정본](../functions/UART_FRAME_STREAM.md#le-fields) |

## Router·전송 제어와 연결하는 함수

다음6개도 private 제안이다. M06의 typed 분류와 M03/M04/M07의 접점을 연결하며, 순서·AGE·Driver 수명 책임을 Codec로 옮기지 않는다.

| 전체 원형 | 입력 → 출력 | 효과·담당 | 상세 |
|---|---|---|---|
| `NetUartMResult NetUartM_DecodeDispatch(const NetUartMFrame *frame, NetUartMDirection direction, NetUartMProfile profile, NetUartMDecoded *out);` | 안정 frame → 정확한 union 멤버+동일 meta | M06 형식 분류; 실패 OUT 불변, 의미 소비 없음 | [Router](../functions/UART_M_ROUTER.md#decode-dispatch) |
| `NetUartMResult NetUartM_BuildDownlinkView(const NetUartMFrame *frame, NetUartMProfile profile, bool bindingVerified, uint8_t *logicalPayload, size_t capacity, Gateway_RelayMessageView_t *out);` | 동일 frame·확인한 Adapter 계약 → 중첩 view+payload 사본 | M03 접점; 모든 실패 검사 후 두 OUT commit, 수명은 caller 책임 | [Gateway view](../functions/UART_M_ROUTER.md#gateway-build) |
| `NetUartMResult NetUartM_AccumulateAge16(uint16_t originalAgeMs, uint32_t unaccountedLocalMs, uint32_t unaccountedHopMs, bool timeBasisVerified, uint16_t *outAgeMs);` | 확인한 미반영 경과 → 포화 AGE | M04 산술; clock 읽기/새 관측 발급 없음 | [AGE](../functions/UART_M_TX_RX_CONTROL.md#age) |
| `NetUartMResult NetUartM_ClassifyRx(const NetUartMRxComparison *c);` | 실제 문맥/순서/원 입력 비교 → 수용 분류 | M04; 장부/consumer 변경 없음 | [순서](../functions/UART_M_TX_RX_CONTROL.md#rx-order) |
| `NetUartMResult NetUartM_ObservePort(NetUartMTxRecord *record, const NetUartMPortEvent *event);` | 같은 제출의 실제 장치 관측 → record | M07/M08; admission/outcome/release 분리, 충돌 시 원 record 불변 | [Driver 관측](../functions/UART_M_TX_RX_CONTROL.md#port-observe) |
| `bool NetUartM_CanReturnBuffer(const NetUartMTxRecord *record);` | 제출 수명·실제 참조 종료 → 반환 자격 | M07 predicate만 수행; allocator/identity 삭제 아님 | [회수](../functions/UART_M_TX_RX_CONTROL.md#reclaim) |

## 메시지별 typed Codec 연결

아래23개 Function에 실제 기존 API와 원형·Owner, 정확한 offset/폭/LE·range/sentinel/enum, 제안 encode/decode와 실패출력 불변·반례가 있다. 기존 C타입 선언은 Data 정본 한 곳만 사용한다.

[M_CONTEXT](../functions/uart_m/M_CONTEXT.md) · [M_REQUEST](../functions/uart_m/M_REQUEST.md) · [M_QUERY](../functions/uart_m/M_QUERY.md) · [M_WARNING_ACK](../functions/uart_m/M_WARNING_ACK.md) · [M_DOMAIN_ALIVE](../functions/uart_m/M_DOMAIN_ALIVE.md) · [M_RESULT](../functions/uart_m/M_RESULT.md) · [M_WARNING](../functions/uart_m/M_WARNING.md) · [M_AVAILABILITY](../functions/uart_m/M_AVAILABILITY.md) · [M_DIGITAL_STATUS](../functions/uart_m/M_DIGITAL_STATUS.md) · [M_DIGITAL_RESULT](../functions/uart_m/M_DIGITAL_RESULT.md) · [M_USER_SETTINGS](../functions/uart_m/M_USER_SETTINGS.md) · [M_QUERY_END](../functions/uart_m/M_QUERY_END.md) · [M_BCM_DOOR_STATE](../functions/uart_m/M_BCM_DOOR_STATE.md) · [M_BCM_CLIMATE_STATE](../functions/uart_m/M_BCM_CLIMATE_STATE.md) · [M_BCM_LIGHT_STATE](../functions/uart_m/M_BCM_LIGHT_STATE.md) · [M_BCM_STATUS](../functions/uart_m/M_BCM_STATUS.md) · [M_CIS_ENVIRONMENT](../functions/uart_m/M_CIS_ENVIRONMENT.md) · [M_CIS_OCCUPANT](../functions/uart_m/M_CIS_OCCUPANT.md) · [M_CIS_REAR](../functions/uart_m/M_CIS_REAR.md) · [M_CIS_STATUS](../functions/uart_m/M_CIS_STATUS.md) · [M_VSS_STATUS](../functions/uart_m/M_VSS_STATUS.md) · [M_WINDOW_STATE](../functions/uart_m/M_WINDOW_STATE.md) · [M_WINDOW_FAULT](../functions/uart_m/M_WINDOW_FAULT.md)

상향4와 DOMAIN 보고6의 Data는 [기존 UART-M 선언](../data/UART_M_DATA.md)에서, 원 CAN11 필드는 [투영표](../data/UART_M_PROJECTION_FIELDS.md)와 CAN Data anchor에서 추적한다. [Frame 내부 자료](../data/UART_FRAME_DATA.md)만 private 후보다. ALIVE/End는 private 감독이며 기능 보고/투영17과 개수를 섞지 않는다.

## 정상·오류·복구 경로

F16은 borrowed byte/timestamp를 같은 index에 복사한다. 완전 Frame은 마지막 실제 byte 시각으로 조립시간을 판단한다. 불완전 후보의 timer는 이전 입력이 미처리 저장소에 없다는 timerCheckPermitted 근거가 있어야 nowMs로 검사한다. suffix의 원 시각은 이동해도 바꾸지 않는다. clock·timeout 동등 비교·양단 profile 근거가 없으면 TIME_HOLD/CONTRACT_HOLD다.

CRC 정상 경계는 전체 P+10을 소비한 뒤 Router에 한 번 넘긴다. typed·문맥·consumer 거부가 발생해도 그 payload 내부 SYNC를 다시 frame으로 쓰지 않는다. CRC·헤더·조립 오류는 시작+1부터 보관 suffix를 재검사하고 마지막 A5를 보존한다. overflow/reset은 확인된 손실 후 부분 후보를 폐기하되 원 Session·기동을 발급하지 않는다.

WINDOW2는 실제 Decode/Encode·Router·표시·timeout·liveness를 HOLD로 유지한다. 정상 codec 변환과 로컬 접수, 실제 UART TX, ESP32 RX, BLE/App 완료는 별개다. M09/F24/F25/ST12의 raw 관측과 실제 Health 공급 바인딩은 [전송 제어](../functions/UART_M_TX_RX_CONTROL.md) 및 [M09](M09_OBSERVATION.md)와 같다.

[상향](../flows/UART_M_UPLINK.md) · [하향](../flows/UART_M_DOWNLINK.md) · [M02 생산/End](../functions/UART_M_QUERY_CONTROL.md) · [Query흐름](../flows/UART_M_QUERY_AND_PROJECTION.md) · [원 CAN투영](../flows/UART_M_CAN_PROJECTION.md)

<a id="uart-c"></a>
## UART-C 추가 연결

F16~18/ST08의 UART-C 별도 profile은 VERSION 1·TYPE 0x60/0x61/0x62·P 19/16/15다. 25필드를 명시한 byte 위치에 따라 encode/decode하며 NetUartM*와 실제 API/저장소 공유를 가정하지 않는다. [함수/흐름](../functions/UART_C_FRAME_ROUTER.md) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)
