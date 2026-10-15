# UART-M v2 — ESP32↔DOMAIN 23 메시지 후보

Interface v2 · r1 · [규격과 연동 조건](../07_CROSS_NODE_INTERFACE_RULES.md#status)

최근 요청 결과 조회는 DOMAIN Device별8건 보관/최신순 최대5건 제공, 기록별 M_RESULT 순차 전송 뒤 M_QUERY_END로 종료한다. 이 제한은 최근 결과 항목에만 적용하며 전체 상태 Query의 응답 수를5건으로 자르지 않는다. 기존23종 TYPE/Frame/CRC와 QueryEnd 세부 count 후보를 변경하지 않는다. [노드 간 공통 규칙](../07_CROSS_NODE_INTERFACE_RULES.md#query)

**경로·VERSION 2·초기 115200 bps 적용 기준 / 상세 프레이밍·CRC·offset·시간 WIRE_PROPOSED.** MOBILE ↔ BLE ↔ ESP32 ↔ UART-M ↔ DOMAIN이다. UART-M과 BLE 프로파일은 별도다.

[시작 안내](../00_START_HERE.md) · [메시지별 API](../05_MESSAGE_INDEX.md) · [ESP32](../02_NODES/ESP32/00_OVERVIEW.md) · [DOMAIN](../02_NODES/DOMAIN/00_OVERVIEW.md) · [Enum 변환](../04_COMMON/30_ENUM_WIRE_MAPPING.md)

<a id="section-profile"></a>
## 직렬 외형·CRC — 두 UART 공통 제안

115200 bps 시작 baud는 사용자 기준이다. 8N1 / 전이중 / RTS·CTS 미사용은 제안이고 Port·전압·핀은 실물 바인딩 확인이 필요하다. UART-C는 VERSION 1이며 runtime·Type·payload는 별도다.

<a id="section-shared-framing"></a>

| frame byte | 필드 | 표현·범위 |
| --- | --- | --- |
|0~1|SYNC|0xA5,0x5A|
|2|VERSION|UART-M2 / UART-C1|
|3|MESSAGE_TYPE|각 링크 Type별 u8|
|4~5|PAYLOAD_LENGTH|P / u16 LE, 0~128 B. 외형·CRC 제외|
|6~7|LINK_SEQUENCE|u16 LE, 방향별 프레임 순서 modulo 65536. 차량 ID와 구분|
|8~P+7|PAYLOAD|P바이트. 아래 payload B0은 frame B8이다|
|P+8~P+9|CRC16|u16 LE. VERSION부터 payload 끝까지 계산, SYNC 제외|

전체 길이는 P+10 B다. SYNC/payload escape를 사용하지 않는 후보이며 frame 공백·CPU 처리·BLE 분할 비용은 별도다. 지원 VERSION/Type, 정확한 Type 길이, 필수 enum을 검사한다. 정상 CRC frame 안의 SYNC는 payload 데이터이고 잘린 frame은 전체 길이까지 조립한다.

<a id="section-shared-crc"></a>

| CRC 항목 | 제안값 |
| --- | --- |
| polynomial |0x1021|
| init |0xFFFF|
| refin/refout |false/false|
| xorout |0|
| 계산 범위 |frame B2~B(P+7) 포함. CRC 필드·SYNC 제외|
| 저장 |little-endian|
| 검산 |ASCII `123456789`→0x29B1|
| 조립 기한 |50ms PROPOSED|

CRC/길이/50 ms 조립 기한 오류가 발생하면 후보 SYNC 첫 바이트 다음부터 resync한다. CRC 정상은 기능 허용이나 Request 실행 확인과 구분한다. 실제 Framer/Codec·split/concat/overflow·무선 시험은 NOT_EXECUTED다.

<a id="section-context"></a>
## 요청·하향 문맥과 v2 이관

| payload byte | 문맥 | 원소·소유자 |
| --- | --- | --- |
|0~3(상향)|DEVICE_CONTEXT_ID|u32, ESP32 등록 단말·현재 연결에 bind. 0 예약|
|4~11(상향)|SESSION_ID|u64, 현재 단말 문맥. 발급 방식 OPEN|
|0~11(하향)|현재 수신 Device/Session|위와 같은 12 B. 과거 REQUEST_SESSION과 별개|
|12~15(하향)|DOMAIN_BOOT_ID|u32, 현재 DOMAIN 기동|
|16~19(하향)|QUERY_ID|u32, 조회는 원 ID, 자발 갱신은 0|

M_CONTEXT 38 B payload / 48 B frame와 등록·연결 shared meta 7 B를 양 끝에서 동시에 맞춘다. legacy 31 B payload / 41 B frame를 VERSION 2의 현재 형식으로 해석하지 않는다. App 활성 meta, 근접 meta, 등록·현재 연결 shared meta는 독립이며 BLE 연결을 App ACTIVE로 합성하지 않는다. UART-M 2에 맞춰 BLE/UART-C의 version을 자동 변경하지 않는다.

M_* 11개 상태 projection은 **DOWN_CONTEXT 20 B + 원 CAN 사용 B**다. B20~27의 원 CAN BOOT/TX 순서와 원 관측 sequence를 보존한다. CAN padding은 중계하지 않고 새 Domain Boot로 과거 snapshot 출처를 치환하지 않는다. 모든 AGE는 전달·대기를 한 번만 추가한다. 제어 사용 한도와 App 표시 갱신 중단은 별도로 해석한다.

<a id="section-request-arguments"></a>

<a id="r2-uart-origin"></a>
### PROPOSED — 11 원 CAN projection과 부분 export

원 CAN `Net*` 수신 객체+`NetCanRxMeta`를 C2 ingress/Getter/TxPort의 동일 원 입력과 owner 직렬 처리 안에서 association한 경우만 DOWN_CONTEXT+원 CAN used(+20 B)로 투영한다. C2 값만 같다는 이유로 최신 CAN Boot/TX를 붙이지 않는다. Pi 관측 identity와 CIS CAN sender identity도 서로 보존한다. 이 결합 증거가 없으면 해당 projection만 local NOT_READY/source NOT_AVAILABLE 및 Query 부분으로 제한하고, Domain Boot/TX/AGE0로 가짜 원본을 만들지 않는다. 원 padding/raw/길이/offset은 변경하지 않는다. 상세 결합·실패는 [DOMAIN 원본 결합](../02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-origin-join)을 따른다.

M_USER_SETTINGS의 existing sentinel은 미확인 값에만 사용하며 확인된0/OFF는 보존한다. 독립 sentinel이 없는 NORMAL 밝기/RGB의 has=false이면 해당 composite frame 누락과 Query STATUS2를 권장한다. M_DIGITAL_RESULT의 DONE는 결과 완료이며 ALREADY_AT_TARGET는 실제 Unlock 전이를 뜻하지 않는다. Wire에 없는 전이 bool을 임의 추론하지 않고 그 전이 표시만 제한한다. [부분 export 규칙](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#r2-partial-export)은 `PROPOSED`; 양단 모듈 채택·Codec binding은 아직 미확인이다.

## M_REQUEST — Kind/operation/ARG의 정확한 분해


| C2 request | Kind | operation | ARG0~5 |
| --- | --- | --- | --- |
| MOBILE_REQUEST_DOOR_LOCK | kind1 | op0 LOCK,1 UNLOCK | ARG0..5=0 |
| MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE | kind2 | op1 | ARG0..1=i16 LE centi-°C, ARG2..5=0 |
| MOBILE_REQUEST_CLIMATE_AUTO_ENABLE | kind2 | op2 | ARG0=bool0/1, ARG1..5=0 |
| MOBILE_REQUEST_FAN_LEVEL | kind2 | op3 | ARG0=Fan0..3, ARG1..5=0 |
| MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE | kind3 | op1 | ARG0=bool0/1, ARG1..5=0 |
| MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL | kind3 | op2 | ARG0=0..100%, ARG1..5=0 |
| MOBILE_REQUEST_INTERIOR_LIGHT_COLOR | kind3 | op3 | ARG0..2=RGB0..255, ARG3..5=0 |
| MOBILE_REQUEST_DIGITAL_KEY_SETTING | kind4 | op1 | ARG0=OFF0/ON1, ARG1..5=0 |


온도는 i16 LE / 0.01°C이며 허용 설정 범위는 DOMAIN 기능 담당의 OPEN이다. 미사용 ARG는 0이다. Fan UNKNOWN은 명령에 허용하지 않는다. 설정 DONE은 설정 반영 완료이며 실내 온도 목표 도달·실제 Fan 출력 완료와 구분한다. 조명 알림 WARNING/FAULT는 DOMAIN 정책이 선택하고 App은 NORMAL 사용·밝기·RGB를 설정한다.

## 메시지 목록 — 시간은 PROPOSED

| 메시지 | ID/Type | 송신 → 수신 | 사용 / 전체 B | 주기 / 수신 중단 |
| --- | --- | --- | --- | --- |
| [M_CONTEXT](#message-m-context) | 0x01 | ESP32 → DOMAIN | 38 / 48 | 100ms / 300ms |
| [M_REQUEST](#message-m-request) | 0x10 | ESP32 → DOMAIN | 24 / 34 | 발생·단계 변경 / 무발생 timeout 없음 |
| [M_QUERY](#message-m-query) | 0x11 | ESP32 → DOMAIN | 32 / 42 | 발생·단계 변경 / 무발생 timeout 없음 |
| [M_WARNING_ACK](#message-m-warning-ack) | 0x12 | ESP32 → DOMAIN | 21 / 31 | 발생·단계 변경 / 무발생 timeout 없음 |
| [M_DOMAIN_ALIVE](#message-m-domain-alive) | 0x02 | DOMAIN → ESP32 | 12 / 22 | 250ms / 750ms |
| [M_RESULT](#message-m-result) | 0x20 | DOMAIN → ESP32 | 44 / 54 | 발생·단계 변경 / 무발생 timeout 없음 |
| [M_WARNING](#message-m-warning) | 0x21 | DOMAIN → ESP32 | 40 / 50 | 1000ms / 3000ms |
| [M_AVAILABILITY](#message-m-availability) | 0x22 | DOMAIN → ESP32 | 62 / 72 | 1000ms / 3000ms |
| [M_DIGITAL_STATUS](#message-m-digital-status) | 0x23 | DOMAIN → ESP32 | 30 / 40 | 1000ms / 3000ms |
| [M_DIGITAL_RESULT](#message-m-digital-result) | 0x24 | DOMAIN → ESP32 | 35 / 45 | 발생·단계 변경 / 무발생 timeout 없음 |
| [M_USER_SETTINGS](#message-m-user-settings) | 0x25 | DOMAIN → ESP32 | 36 / 46 | 1000ms / 3000ms |
| [M_QUERY_END](#message-m-query-end) | 0x40 | DOMAIN → ESP32 | 24 / 34 | 발생·단계 변경 / 무발생 timeout 없음 |
| [M_BCM_DOOR_STATE](#message-m-bcm-door-state) | 0x30 | DOMAIN → ESP32 | 40 / 50 | 200ms / 600ms |
| [M_BCM_CLIMATE_STATE](#message-m-bcm-climate-state) | 0x31 | DOMAIN → ESP32 | 44 / 54 | 200ms / 600ms |
| [M_BCM_LIGHT_STATE](#message-m-bcm-light-state) | 0x32 | DOMAIN → ESP32 | 45 / 55 | 200ms / 600ms |
| [M_BCM_STATUS](#message-m-bcm-status) | 0x33 | DOMAIN → ESP32 | 42 / 52 | 1000ms / 3000ms |
| [M_CIS_ENVIRONMENT](#message-m-cis-environment) | 0x34 | DOMAIN → ESP32 | 60 / 70 | 200ms / 600ms |
| [M_CIS_OCCUPANT](#message-m-cis-occupant) | 0x35 | DOMAIN → ESP32 | 45 / 55 | 200ms / 600ms |
| [M_CIS_REAR](#message-m-cis-rear) | 0x36 | DOMAIN → ESP32 | 39 / 49 | 200ms / 600ms |
| [M_CIS_STATUS](#message-m-cis-status) | 0x37 | DOMAIN → ESP32 | 54 / 64 | 1000ms / 3000ms |
| [M_VSS_STATUS](#message-m-vss-status) | 0x3A | DOMAIN → ESP32 | 48 / 58 | 1000ms / 3000ms |
| [M_WINDOW_STATE](#message-m-window-state) | 0x38 | DOMAIN → ESP32 | 42 / 52 | 200ms / 600ms |
| [M_WINDOW_FAULT](#message-m-window-fault) | 0x39 | DOMAIN → ESP32 | 42 / 52 | 1000ms / 3000ms |


<a id="message-m-context"></a>
## `M_CONTEXT` — `0x01`

**ESP32 → DOMAIN · 38 B 사용 / 48 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileContext`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

등록·현재 연결 shared meta 7 B를 추가한 38 B payload / 48 B frame 후보이다. legacy 31 B payload / 41 B frame는 수용하지 않는다. 양 끝이 같은 v2 Codec을 선택하는 것은 CONTRACT_OPEN다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~11 | `DEVICE_CONTEXT_ID`, `SESSION_ID` | u32/u64. 현재 단말·세션 문맥 | E2D-003/004 |
| B12 | `DEVICE_REGISTRATION_STATE` | `u8` · 0=NOT_REGISTERED; 1=REGISTERED; 255=UNKNOWN | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B13 | `BT_CONNECTION_STATE` | `u8` · 0=DISCONNECTED; 1=CONNECTED; 255=UNKNOWN | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B14 | `APP_ACTIVE_STATE` | `u8` · 0=INACTIVE; 1=ACTIVE; 255=UNKNOWN | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B15 | `APP_QUALITY` | `u8` · VALUE_QUALITY | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B16~19 | `APP_UPDATE_SEQUENCE` | `u32` · App의 새 활성 정보 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B20~21 | `APP_AGE` | `u16` · App 활성 정보 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B22 | `PROXIMITY_STATE` | `u8` · 0=FAR; 1=NEAR; 255=UNKNOWN | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B23 | `PROXIMITY_VALIDITY` | `u8` · 0=INVALID; 1=VALID | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B24 | `PROXIMITY_REASON` | `u8` · 품질 사유 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B25~28 | `PROXIMITY_UPDATE_SEQUENCE` | `u32` · ESP32 새 RSSI 평가 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B29~30 | `PROXIMITY_AGE` | `u16` · RSSI 평가 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B31 | `REGISTRATION_CONNECTION_QUALITY` | `u8` · 등록·현재 연결 coherent context의 공유 VALUE_QUALITY | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B32~35 | `REGISTRATION_CONNECTION_UPDATE_SEQUENCE` | `u32` · 기존 owner의 shared observation revision을 반영한 실제 공동 평가 순서 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B36~37 | `REGISTRATION_CONNECTION_AGE` | `u16` · 같은 등록·현재 peer/연결 평가 이후 누적 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-request"></a>
## `M_REQUEST` — `0x10`

**ESP32 → DOMAIN · 24 B 사용 / 34 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileRequest`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~11 | `DEVICE_CONTEXT_ID`, `SESSION_ID` | u32/u64. 현재 단말·세션 문맥 | E2D-003/004 |
| B12~15 | `REQUEST_ID` | `u32` · App이 새 사용자 입력마다 생성 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B16 | `REQUEST_KIND` | `u8` · 1=도어; 2=공조; 3=실내 조명; 4=디지털 키 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B17 | `OPERATION` | `u8` · 요청 종류별 동작 코드 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B18 | `ARG_0` | `u8` · §6.1 요청 종류별 인수 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B19 | `ARG_1` | `u8` · §6.1 요청 종류별 인수 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B20 | `ARG_2` | `u8` · §6.1 요청 종류별 인수 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B21 | `ARG_3` | `u8` · §6.1 요청 종류별 인수 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B22 | `ARG_4` | `u8` · §6.1 요청 종류별 인수 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B23 | `ARG_5` | `u8` · §6.1 요청 종류별 인수 | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="r2-request-deadline"></a>
### 조건부 PROPOSED — 구 SENT3s 정책 확인과 현재 Request 제한 (N01/)

구 SysRS의 MOBILE SENT부터3s 정책은 최신 효력이 확인되지 않았고, 현재 M_REQUEST24 B에는 원 SENT 시각/AGE가 없다. **NETWORK_OWNED 권장 현재 처리**는 peer/Session 재검증, 원 RequestId 보존, 링크/현재 문맥이 준비되지 않은 시점의 요청을 기존 NOT_READY로 로컬 거부, 재연결을 넘는 보류 Request 자동 송신 금지다. 이 안은 늦은 재생/중복 실행을 줄이지만 ESP32 수신 시각을 MOBILE SENT로 바꾸거나 end-to-end3s 준수를 증명하지 않는다.

**기능 정책·연동 조건 미확정:** MOBILE SENT 기준 3s 정책의 적용 여부와 원 SENT 경과를 신뢰할 BLE/producer evidence·Gateway 단조시간 대응 근거가 필요하다. 그 근거가 있으면 원 경과+Gateway 대기+남은 전달 상한으로 수용 기한을 검사하는 후보를 적용할 수 있다. 근거가 없으면 해당 3s 정책에 의존하는 실행은 `SAFE_RESTRICTED`이며 Query/정상 상태 조회 전체를 차단하지 않는다. Gateway 수신 이후 3s만 검사하는 것은 원 정책과 동등하지 않다. 원 시간 Wire 추가는 별도 계약 변경이 필요하며 현재 24 B/원형에는 포함되지 않는다. 3.5s 지연·재연결·중복/큐 대기 동작은 검증 전이고, 정책 미확정과 구현 결함을 구분한다.

<a id="message-m-query"></a>
## `M_QUERY` — `0x11`

**ESP32 → DOMAIN · 32 B 사용 / 42 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileQuery`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

MOBILE 원 Query → Gateway 현재 Device/Session 검증 → DOMAIN private Query / 기존 snapshot Getter 18개. 기능 실행 없는 조회이며 SESSION_READY=0인 최초 문맥 조회와 App 초기 목록 완료를 구분한다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~11 | `DEVICE_CONTEXT_ID`, `SESSION_ID` | u32/u64. 현재 단말·세션 문맥 | E2D-003/004 |
| B12~15 | `QUERY_ID` | `u32` · 이번 조회 식별, 0 금지 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B16 | `QUERY_SCOPE` | `u8` · 0=현재 상태; 1=요청 결과; 2=경고; 3=전체 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B17 | `RESERVED` | `u8` · 0 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B18~25 | `REQUEST_SESSION` | `u64` · 조회할 원 요청 Session; 상태 조회면 0 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B26~29 | `REQUEST_ID` | `u32` · 조회할 요청; 상태 조회면 0 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B30~31 | `CURSOR` | `u16` · 이 초안에서는 0 | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-warning-ack"></a>
## `M_WARNING_ACK` — `0x12`

**ESP32 → DOMAIN · 21 B 사용 / 31 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileWarningAck`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

Domain Boot + Warning Type + Occurrence + 원 단말 문맥에 대한 읽음이다. 경고 Type은[명시 역변환](../04_COMMON/30_ENUM_WIRE_MAPPING.md#map-domainwarningtype-t). 위험 CLEAR·음향 STOP과 구분한다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~11 | `DEVICE_CONTEXT_ID`, `SESSION_ID` | u32/u64. 현재 단말·세션 문맥 | E2D-003/004 |
| B12 | `WARNING_TYPE` | `u8` · 경고 종류 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B13~16 | `OCCURRENCE_ID` | `u32` · 읽은 경고 발생 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B17~20 | `WARNING_DOMAIN_BOOT` | `u32` · 읽은 경고의 Domain 기동 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-domain-alive"></a>
## `M_DOMAIN_ALIVE` — `0x02`

**DOMAIN → ESP32 · 12 B 사용 / 22 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileDomainAlive`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

**NETWORK_PRIVATE**. DOMAIN 링크 생존·기동 평가 → ESP32 private 감시. BLE/App 원 frame relay와 실행 Result 합성을 금지한다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~3 | `DOMAIN_BOOT_ID` | `u32` · Domain 현재 기동 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B4~7 | `UPDATE_SEQUENCE` | `u32` · Domain 생존 평가 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B8~9 | `SOURCE_AGE` | `u16` · 평가 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B10 | `DOMAIN_STATE` | `u8` · 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B11 | `VALUE_QUALITY` | `u8` · VALUE_QUALITY | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-result"></a>
## `M_RESULT` — `0x20`

**DOMAIN → ESP32 · 44 B 사용 / 54 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileResult`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `REQUEST_SESSION` | `u64` · 원 요청 Session | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B28~31 | `REQUEST_ID` | `u32` · 원 요청 ID | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B32 | `RESULT` | `u8` · 공통 차량 Result; 확인 기록 없으면 255=필드 없음 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B33~34 | `RESULT_REASON` | `u16` · 공통 사유 코드 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B35 | `RESULT_CONFIRMATION` | `u8` · 0=UNCONFIRMED; 1=CONFIRMED | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B36 | `COMMAND_TARGET` | `u8` · 0=Domain 설정; 1=BCM; 2=WINDOW | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B37~40 | `COMMAND_ID` | `u32` · 실행 명령 연계; 설정만 반영한 경우 0 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B41 | `ORIGIN` | `u8` · 요청 출처 코드 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B42~43 | `RESULT_AGE` | `u16` · 결과 확인 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-warning"></a>
## `M_WARNING` — `0x21`

**DOMAIN → ESP32 · 40 B 사용 / 50 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileWarning`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20 | `WARNING_TYPE` | `u8` · 경고 종류 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B21~24 | `OCCURRENCE_ID` | `u32` · Domain 기동 내 경고 발생 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B25 | `SEVERITY` | `u8` · 0=INFO; 1=CAUTION; 2=EMERGENCY | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B26 | `WARNING_STATE` | `u8` · 0=CLEAR; 1=ACTIVE | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B27 | `READ_STATE` | `u8` · 0=UNREAD; 1=READ | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B28 | `VALUE_QUALITY` | `u8` · VALUE_QUALITY | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B29 | `QUALITY_REASON` | `u8` · 품질 사유 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B30~33 | `SOURCE_BOOT` | `u32` · 원 관측 생성 노드 기동 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B34~37 | `SOURCE_SEQUENCE` | `u32` · 원 관측 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B38~39 | `SOURCE_AGE` | `u16` · 원 관측 후 누적 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-availability"></a>
## `M_AVAILABILITY` — `0x22`

**DOMAIN → ESP32 · 62 B 사용 / 72 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileAvailability`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

기능 항목 i=0..8의 순서는 DOOR / CLIMATE / INTERIOR_LIGHT / DIGITAL_KEY / OCCUPANT / ENVIRONMENT / REAR_WARNING / WINDOW / VSS이며 FUNCTION_ID=1..9다. C2 기능 enum 0..8 순서와 다르므로[명시 변환](../04_COMMON/30_ENUM_WIRE_MAPPING.md#map-domainfunctionid-t)을 사용한다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~55 | `9개 기능 항목` | `항목 i=0~8: B20+4i 기능 ID/u8, +1 가용성/u8, +2~3 사유/u16` · 항목 i=0~8: B20+4i 기능 ID/u8, +1 가용성/u8, +2~3 사유/u16 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B56~59 | `UPDATE_SEQUENCE` | `u32, 새 가용성 평가 번호` · u32, 새 가용성 평가 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B60~61 | `SOURCE_AGE` | `u16, 평가 후 ms` · u16, 평가 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-digital-status"></a>
## `M_DIGITAL_STATUS` — `0x23`

**DOMAIN → ESP32 · 30 B 사용 / 40 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileDigitalStatus`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20 | `DIGITAL_KEY_SETTING_STATE` | `u8` · 0=OFF; 1=ON | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B21 | `DIGITAL_KEY_AVAILABILITY` | `u8` · 0=AVAILABLE; 1=LIMITED; 2=UNAVAILABLE | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B22~23 | `REASON` | `u16` · 공통 사유 코드 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B24~27 | `UPDATE_SEQUENCE` | `u32` · 새 판단/설정 평가 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B28~29 | `SOURCE_AGE` | `u16` · 평가 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-digital-result"></a>
## `M_DIGITAL_RESULT` — `0x24`

**DOMAIN → ESP32 · 35 B 사용 / 45 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileDigitalResult`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~23 | `AUTO_OCCURRENCE_ID` | `u32` · 자동 Unlock 실행 식별 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B24~27 | `RELATED_DOOR_COMMAND_ID` | `u32` · BCM 도어 명령 연계 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B28 | `ORIGIN` | `u8` · 2=PROXIMITY_AUTO | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B29 | `RESULT` | `u8` · 공통 Result; 기록 없으면 255=필드 없음 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B30~31 | `RESULT_REASON` | `u16` · 공통 사유 코드 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B32 | `RESULT_CONFIRMATION` | `u8` · 0=UNCONFIRMED; 1=CONFIRMED | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B33~34 | `RESULT_AGE` | `u16` · 결과 확인 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-user-settings"></a>
## `M_USER_SETTINGS` — `0x25`

**DOMAIN → ESP32 · 36 B 사용 / 46 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileUserSettings`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~21 | `TARGET_TEMPERATURE` | `i16` · Domain이 반영한 목표 온도, 0.01°C/raw, -32768=미확인 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B22 | `CLIMATE_AUTO` | `u8` · 0=OFF; 1=ON; 255=미확인 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B23 | `USER_FAN_LEVEL` | `u8` · 사용자가 설정한 FAN 코드 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B24 | `INTERIOR_LIGHT_ENABLED` | `u8` · 0=OFF; 1=ON; 255=미확인 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B25 | `NORMAL_LIGHT_LEVEL` | `u8` · 사용자 NORMAL 밝기,0~100% | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B26~28 | `NORMAL_LIGHT_RGB` | `rgb` · 사용자 NORMAL 색상 R/G/B | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B29 | `VALUE_QUALITY` | `u8` · 설정 확인 품질 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B30~33 | `UPDATE_SEQUENCE` | `u32` · 현재 설정 확인 평가 번호 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B34~35 | `SOURCE_AGE` | `u16` · 설정 확인 평가 후 ms | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="message-m-query-end"></a>
## `M_QUERY_END` — `0x40`

**DOMAIN → ESP32 · 24 B 사용 / 34 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileQueryEnd`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

**NETWORK_PRIVATE 조회 종료**. QUERY_ID / 현재 Session / Boot / ITEM_COUNT를 맞춘다. End 수신만으로 목록 누락 없음·차량 DONE·App 동기화 완료를 단정하지 않는다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20 | `QUERY_SCOPE` | `u8` · 원 조회 범위 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B21 | `QUERY_STATUS` | `u8` · 0=완료; 1=요청 없음; 2=일부 확인 불가; 3=문맥 거부 | 메시지별 부가 필드(현 UART-M Wire 계약) |
| B22~23 | `ITEM_COUNT` | `u16` · 조회 종료를 뺀 응답 메시지 수 | 메시지별 부가 필드(현 UART-M Wire 계약) |


<a id="r2-query-contract"></a>
### PROPOSED — Query 양단 완료·0건·부분 사용

성공한 최종 로컬 UART TX 응답만 ITEM_COUNT에 포함한다. 확정 실패 terminal은 종결 판정 근거지만 성공 count가 아니며 pending이 남으면 End를 보류한다. 10개 중 8개 성공/2개 확정 실패는 `STATUS=2, ITEM_COUNT=8` 후보이고 8개 성공/2개 pending은 End 보류다. 품질 UNCONFIRMED인 정상 응답의 TX 성공과 송신 결과 미확인은 다르다. [DOMAIN 종결 규칙](../02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer)의 PROPOSED/BINDING_OPEN 지위를 유지한다.

기존 byte/STATUS 숫자는 그대로다. 권장 `ITEM_COUNT`는 같은 현재 Device/Session·Domain Boot·QueryID/scope의 응답 중 **최종 로컬 UART TX 완료를 증명한 N**이며 End·자발 QUERY_ID0·중복 frame은 제외한다. DOMAIN은 모든 응답의 terminal TX 완료/확정 실패 후 End를 그 뒤 송신한다. enqueue/NET_SUBMIT_OK는 count 근거가 아니고 UART TX 완료도 ESP32/BLE 수신 증거가 아니다.

기존 `QUERY_STATUS=0`은 해당 scope의 필요한 항목 확인·encode·provenance 및 실송신에서 누락이 없는 경우만 전송 목록 완료 후보로 쓴다. ESP32는 같은 활성 문맥·scope·Boot와 실제 유효 수신 N=`ITEM_COUNT`, 수신 reject/미확인 손실 없음까지 확인한다. 각 응답의 UNKNOWN/INVALID/STALE/원 AGE는 그대로이며 목록 완료가 전체 값 최신성을 보증하지 않는다. 기존 `2`는 확인 불가/누락/확정 일부 송신 실패를 뜻하며 TX 완료8/확정 실패2이면 count8의 부분 후보이다. `1,count0`은 원 Request 조회의 기록 부재 확인, `3,count0`은 문맥 거부에만 쓴다.

0건은 원 scope의 빈 집합을 실제 증명한 경우만 `0,count0` 후보이며 Getter 실패·미표현 raw·provenance 부재를 0건 성공으로 만들지 않는다. TX 완료/부분 송신 도착 여부/End 순서·송신을 관측하지 못하면 성공 End를 생성하지 않고 해당 Query 완료 binding을 제한한다. 응답 ordinal이 없으므로 모호한 송신을 새 LINK_SEQUENCE로 자동 재시도하지 않는다. End count 불일치, End 미수신, timeout, Session/Domain Boot 변경은 로컬 조회 불완전/만료로 처리하고 차량 DONE/FAILED를 만들지 않는다. 중복/late End는 닫힌 조회를 재개하지 않는다.

**NETWORK_OWNED + CROSS_NODE_AGREEMENT / PROPOSED:** [DOMAIN 생산자](../02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer)와 [ESP32 소비자](../02_NODES/ESP32/25_PRIVATE_ROUTING.md#r2-query-consumer)가 같은 count/직렬화/0건 기준을 확인해야 한다. driver TX 완료·수신 dedup·timeout 수치·MOBILE/BLE 부분/종결 전달은 `IMPLEMENTATION_BINDING`/기능 모듈 조건 확인 대상이며 `BINDING_OPEN`. 별도 ACK/raw/BLE format을 신설하지 않는다. 실제 Firmware/Codec/Board/무선 시험 `NOT_EXECUTED`.

<a id="message-m-bcm-door-state"></a>
## `M_BCM_DOOR_STATE` — `0x30`

**DOMAIN → ESP32 · 40 B 사용 / 50 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileBcmDoorState`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `DOOR_LOCK_STATE` | `u8` · 0=LOCKED; 1=UNLOCKED; 255=UNKNOWN | B2D-002 |
| B29 | `DOOR_OPEN_STATE` | `u8` · 0=CLOSED; 1=OPEN; 255=UNKNOWN | B2D-003 |
| B30 | `DOOR_COMPOSITE_STATE` | `u8` · 0=NORMAL; 1=INCONSISTENT; 2=UNTRUSTED | B2D-004 |
| B31 | `LOCK_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B32 | `OPEN_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B33 | `COMPOSITE_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B34~37 | `UPDATE_SEQUENCE` | `u32` · 이번 도어 상태 평가 번호 | 공통·추가 상세 |
| B38~39 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |


<a id="message-m-bcm-climate-state"></a>
## `M_BCM_CLIMATE_STATE` — `0x31`

**DOMAIN → ESP32 · 44 B 사용 / 54 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileBcmClimateState`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `FAN_COMMAND_LEVEL` | `u8` · 0=OFF; 1=LOW; 2=MEDIUM; 3=HIGH | B2D-005 |
| B29 | `FAN_MEASURED_LEVEL` | `u8` · FAN 코드 및 255=UNKNOWN | B2D-006 |
| B30 | `THERMAL_DIRECTION_STATE` | `u8` · 0=IDLE; 1=COOL; 2=HEAT | B2D-007 |
| B31 | `THERMAL_OUTPUT_LEVEL` | `u8` · 0~100%, 1%/raw | B2D-008 |
| B32 | `HEAT_REMOVAL_STATE` | `u8` · 0=정상; 1=과열 차단; 255=측정 불가 | B2D-009 |
| B33 | `FAN_COMMAND_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B34 | `FAN_MEASURED_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B35 | `THERMAL_DIRECTION_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B36 | `THERMAL_OUTPUT_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B37 | `HEAT_REMOVAL_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B38~41 | `UPDATE_SEQUENCE` | `u32` · 이번 공조 상태 평가 번호 | 공통·추가 상세 |
| B42~43 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |


<a id="message-m-bcm-light-state"></a>
## `M_BCM_LIGHT_STATE` — `0x32`

**DOMAIN → ESP32 · 45 B 사용 / 55 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileBcmLightState`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `INTERIOR_LIGHT_TYPE_STATE` | `u8` · LIGHT_TYPE 코드 | B2D-010 |
| B29 | `INTERIOR_LIGHT_LEVEL_STATE` | `u8` · 0~100% | B2D-011 |
| B30~32 | `INTERIOR_LIGHT_COLOR_STATE` | `rgb` · R/G/B 각각 0~255 | B2D-012 |
| B33 | `INTERIOR_LIGHT_APPLY_RESULT` | `u8` · 0=적용됨; 1=실패; 255=UNKNOWN | B2D-013 |
| B34 | `LIGHT_PHYSICAL_FEEDBACK_CAPABILITY` | `u8` · 0=미지원; 1=지원; 현재 0 | B2D-014 |
| B35 | `LIGHT_TYPE_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B36 | `LIGHT_LEVEL_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B37 | `LIGHT_COLOR_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B38 | `LIGHT_APPLY_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B39~42 | `UPDATE_SEQUENCE` | `u32` · 이번 조명 적용 상태 평가 번호 | 공통·추가 상세 |
| B43~44 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |


<a id="message-m-bcm-status"></a>
## `M_BCM_STATUS` — `0x33`

**DOMAIN → ESP32 · 42 B 사용 / 52 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileBcmStatus`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `BCM_ECU_STATE` | `u8` · 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | B2D-001 |
| B29~30 | `FAULT_MASK` | `u16` · BCM 고장 비트 0~7 | B2D-026;  B2D-027;  B2D-028; B2D-029;  B2D-030;  B2D-031; B2D-032;  B2D-033 |
| B31 | `ACTIVE_FAULT_CATEGORY` | `u8` · 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM | 공통·추가 상세 |
| B32~33 | `ACTIVE_FAULT_CODE` | `u16` · 현재 주요 고장; bit번호+1 | 공통·추가 상세 |
| B34 | `AFFECTED_FUNCTION` | `u8` · bit0=도어; bit1=공조; bit2=조명 | 공통·추가 상세 |
| B35 | `RECOVERING` | `u8` · 0=아님; 1=복구 중 | 공통·추가 상세 |
| B36~39 | `UPDATE_SEQUENCE` | `u32` · 새 상태 평가 | 공통·추가 상세 |
| B40~41 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |


<a id="message-m-cis-environment"></a>
## `M_CIS_ENVIRONMENT` — `0x34`

**DOMAIN → ESP32 · 60 B 사용 / 70 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileCisEnvironment`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28~29 | `TEMPERATURE` | `i16` · 0.01°C/raw; -4000~12500; -32768=무효 | C2D-003 |
| B30~33 | `TEMPERATURE_SEQUENCE` | `u32` · 해당 값의 새 측정 번호 | C2D-014 |
| B34~35 | `TEMPERATURE_AGE` | `u16` · 해당 값 원본 측정 후 ms | C2D-013 |
| B36 | `TEMPERATURE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B37 | `TEMPERATURE_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B38~39 | `HUMIDITY` | `u16` · 0.01%RH/raw; 0~10000; 65535=무효 | C2D-004 |
| B40~43 | `HUMIDITY_SEQUENCE` | `u32` · 해당 값의 새 측정 번호 | C2D-014 |
| B44~45 | `HUMIDITY_AGE` | `u16` · 해당 값 원본 측정 후 ms | C2D-013 |
| B46 | `HUMIDITY_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B47 | `HUMIDITY_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B48~51 | `ILLUMINANCE` | `u32` · 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 | C2D-005 |
| B52~55 | `ILLUMINANCE_SEQUENCE` | `u32` · 해당 값의 새 측정 번호 | C2D-014 |
| B56~57 | `ILLUMINANCE_AGE` | `u16` · 해당 값 원본 측정 후 ms | C2D-013 |
| B58 | `ILLUMINANCE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B59 | `ILLUMINANCE_REASON` | `u8` · CIS 품질 사유 | C2D-012 |


<a id="message-m-cis-occupant"></a>
## `M_CIS_OCCUPANT` — `0x35`

**DOMAIN → ESP32 · 45 B 사용 / 55 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileCisOccupant`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28~31 | `PI_BOOT_ID` | `u32` · Pi 원본 기동 번호 | C2D-013 |
| B32~35 | `VISION_SEQUENCE` | `u32` · Pi 새 판정 번호, 중계 시 보존 | C2D-014 |
| B36~37 | `VISION_AGE` | `u16` · Pi 원본 판정 후 누적 ms | C2D-013 |
| B38 | `OCCUPANT_PRESENCE` | `u8` · 0=없음; 1=있음; 255=확인 불가 | C2D-001 |
| B39 | `PRESENCE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B40 | `PRESENCE_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B41 | `OCCUPANT_COUNT` | `u8` · 0~5명,255=확인 불가 | C2D-002 |
| B42 | `COUNT_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B43 | `COUNT_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B44 | `VISION_FUNCTION_STATUS` | `u8` · CIS 기능 상태 | C2D-009 |


<a id="message-m-cis-rear"></a>
## `M_CIS_REAR` — `0x36`

**DOMAIN → ESP32 · 39 B 사용 / 49 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileCisRear`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28~31 | `UPDATE_SEQUENCE` | `u32` · 새 거리 관측 번호 | C2D-014 |
| B32~33 | `SOURCE_AGE` | `u16` · 거리 원본 관측 후 ms | C2D-013 |
| B34~35 | `REAR_DISTANCE` | `u16` · 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분 | C2D-006 |
| B36 | `VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B37 | `QUALITY_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B38 | `PROXIMITY_STATUS` | `u8` · 0=VALID_DISTANCE; 1=NO_OBJECT; 2=UNAVAILABLE; 3=FAULT; 4=RECOVERING; 5=INACTIVE | C2D-007 |


<a id="message-m-cis-status"></a>
## `M_CIS_STATUS` — `0x37`

**DOMAIN → ESP32 · 54 B 사용 / 64 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileCisStatus`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `CIS_STATE` | `u8` · 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT | C2D-008 |
| B29 | `VISION_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B30 | `TEMPERATURE_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B31 | `HUMIDITY_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B32 | `ILLUMINANCE_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B33 | `REAR_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B34 | `PI_UART_STATUS` | `u8` · 0=READY; 1=UNAVAILABLE; 2=RECOVERING | C2D-010 |
| B35 | `CAN_INTERFACE_STATUS` | `u8` · 0=READY; 1=UNAVAILABLE; 2=RECOVERING | C2D-010 |
| B36~37 | `FAULT_MASK` | `u16` · CIS 고장 비트 0~8 | C2D-015;  C2D-016;  C2D-017; C2D-018;  C2D-019;  C2D-020; C2D-021;  C2D-022;  C2D-023 |
| B38~39 | `ACTIVE_FAULT` | `u16` · 현재 주요 고장 코드 | C2D-024 |
| B40 | `RECOVERING` | `u8` · 0=아님; 1=복구 중 | C2D-025 |
| B41~42 | `LAST_FAULT` | `u16` · 이번 기동 이후 최근 주요 고장 코드 | C2D-026 |
| B43~46 | `LAST_FAULT_OCCURRENCE` | `u32` · 최근 고장 발생 번호 | C2D-026 |
| B47 | `AFFECTED_FUNCTION` | `u8` · bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 | C2D-027 |
| B48~51 | `UPDATE_SEQUENCE` | `u32` · 이번 상태 평가 번호 | 공통·추가 상세 |
| B52~53 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |


<a id="message-m-vss-status"></a>
## `M_VSS_STATUS` — `0x3A`

**DOMAIN → ESP32 · 48 B 사용 / 58 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileVssStatus`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `VSS_STATE` | `u8` · 0=STARTUP; 1=READY; 2=PLAYING; 3=FAULT | V2D-001 |
| B29 | `VSS_AVAILABILITY` | `u8` · 0=FULL; 1=DEGRADED; 2=UNAVAILABLE | V2D-002 |
| B30 | `VSS_ACCEPTING_EVENTS` | `u8` · 0=불가; 1=수용 가능 | V2D-003 |
| B31 | `VSS_FAULT_ACTIVE` | `u8` · 0=없음; 1=현재 고장 있음 | V2D-004 |
| B32 | `FAULT_MASK` | `u8` · VSS 고장 비트 0~4 | V2D-006;  V2D-007;  V2D-008; V2D-009;  V2D-010 |
| B33~34 | `VSS_LAST_FAULT` | `u16` · 이번 기동 이후 최근 주요 내부 출력 고장 | V2D-005 |
| B35~38 | `LAST_FAULT_OCCURRENCE` | `u32` · 최근 고장 발생 번호 | V2D-005 |
| B39~40 | `LAST_FAULT_AGE` | `u16` · 최근 고장 발생 후 ms; 포화 허용 | V2D-005 |
| B41 | `LAST_FAULT_QUALITY` | `u8` · 0=기록 있음; 3=NO_DATA | V2D-005 |
| B42~45 | `UPDATE_SEQUENCE` | `u32` · 이번 상태 평가 | 공통·추가 상세 |
| B46~47 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |


<a id="message-m-window-state"></a>
## `M_WINDOW_STATE` — `0x38`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**DOMAIN → ESP32 · 42 B 사용 / 52 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileWindowState`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `MOTION_STATE` | `u8` · 0=STOPPED; 1=OPENING; 2=CLOSING; 3=ANTIPINCH_REVERSING; 255=UNKNOWN | W2D-002 |
| B29 | `WINDOW_ECU_STATE` | `u8` · 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | W2D-003 |
| B30 | `WINDOW_POSITION` | `u8` · 0~100% 닫힘 비율,255=확인 불가 | W2D-004 |
| B31 | `WINDOW_VALUE_QUALITY` | `u8` · VALUE_QUALITY | W2D-005 |
| B32 | `FULLY_OPEN_STATE` | `u8` · 0=아님; 1=완전 열림; 255=확인 불가 | W2D-006 |
| B33 | `FULLY_CLOSED_STATE` | `u8` · 0=아님; 1=완전 닫힘; 255=확인 불가 | W2D-007 |
| B34 | `ANTIPINCH_PROTECTION_STATUS` | `u8` · 0=대기; 1=진행; 2=완료; 3=중단 | W2D-011 |
| B35 | `REVERSE_STATE` | `u8` · 0=역전 아님; 1=역전 중; 255=확인 불가 | W2D-011 |
| B36~39 | `UPDATE_SEQUENCE` | `u32` · 새 상태 평가 | 공통·추가 상세 |
| B40~41 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |


<a id="message-m-window-fault"></a>
## `M_WINDOW_FAULT` — `0x39`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**DOMAIN → ESP32 · 42 B 사용 / 52 B 전체 frame · WIRE_PROPOSED.**

Codec 전용 타입 `NetMobileWindowFault`: [DOMAIN Codec Data](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [ESP32 API](../02_NODES/ESP32/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~19 | `DOWN_CONTEXT` | DEVICE_CONTEXT_ID/u32 + SESSION_ID/u64 + DOMAIN_BOOT_ID/u32 + QUERY_ID/u32 | E2D-003/004 · query |
| B20~27 | `ORIGINAL_CAN_BOOT_ID`, `ORIGINAL_CAN_TX_SEQUENCE` | CAN 원 송신자의 u32 BOOT_ID 및 u32 TX_SEQUENCE; 중계하면서 DOMAIN 값으로 변경하지 않음 | CAN 공통 header 원본 보존 |
| B28 | `FAULT_CATEGORY` | `u8` · 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM | W2D-014 |
| B29~30 | `FAULT_CODE` | `u16` · WINDOW 고장 코드 | W2D-014 |
| B31 | `FAULT_STATUS` | `u8` · 0=없음; 1=현재 고장; 2=복구 중 | W2D-014 |
| B32~35 | `OCCURRENCE_ID` | `u32` · 현재/최근 고장 연결 | 공통·추가 상세 |
| B36~39 | `UPDATE_SEQUENCE` | `u32` · 상태 평가 번호 | 공통·추가 상세 |
| B40~41 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |
