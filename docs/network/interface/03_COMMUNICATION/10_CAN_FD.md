# 단일 공유 CAN FD — 22 메시지 후보

Interface v2 · r1 · [규격과 연동 조건](../07_CROSS_NODE_INTERFACE_RULES.md#status)

**메시지 이름·방향 MESSAGE_LOCKED / ID·DLC·offset·시간 WIRE_PROPOSED.** 하나의 공유 CAN에 DOMAIN/BCM/CIS/VSS가 참여한다. WINDOW는 **[구현 보류 — 설계 유지]**이며 현재 연결되지 않은 노드의 수신 중단을 실제 고장으로 판정하지 않는다.

[시작 안내](../00_START_HERE.md) · [48 메시지별 API](../05_MESSAGE_INDEX.md) · [공통 타입·AGE](../04_COMMON/10_DATA_AND_ID_RULES.md) · [C enum↔Raw](../04_COMMON/30_ENUM_WIRE_MAPPING.md)

<a id="section-profile"></a>
## 설정·공통 Header

<a id="r1-can-receive"></a>
수신 순서는 송신 출처·수용 BOOT·CAN ID별로 판정한다. TX_SEQUENCE와 원시 바이트가 모두 같은 정확한 중복 프레임은 새 수신 생존 근거가 아니다. 새 TX_SEQUENCE의 정상 반복은 수용 뒤 해당 기능 수신감시에 전달하지만 원 관측값·생성시각·관측번호를 새로 만들지 않는다. 같은 원 관측의 확인된 품질 저하는 기존 C2로 인계하고, 높은 TX만으로 값 변경/정상 승격을 허용하지 않는다. raw·품질·Meta·순서의 수용 결과를 함께 연결하며 실패 시 일부만 저장 완료로 처리하지 않는다. [공통 분기](../04_COMMON/10_DATA_AND_ID_RULES.md#section-repeat-and-quality)의 실제 binding은 검증 대기다.

CAN TX_SEQUENCE는 수용한 송신 노드 BOOT 안에서 메시지별로 관리한다. 수신 비교 범위는 송신 출처·BOOT·CAN ID/메시지 구분을 포함한다. CIS_ENVIRONMENT(TX1), CIS_OCCUPANT(TX1), CIS_ENVIRONMENT(TX2)는 서로 다른 메시지의 순번을 섞지 않으면 정상 순서다. 원본 센서 관측 순번이나 UART-C의 방향별 LINK_SEQUENCE와 혼용하지 않는다. 새 필드가 아니라 기존 카운터 비교 범위를 명확히 한 것이다.

DLC 코드와 실제 전송 바이트 수를 구별한다. CIS_REAR/DOMAIN_PERMISSION은 사용19B·전송20B·DLC11, CIS_OCCUPANT는25B·32B·DLC13, CIS_ENVIRONMENT는40B·48B·DLC14, CIS_STATUS는34B·48B·DLC14다. 기존 숫자·offset을 변경하지 않는다.

| 항목 | 후보 / 상태 |
| --- | --- |
| 물리 경로 | DOMAIN ↔ 단일 공유 CAN ↔ BCM/CIS/VSS/WINDOW |
| 프레임 | ISO CAN FD / FDF=1 / BRS=1 / RTR 미사용 / 표준 11-bit ID — PROPOSED |
| nominal / data | 500 kbps / 2 Mbps — PROPOSED. 전체 참여 노드의 동일 설정 필요 |
| 직렬화 | 고정 offset의 명시 Codec, 다중 바이트 LE, i16은 2의 보수, RGB는 R/G/B 연속 3바이트 |
| 검출·응답 | CAN FD 자체 오류 검출. 별도 응용 CRC/ACK/Heartbeat는 추가하지 않는 제안 |
| 사용 길이 / DLC | DLC 실제 송신 길이에서 남은 바이트는 0 padding. 사용 길이와 DLC 구분 |
| 물리·SDK | transceiver / clock / bit timing / filter / MB / FIFO / Port / 핀 / 전압 / 종단은 OPEN |

| payload byte | 필드 | Raw 타입 | 생산·소비 |
| --- | --- | --- | --- |
| B0~3 | BOOT_ID | u32LE | 송신 Network의 기동 owner. 0 예약 |
| B4~7 | TX_SEQUENCE | u32LE | 메시지별 송신 프레임 순서. 최초 1, 프레임마다 증가하며 같은 원본의 재전송도 증가 |
| B8~11(명령만) | TARGET_BOOT_ID | u32LE | DOMAIN Network가 마지막 정상 대상 상태에서 확인 |
| B12~15(명령만) | COMMAND_ID | u32LE | **C2 최종 결정의 commandId 그대로**. Network 재발급 금지 |
| B16~17(명령만) | COMMAND_AGE | u16LE | 명령 최초 생성부터 누적. 원 판단의 meta.age와 별개 |
| B18~19(명령만) | ACCEPT_LIMIT | u16LE | 200 ms 후보 ↔ 구 SysRS 1 s 충돌 OPEN. 적용 조건 미확정 |

CAN ACK는 버스에 참여한 노드의 수신 사실을 보여줄 뿐이며, 지정 ECU의 명령 수용이나 기능 실행 DONE을 증명하지 않는다. padding은 의미값으로 사용하지 않는다. 필수 코드·ID·길이·세대 오류를 정상 typed callback이나 liveness로 연결하지 않는다.

DLC 9/10/11/12/13/14/15는 12/16/20/24/32/48/64 B에 대응한다. Codec는 C struct의 `sizeof`를 Wire 길이로 사용하지 않는다. 수신 상태의 UPDATE_SEQUENCE는 새 관측에서만 증가하며 TX_SEQUENCE와 독립이다.

## 메시지 목록 — 시간은 PROPOSED

메시지별 길이·주기는 아래 후보를 적용한다. 후방 활성 Permission은 원 평가 약 200ms/원 기한 500ms와 CAN 50ms 송신/150ms 수신 감시를 적용한다. 온도·습도 데이터의 사용 기한은 원본 관측 기준 각각 2500ms다. 후방 거리 원 150ms 후보와 Permission 원 500ms는 별개다. [시간 규칙](../07_CROSS_NODE_INTERFACE_RULES.md#timing)

| 메시지 | ID/Type | 송신 → 수신 | 사용 / 전체 B | 주기 / 수신 중단 |
| --- | --- | --- | --- | --- |
| [DOMAIN_PERMISSION](#message-domain-permission) | 0x080 | DOMAIN → BCM, CIS, WINDOW, VSS | 19 / 20 / DLC 11 | 50ms / 150ms |
| [BCM_DOOR_COMMAND](#message-bcm-door-command) | 0x100 | DOMAIN → BCM | 22 / 24 / DLC 12 | 발생·단계 변경 / 무발생 timeout 없음 |
| [BCM_CLIMATE_COMMAND](#message-bcm-climate-command) | 0x110 | DOMAIN → BCM | 23 / 24 / DLC 12 | 발생·단계 변경 / 무발생 timeout 없음 |
| [BCM_LIGHT_COMMAND](#message-bcm-light-command) | 0x120 | DOMAIN → BCM | 25 / 32 / DLC 13 | 발생·단계 변경 / 무발생 timeout 없음 |
| [WINDOW_COMMAND](#message-window-command) | 0x090 | DOMAIN → WINDOW | 24 / 24 / DLC 12 | 발생·단계 변경 / 무발생 timeout 없음 |
| [BCM_DOOR_STATE](#message-bcm-door-state) | 0x200 | BCM → DOMAIN | 20 / 20 / DLC 11 | 100ms / 300ms |
| [BCM_CLIMATE_STATE](#message-bcm-climate-state) | 0x210 | BCM → DOMAIN | 24 / 24 / DLC 12 | 200ms / 600ms |
| [BCM_LIGHT_STATE](#message-bcm-light-state) | 0x220 | BCM → DOMAIN | 25 / 32 / DLC 13 | 200ms / 600ms |
| [BCM_RESULT](#message-bcm-result) | 0x140 | BCM → DOMAIN | 29 / 32 / DLC 13 | 발생·단계 변경 / 무발생 timeout 없음 |
| [BCM_EVENT](#message-bcm-event) | 0x141 | BCM → DOMAIN | 25 / 32 / DLC 13 | 발생·단계 변경 / 무발생 timeout 없음 |
| [BCM_STATUS](#message-bcm-status) | 0x230 | BCM → DOMAIN | 22 / 24 / DLC 12 | 250ms / 750ms |
| [CIS_ENVIRONMENT](#message-cis-environment) | 0x300 | CIS → DOMAIN | 40 / 48 / DLC 14 | 200ms / 600ms |
| [CIS_OCCUPANT](#message-cis-occupant) | 0x310 | CIS → DOMAIN | 25 / 32 / DLC 13 | 100ms / 300ms |
| [CIS_REAR](#message-cis-rear) | 0x0A0 | CIS → DOMAIN | 19 / 20 / DLC 11 | 50ms / 150ms |
| [CIS_STATUS](#message-cis-status) | 0x320 | CIS → DOMAIN | 34 / 48 / DLC 14 | 250ms / 750ms |
| [WINDOW_STATE](#message-window-state) | 0x0B0 | WINDOW → DOMAIN | 22 / 24 / DLC 12 | 50ms / 150ms |
| [WINDOW_EVENT](#message-window-event) | 0x091 | WINDOW → DOMAIN | 26 / 32 / DLC 13 | 발생·단계 변경 / 무발생 timeout 없음 |
| [WINDOW_RESULT](#message-window-result) | 0x092 | WINDOW → DOMAIN | 20 / 20 / DLC 11 | 발생·단계 변경 / 무발생 timeout 없음 |
| [WINDOW_FAULT](#message-window-fault) | 0x330 | WINDOW → DOMAIN | 22 / 24 / DLC 12 | 250ms / 750ms |
| [VSS_EVENT](#message-vss-event) | 0x160 | DOMAIN → VSS | 23 / 24 / DLC 12 | 발생·단계 변경 / 무발생 timeout 없음 |
| [VSS_WARNING_STATE](#message-vss-warning-state) | 0x0C0 | DOMAIN → VSS | 60 / 64 / DLC 15 | 50ms / 150ms |
| [VSS_STATUS](#message-vss-status) | 0x340 | VSS → DOMAIN | 28 / 32 / DLC 13 | 250ms / 750ms |


<a id="message-domain-permission"></a>
## `DOMAIN_PERMISSION` — `0x080`

후방 활성 Power는 약 200ms마다 실제로 재평가하고, 원본 평가의 사용 기한 500ms와 새 평가·값 변경 시 전달 및 CAN 50ms 송신/150ms 수신 감시를 적용한다. 반복 CAN TX는 원 AGE나 원 평가 순번을 초기화하지 않는다. 아래 Power/Operation/Alive 생성·공유 AGE 합성은 별도의 미확정 후보이다. [Permission 규칙](../07_CROSS_NODE_INTERFACE_RULES.md#permission)

**DOMAIN → BCM, CIS, WINDOW, VSS · 19 B 사용 / 20 B CAN 전송 / DLC 11 · WIRE_PROPOSED.**

CIS/WINDOW는 허용 기능을 소비하고 BCM/VSS는 Network-private Domain 감시를 소비한다. [공통 7필드 타입·합성 미정](../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission)과 [합성 규칙](../04_COMMON/10_DATA_AND_ID_RULES.md#section-permission-synthesis)을 따른다.

**초기 송신 경계:** DOMAIN 기능이 제공한 Power 원 값·Quality·AGE를 확인해 `DOMAIN_PERMISSION`의 전원 필드에 반영한다. 확인된 새 평가 또는 의미·품질 변경을 놓치지 않으며, 주기 CAN 송신만으로 원 평가 Sequence와 AGE를 새로 만들지 않는다. DOMAIN Core가 평가 FAILED/NO_DATA에서 이전 ALLOW를 철회한다는 담당자 구현 보고는 실제 TxPort→CAN 경로의 연결 시험 완료와는 구분한다.

`POWER_OPERATION_STATE`, Operation/Alive 결합 조건, 공통 `UPDATE_SEQUENCE/SOURCE_AGE`의 정확한 합성 기준은 기능 Owner의 현재 생산 계약에 따라 확정한다. 미확인 의미는 기존 UNKNOWN/비정상 Quality를 사용하고 임의로 정상 ALLOW나 이동 가능 상태를 만들지 않는다. 두 개 이상의 독립 근거가 합성되는 경우 각 근거의 나이를 축소하거나 순번을 원 성공 평가 번호와 동일시하지 않는다. 기존 세 C2 입력의 전체 합성은 해당 기능 경로를 연결할 때 확인한다. [Permission 합성 조건](../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

<a id="delta-can-permission"></a>
**의미 투영 권장안 / DOMAIN 연동 조건 확인 필요:** DOMAIN Adapter는 확정 ALLOW/DENY만 true/false+OK로 투영하고 UNKNOWN은 원인에 맞는 false+NO_DATA/STALE/INVALID로 구분하는 안을 따른다. CAN에서는 해당 무효 전원을255+비정상 품질로 전달한다. UNKNOWN+OK 보존이 필수인 경우 기존 C2의 표현 손실을 숨기지 않고 [최소 계약 대안](../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)을 별도 검토한다. Network는 의미를 추측하지 않고 C2 품질 충돌·시간·문맥을 검증한다.

아래 B13~16의 ‘허용 상태의 새 평가 번호’는 **Network 합성 UPDATE_SEQUENCE**의 설명이며 DOMAIN 원 성공 평가 Sequence와 동일한 번호라는 뜻이 아니다. B17~18은 위 공통 SOURCE_AGE 후보로서 각 입력의 독립 최신성을 모두 증명하지 못한다. 성공17 뒤 실패는 원 성공17·원 시각을 보존하면서 현재 품질 제한을 전달 요청하는 권장 경로다. 동일 원 성공 Sequence 중복으로 버리거나 실패 시 AGE0을 만들지 않는다. 합성·프레임·relay 갱신은 [번호 공간](../04_COMMON/10_DATA_AND_ID_RULES.md#delta-permission-sequences)으로 구분한다. 이전 Permission 원본 사용 기한 150ms 후보는 500ms로 대체됐으며 CAN 수신 중단150ms·후방 거리150ms 후보·UART1500ms는 유지한다.

| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `VEHICLE_POWER_PERMISSION` | `u8` · 0=불허; 1=허용; 255=확인 불가 | D2C-001 |
| B9 | `POWER_PERMISSION_QUALITY` | `u8` · VALUE_QUALITY | D2C-002 |
| B10 | `POWER_OPERATION_STATE` | `u8` · 0=이동 불가; 1=이동 가능; 255=확인 불가 | D2W-007 |
| B11 | `OPERATION_ALLOWED` | `u8` · 0=불허; 1=허용 | D2W-008 |
| B12 | `OPERATION_QUALITY` | `u8` · VALUE_QUALITY | 공통·추가 상세 |
| B13~16 | `UPDATE_SEQUENCE` | `u32` · 허용 상태의 새 평가 번호 | 공통·추가 상세 |
| B17~18 | `SOURCE_AGE` | `u16` · 허용 상태 평가 후 경과 시간, ms | 공통·추가 상세 |
| B19~19 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-bcm-door-command"></a>
## `BCM_DOOR_COMMAND` — `0x100`

**DOMAIN → BCM · 22 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetBcmDoorCommand`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~19 | `TARGET_BOOT_ID`, `COMMAND_ID`, `COMMAND_AGE`, `ACCEPT_LIMIT` | u32/u32/u16/u16. 기동·명령 ID·원본 Age·수용 한도 | CAN 명령 공통부(TARGET_BOOT_ID·COMMAND_ID·AGE·LIMIT) |
| B20 | `DOOR_LOCK_TARGET` | `u8` · 0=LOCK; 1=UNLOCK | D2B-001 |
| B21 | `ORIGIN` | `u8` · 요청 출처 코드 | 공통·추가 상세 |
| B22~23 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-bcm-climate-command"></a>
## `BCM_CLIMATE_COMMAND` — `0x110`

**DOMAIN → BCM · 23 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetBcmClimateCommand`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~19 | `TARGET_BOOT_ID`, `COMMAND_ID`, `COMMAND_AGE`, `ACCEPT_LIMIT` | u32/u32/u16/u16. 기동·명령 ID·원본 Age·수용 한도 | CAN 명령 공통부(TARGET_BOOT_ID·COMMAND_ID·AGE·LIMIT) |
| B20 | `FAN_TARGET_LEVEL` | `u8` · FAN 코드; UNKNOWN 금지 | D2B-003 |
| B21 | `THERMAL_DIRECTION` | `u8` · 0=IDLE; 1=COOL; 2=HEAT | D2B-004 |
| B22 | `THERMAL_TARGET_LEVEL` | `u8` · 0~100%, 1%/raw | D2B-005 |
| B23~23 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-bcm-light-command"></a>
## `BCM_LIGHT_COMMAND` — `0x120`

**DOMAIN → BCM · 25 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetBcmLightCommand`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~19 | `TARGET_BOOT_ID`, `COMMAND_ID`, `COMMAND_AGE`, `ACCEPT_LIMIT` | u32/u32/u16/u16. 기동·명령 ID·원본 Age·수용 한도 | CAN 명령 공통부(TARGET_BOOT_ID·COMMAND_ID·AGE·LIMIT) |
| B20 | `INTERIOR_LIGHT_TYPE` | `u8` · LIGHT_TYPE 코드 | D2B-006 |
| B21 | `INTERIOR_LIGHT_LEVEL` | `u8` · 0~100%, 1%/raw | D2B-007 |
| B22~24 | `INTERIOR_LIGHT_COLOR` | `rgb` · R/G/B 각각 0~255 | D2B-008 |
| B25~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-window-command"></a>
## `WINDOW_COMMAND` — `0x090`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**DOMAIN → WINDOW · 24 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetWindowCommand`: [WINDOW Data](../02_NODES/WINDOW/10_DATA.md) · [WINDOW API](../02_NODES/WINDOW/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

VENT 목표를 닫힘 기준 70%로 보는 값은 후보이며 구 SysRS 80%와 CONFLICT다. 명령 수용 200 ms와 구 1 s도 적용 계약이 미확정이다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~19 | `TARGET_BOOT_ID`, `COMMAND_ID`, `COMMAND_AGE`, `ACCEPT_LIMIT` | u32/u32/u16/u16. 기동·명령 ID·원본 Age·수용 한도 | CAN 명령 공통부(TARGET_BOOT_ID·COMMAND_ID·AGE·LIMIT) |
| B20 | `WINDOW_COMMAND` | `u8` · 1=OPEN; 2=CLOSE; 3=STOP; 4=VENT; 5=MOVE_TO_POSITION | D2W-001 |
| B21 | `TARGET_CHANNEL` | `u8` · 0=현재 단일 채널 | D2W-002 |
| B22 | `TARGET_POSITION` | `u8` · 0~100% 닫힘 비율; VENT=70 | D2W-006 |
| B23 | `SOURCE_CONTEXT` | `u8` · 상위 요청 출처 코드 | D2W-005 |


<a id="message-bcm-door-state"></a>
## `BCM_DOOR_STATE` — `0x200`

**BCM → DOMAIN · 20 B 사용 / 20 B CAN 전송 / DLC 11 · WIRE_PROPOSED.**

의미 타입 `NetBcmDoorState`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `DOOR_LOCK_STATE` | `u8` · 0=LOCKED; 1=UNLOCKED; 255=UNKNOWN | B2D-002 |
| B9 | `DOOR_OPEN_STATE` | `u8` · 0=CLOSED; 1=OPEN; 255=UNKNOWN | B2D-003 |
| B10 | `DOOR_COMPOSITE_STATE` | `u8` · 0=NORMAL; 1=INCONSISTENT; 2=UNTRUSTED | B2D-004 |
| B11 | `LOCK_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B12 | `OPEN_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B13 | `COMPOSITE_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B14~17 | `UPDATE_SEQUENCE` | `u32` · 이번 도어 상태 평가 번호 | 공통·추가 상세 |
| B18~19 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |


<a id="message-bcm-climate-state"></a>
## `BCM_CLIMATE_STATE` — `0x210`

**BCM → DOMAIN · 24 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetBcmClimateState`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `FAN_COMMAND_LEVEL` | `u8` · 0=OFF; 1=LOW; 2=MEDIUM; 3=HIGH | B2D-005 |
| B9 | `FAN_MEASURED_LEVEL` | `u8` · FAN 코드 및 255=UNKNOWN | B2D-006 |
| B10 | `THERMAL_DIRECTION_STATE` | `u8` · 0=IDLE; 1=COOL; 2=HEAT | B2D-007 |
| B11 | `THERMAL_OUTPUT_LEVEL` | `u8` · 0~100%, 1%/raw | B2D-008 |
| B12 | `HEAT_REMOVAL_STATE` | `u8` · 0=정상; 1=과열 차단; 255=측정 불가 | B2D-009 |
| B13 | `FAN_COMMAND_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B14 | `FAN_MEASURED_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B15 | `THERMAL_DIRECTION_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B16 | `THERMAL_OUTPUT_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B17 | `HEAT_REMOVAL_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B18~21 | `UPDATE_SEQUENCE` | `u32` · 이번 공조 상태 평가 번호 | 공통·추가 상세 |
| B22~23 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |


<a id="message-bcm-light-state"></a>
## `BCM_LIGHT_STATE` — `0x220`

**BCM → DOMAIN · 25 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetBcmLightState`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `INTERIOR_LIGHT_TYPE_STATE` | `u8` · LIGHT_TYPE 코드 | B2D-010 |
| B9 | `INTERIOR_LIGHT_LEVEL_STATE` | `u8` · 0~100% | B2D-011 |
| B10~12 | `INTERIOR_LIGHT_COLOR_STATE` | `rgb` · R/G/B 각각 0~255 | B2D-012 |
| B13 | `INTERIOR_LIGHT_APPLY_RESULT` | `u8` · 0=적용됨; 1=실패; 255=UNKNOWN | B2D-013 |
| B14 | `LIGHT_PHYSICAL_FEEDBACK_CAPABILITY` | `u8` · 0=미지원; 1=지원; 현재 0 | B2D-014 |
| B15 | `LIGHT_TYPE_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B16 | `LIGHT_LEVEL_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B17 | `LIGHT_COLOR_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B18 | `LIGHT_APPLY_QUALITY` | `u8` · VALUE_QUALITY | B2D-015 |
| B19~22 | `UPDATE_SEQUENCE` | `u32` · 이번 조명 적용 상태 평가 번호 | 공통·추가 상세 |
| B23~24 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |
| B25~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-bcm-result"></a>

## `BCM_RESULT` — `0x140`

**BCM/DOMAIN 확인 대기:** Door·Climate·Interior Light는 각각 원 Command ID·명령 종류·세대에 연결된 새로 확인한 실제 상태·품질·Fault와 원본 관측 시각으로 확인 조건을 정해야 한다. 도어 증거를 공조·조명에 일괄 적용하지 않는다. DONE·CAN 수신·Network 접수만으로 CONFIRMED를 만들지 않으며, 현재 Wire로 충분한 근거가 없으면 해당 결과는 UNCONFIRMED로 제한한다. 새 confirmation 필드나 Wire 값은 추가하지 않는다.

**BCM → DOMAIN · 29 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetBcmResult`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


**PROPOSED — 원 실행 연결과 확인의 분리:** 수용한 원 `COMMAND_DOMAIN_BOOT+COMMAND_ID+COMMAND_KIND` 및 원 명령 내용을 기존 실행 문맥에 연결한다. 같은 명령의 재전송은 새 ID나 새 실행이 아니며 원 Application의 단계·결과·occurrence·발생 AGE를 재사용한다. 원본 식별자가 같지만 명령 내용이 다르면 재실행하지 않고 기존 오류 인계로 BCM 실행 owner가 판단한다. 실제 Application 사실만 ACCEPTED/IN_PROGRESS/DONE/REJECTED/FAILED의 근거이며 local submit OK·CAN 수신·결과 미수신으로 실행 DONE/FAILED를 합성하지 않는다.

C2 `DomainIf_BcmCommandResult_t.confirmation`은 이 Wire/`NetBcmResult`에 별도 필드가 없다. 확정된 BCM 의미 근거가 없으면 수신 결과를 기존 `RESULT_CONFIRMATION_UNCONFIRMED`로 소비하는 안을 권장한다. 원 DONE/reason/event를 바꾸지 않으며, 정상 프레임 또는 DONE만으로 CONFIRMED나 실제 unlock 전이를 만들지 않는다. 도어 COMPLETED+유효 event quality/원 AGE가 충분한 확인 근거인지, 비도어 terminal 결과가 어떤 실제 피드백을 뜻하는지는 BCM/DOMAIN 기능 owner가 확인해야 한다. `NETWORK_OWNED`는 원 identity·검증·전달, `FUNCTION_OWNER`는 실행·피드백, `CROSS_NODE_AGREEMENT`는 confirmation 소비, `IMPLEMENTATION_BINDING`은 재전송/실패 인계 연결이다. 후보는 PROPOSED이며 기능 정책과 실제 동작은 검증 전이다.

| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~11 | `COMMAND_DOMAIN_BOOT` | `u32` · 원 명령을 생성한 Domain 기동 번호 | 공통·추가 상세 |
| B12~15 | `COMMAND_ID` | `u32` · 처리한 Domain 명령 ID | D2B-002 |
| B16 | `COMMAND_KIND` | `u8` · 1=도어; 2=공조; 3=조명 | 공통·추가 상세 |
| B17 | `RESULT` | `u8` · 공통 차량 Result 코드 | B2D-016 |
| B18~19 | `RESULT_REASON` | `u16` · 공통 사유 코드 | B2D-017 |
| B20 | `DOOR_EVENT` | `u8` · 0=없음; 1=LOCK_COMPLETED; 2=UNLOCK_COMPLETED; 3=ALREADY_AT_TARGET; 4=REQUEST_REJECTED; 5=REQUEST_FAILED | B2D-018;  B2D-019;  B2D-020; B2D-021;  B2D-022 |
| B21~24 | `OCCURRENCE_ID` | `u32` · 도어 이벤트 번호; 없으면 0 | 공통·추가 상세 |
| B25~26 | `EVENT_AGE` | `u16` · 이벤트 발생 후 경과 시간, ms | 공통·추가 상세 |
| B27 | `EVENT_QUALITY` | `u8` · VALUE_QUALITY | 공통·추가 상세 |
| B28 | `DOOR_TARGET` | `u8` · 0=LOCK; 1=UNLOCK; 255=비도어 | 공통·추가 상세 |
| B29~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-bcm-event"></a>
## `BCM_EVENT` — `0x141`

**BCM → DOMAIN · 25 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetBcmEvent`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `EVENT_KIND` | `u8` · 1=OVERHEAT_DETECTED; 2=FAN_MISMATCH_DETECTED; 3=RECOVERY_CONFIRMED | B2D-023;  B2D-024;  B2D-025 |
| B9~12 | `OCCURRENCE_ID` | `u32` · 이벤트 식별 | 공통·추가 상세 |
| B13~14 | `EVENT_AGE` | `u16` · 발생 후 경과 시간, ms | 공통·추가 상세 |
| B15 | `EVENT_QUALITY` | `u8` · VALUE_QUALITY | 공통·추가 상세 |
| B16~19 | `RELATED_DOMAIN_BOOT` | `u32` · 연결 명령의 Domain 기동 번호; 없으면 0 | 공통·추가 상세 |
| B20~23 | `RELATED_COMMAND_ID` | `u32` · 연결된 명령, 없으면 0 | 공통·추가 상세 |
| B24 | `AFFECTED_FUNCTION` | `u8` · BCM 기능 비트 | 공통·추가 상세 |
| B25~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-bcm-status"></a>
## `BCM_STATUS` — `0x230`

**BCM → DOMAIN · 22 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetBcmStatus`: [BCM Data](../02_NODES/BCM/10_DATA.md) · [BCM API](../02_NODES/BCM/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


**PROPOSED — 통신 Health는 기존 진단 owner가 평가:** Network의 최초 링크 상실 발견 주체·시각·감시 구간과 실제 회복 관측을 기존 BCM 진단 담당자에게 전달한다. owner가 근거에 따라 기존 COMM bit7·영향 기능·회복 상태를 평가해 `BcmNet_PublishStatus()`로 제출한다. DOMAIN이 확인한 BCM 수신 상실은 DOMAIN 로컬 관측이며, BCM이 실제로 보고하지 않은 원격 COMM 비트를 의미하지 않는다. 이 메시지의 SOURCE_AGE는 현재 BCM 평가의 AGE이며 최초 COMM 발생 AGE/최근 이력을 대신하지 않는다. 최초 발견·복구 세부 근거는 기존 owner가 보존한다. VSS에는 COMM mask bit가 없으므로 링크 상실을 AUDIO_OUTPUT_FAILURE로 합성하지 않는다. `NETWORK_OWNED`는 관측/인계, `FUNCTION_OWNER`는 진단·clear, `IMPLEMENTATION_BINDING`은 기존 실제 입력·수명 연결이다. BCM 모듈은 최초 발견/회복을 받는 기존 연결과 actuator feedback timeout의 평가 주체를 확인해야 한다. 실제 연결/SDK 시험은 NOT_EXECUTED이고 처리 안은 PROPOSED다.

| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `BCM_ECU_STATE` | `u8` · 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | B2D-001 |
| B9~10 | `FAULT_MASK` | `u16` · BCM 고장 비트 0~7 | B2D-026;  B2D-027;  B2D-028; B2D-029;  B2D-030;  B2D-031; B2D-032;  B2D-033 |
| B11 | `ACTIVE_FAULT_CATEGORY` | `u8` · 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM | 공통·추가 상세 |
| B12~13 | `ACTIVE_FAULT_CODE` | `u16` · 현재 주요 고장; bit번호+1 | 공통·추가 상세 |
| B14 | `AFFECTED_FUNCTION` | `u8` · bit0=도어; bit1=공조; bit2=조명 | 공통·추가 상세 |
| B15 | `RECOVERING` | `u8` · 0=아님; 1=복구 중 | 공통·추가 상세 |
| B16~19 | `UPDATE_SEQUENCE` | `u32` · 새 상태 평가 | 공통·추가 상세 |
| B20~21 | `SOURCE_AGE` | `u16` · 평가 후 경과 시간, ms | 공통·추가 상세 |
| B22~23 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-cis-environment"></a>
## `CIS_ENVIRONMENT` — `0x300`

**CIS → DOMAIN · 40 B 사용 / 48 B CAN 전송 / DLC 14 · WIRE_PROPOSED.**

**CIS 환경 수신 후 동기 전환 / 연속 Epoch 목표 수용 대기:** 기존 BOOT_ID와 센서별 원 Sequence로 A900→B0을 검증하며 새 전역번호로 이어 붙이지 않는다. 양쪽 정상 실행·초기 정합 구간에서 Network 자기 소유 구 환경 대기·콜백·미시작 Query 응답 차단/직렬 처리 → 단일 동기 private 요청 → DOMAIN 내부 구 쓰기 배제·VSM/Meta·관련 순서 문맥 무효화 → 현재 요청·적용 Epoch·명확한 완료 대조 → 새 원 관측을 기존 Rx로 전달하는 순서다. BOOT 무효화용 Rx NO_DATA는 중복 호출하지 않는다. 같은 BOOT의 센서 NO_DATA·만료·수신 중단은 기존 Rx·품질·시간 정책을 유지하며 BOOT/Sequence 리셋이나 새 Epoch 사건이 아니다. 첫1/완료 후 다음+1은 목표이며 현재 Core의 다른 Epoch 거부 보고·실제 연결 미검증을 구분하는 [완료 경계](../02_NODES/DOMAIN/20_API.md#delta-environment-completion)를 따른다. 실패·완료 불명은 환경 차단·오류 보고 대상으로 자동 재시도하지 않는다. BOOT 재사용·단독 재시작·기억 상실의 자동 복구는 보장하지 않는다. 아래 ID·길이·필드 바이트와 공개 C2는 유지한다.

의미 타입 `NetCisEnvironment`: [CIS Data](../02_NODES/CIS/10_DATA.md) · [CIS API](../02_NODES/CIS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~9 | `TEMPERATURE` | `i16` · 0.01°C/raw; -4000~12500; -32768=무효 | C2D-003 |
| B10~13 | `TEMPERATURE_SEQUENCE` | `u32` · 해당 값의 새 측정 번호 | C2D-014 |
| B14~15 | `TEMPERATURE_AGE` | `u16` · 해당 값 원본 측정 후 ms | C2D-013 |
| B16 | `TEMPERATURE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B17 | `TEMPERATURE_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B18~19 | `HUMIDITY` | `u16` · 0.01%RH/raw; 0~10000; 65535=무효 | C2D-004 |
| B20~23 | `HUMIDITY_SEQUENCE` | `u32` · 해당 값의 새 측정 번호 | C2D-014 |
| B24~25 | `HUMIDITY_AGE` | `u16` · 해당 값 원본 측정 후 ms | C2D-013 |
| B26 | `HUMIDITY_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B27 | `HUMIDITY_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B28~31 | `ILLUMINANCE` | `u32` · 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 | C2D-005 |
| B32~35 | `ILLUMINANCE_SEQUENCE` | `u32` · 해당 값의 새 측정 번호 | C2D-014 |
| B36~37 | `ILLUMINANCE_AGE` | `u16` · 해당 값 원본 측정 후 ms | C2D-013 |
| B38 | `ILLUMINANCE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B39 | `ILLUMINANCE_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B40~47 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-cis-occupant"></a>
## `CIS_OCCUPANT` — `0x310`

**CIS → DOMAIN · 25 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetCisOccupant`: [CIS Data](../02_NODES/CIS/10_DATA.md) · [CIS API](../02_NODES/CIS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~11 | `PI_BOOT_ID` | `u32` · Pi 원본 기동 번호 | C2D-013 |
| B12~15 | `VISION_SEQUENCE` | `u32` · Pi 새 판정 번호, 중계 시 보존 | C2D-014 |
| B16~17 | `VISION_AGE` | `u16` · Pi 원본 판정 후 누적 ms | C2D-013 |
| B18 | `OCCUPANT_PRESENCE` | `u8` · 0=없음; 1=있음; 255=확인 불가 | C2D-001 |
| B19 | `PRESENCE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B20 | `PRESENCE_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B21 | `OCCUPANT_COUNT` | `u8` · 0~5명,255=확인 불가 | C2D-002 |
| B22 | `COUNT_VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B23 | `COUNT_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B24 | `VISION_FUNCTION_STATUS` | `u8` · CIS 기능 상태 | C2D-009 |
| B25~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-cis-rear"></a>
## `CIS_REAR` — `0x0A0`

후방 거리 원 관측 사용 한도는 `CIS_REAR` 150ms 후보이며 Permission 원 500ms와 별개다. SysRS 후방 거리 최대 500ms 잠정 기준보다 이르게 사용을 제한하지만 실제 센서·전송·소비 주기에서의 가용성은 미검증이다. 이 프로필에서는 누적 AGE 150ms에 도달하면 거리값을 사용하지 않으며 149ms에서도 품질·상태·수신 감시 조건을 만족해야 한다. 실제 DOMAIN 소비 기준 변경에는 해당 거리 한도와 가용성 검증이 필요하다. [시간 규칙](../07_CROSS_NODE_INTERFACE_RULES.md#timing)

**CIS → DOMAIN · 19 B 사용 / 20 B CAN 전송 / DLC 11 · WIRE_PROPOSED.**

의미 타입 `NetCisRear`: [CIS Data](../02_NODES/CIS/10_DATA.md) · [CIS API](../02_NODES/CIS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~11 | `UPDATE_SEQUENCE` | `u32` · 새 거리 관측 번호 | C2D-014 |
| B12~13 | `SOURCE_AGE` | `u16` · 거리 원본 관측 후 ms | C2D-013 |
| B14~15 | `REAR_DISTANCE` | `u16` · 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분 | C2D-006 |
| B16 | `VALIDITY` | `u8` · 0=INVALID; 1=VALID | C2D-011 |
| B17 | `QUALITY_REASON` | `u8` · CIS 품질 사유 | C2D-012 |
| B18 | `PROXIMITY_STATUS` | `u8` · 0=VALID_DISTANCE; 1=NO_OBJECT; 2=UNAVAILABLE; 3=FAULT; 4=RECOVERING; 5=INACTIVE | C2D-007 |
| B19~19 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-cis-status"></a>
## `CIS_STATUS` — `0x320`

**CIS → DOMAIN · 34 B 사용 / 48 B CAN 전송 / DLC 14 · WIRE_PROPOSED.**

의미 타입 `NetCisStatus`: [CIS Data](../02_NODES/CIS/10_DATA.md) · [CIS API](../02_NODES/CIS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `CIS_STATE` | `u8` · 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT | C2D-008 |
| B9 | `VISION_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B10 | `TEMPERATURE_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B11 | `HUMIDITY_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B12 | `ILLUMINANCE_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B13 | `REAR_STATUS` | `u8` · 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | C2D-009 |
| B14 | `PI_UART_STATUS` | `u8` · 0=READY; 1=UNAVAILABLE; 2=RECOVERING | C2D-010 |
| B15 | `CAN_INTERFACE_STATUS` | `u8` · 0=READY; 1=UNAVAILABLE; 2=RECOVERING | C2D-010 |
| B16~17 | `FAULT_MASK` | `u16` · CIS 고장 비트 0~8 | C2D-015;  C2D-016;  C2D-017; C2D-018;  C2D-019;  C2D-020; C2D-021;  C2D-022;  C2D-023 |
| B18~19 | `ACTIVE_FAULT` | `u16` · 현재 주요 고장 코드 | C2D-024 |
| B20 | `RECOVERING` | `u8` · 0=아님; 1=복구 중 | C2D-025 |
| B21~22 | `LAST_FAULT` | `u16` · 이번 기동 이후 최근 주요 고장 코드 | C2D-026 |
| B23~26 | `LAST_FAULT_OCCURRENCE` | `u32` · 최근 고장 발생 번호 | C2D-026 |
| B27 | `AFFECTED_FUNCTION` | `u8` · bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 | C2D-027 |
| B28~31 | `UPDATE_SEQUENCE` | `u32` · 이번 상태 평가 번호 | 공통·추가 상세 |
| B32~33 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |
| B34~47 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-window-state"></a>
## `WINDOW_STATE` — `0x0B0`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**WINDOW → DOMAIN · 22 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetWindowState`: [WINDOW Data](../02_NODES/WINDOW/10_DATA.md) · [WINDOW API](../02_NODES/WINDOW/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `MOTION_STATE` | `u8` · 0=STOPPED; 1=OPENING; 2=CLOSING; 3=ANTIPINCH_REVERSING; 255=UNKNOWN | W2D-002 |
| B9 | `WINDOW_ECU_STATE` | `u8` · 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | W2D-003 |
| B10 | `WINDOW_POSITION` | `u8` · 0~100% 닫힘 비율,255=확인 불가 | W2D-004 |
| B11 | `WINDOW_VALUE_QUALITY` | `u8` · VALUE_QUALITY | W2D-005 |
| B12 | `FULLY_OPEN_STATE` | `u8` · 0=아님; 1=완전 열림; 255=확인 불가 | W2D-006 |
| B13 | `FULLY_CLOSED_STATE` | `u8` · 0=아님; 1=완전 닫힘; 255=확인 불가 | W2D-007 |
| B14 | `ANTIPINCH_PROTECTION_STATUS` | `u8` · 0=대기; 1=진행; 2=완료; 3=중단 | W2D-011 |
| B15 | `REVERSE_STATE` | `u8` · 0=역전 아님; 1=역전 중; 255=확인 불가 | W2D-011 |
| B16~19 | `UPDATE_SEQUENCE` | `u32` · 새 상태 평가 | 공통·추가 상세 |
| B20~21 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |
| B22~23 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-window-event"></a>
## `WINDOW_EVENT` — `0x091`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**WINDOW → DOMAIN · 26 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetWindowEvent`: [WINDOW Data](../02_NODES/WINDOW/10_DATA.md) · [WINDOW API](../02_NODES/WINDOW/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

REQUEST_SEQUENCE / RELATED_REQUEST_SEQUENCE ↔ C2 commandId는 PROPOSED/OPEN이다. 원 ID echo가 확정된 것으로 표시하지 않는다. END 사건과 AntiPinch 사건, 로컬 occurrence와 상위 command를 분리한다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `EVENT_KIND` | `u8` · 1=FULLY_OPENED; 2=FULLY_CLOSED; 3=WINDOW_ANTIPINCH | W2D-008;  W2D-009;  W2D-010 |
| B9 | `TARGET_CHANNEL` | `u8` · 현재 0 | 공통·추가 상세 |
| B10~13 | `OCCURRENCE_ID` | `u32` · 새 전이/끼임 발생 번호 | 공통·추가 상세 |
| B14~17 | `RELATED_DOMAIN_BOOT` | `u32` · 연결 명령의 Domain 기동 번호; 로컬은 0 | 공통·추가 상세 |
| B18~21 | `RELATED_REQUEST_SEQUENCE` | `u32` · Domain 명령 연계; 로컬 발생은 0 | 공통·추가 상세 |
| B22 | `ORIGIN` | `u8` · 요청 출처 코드 | 공통·추가 상세 |
| B23~24 | `EVENT_AGE` | `u16` · 발생 후 ms | 공통·추가 상세 |
| B25 | `EVENT_QUALITY` | `u8` · VALUE_QUALITY | 공통·추가 상세 |
| B26~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-window-result"></a>
## `WINDOW_RESULT` — `0x092`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**WINDOW → DOMAIN · 20 B 사용 / 20 B CAN 전송 / DLC 11 · WIRE_PROPOSED.**

의미 타입 `NetWindowResult`: [WINDOW Data](../02_NODES/WINDOW/10_DATA.md) · [WINDOW API](../02_NODES/WINDOW/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

REQUEST_SEQUENCE / RELATED_REQUEST_SEQUENCE ↔ C2 commandId는 PROPOSED/OPEN이다. 원 ID echo가 확정된 것으로 표시하지 않는다. END 사건과 AntiPinch 사건, 로컬 occurrence와 상위 command를 분리한다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~11 | `REQUEST_DOMAIN_BOOT` | `u32` · 원 명령을 생성한 Domain 기동 번호 | 공통·추가 상세 |
| B12~15 | `REQUEST_SEQUENCE` | `u32` · 원 Domain 명령 식별 | D2W-003 |
| B16 | `TARGET_CHANNEL` | `u8` · 현재 0 | 공통·추가 상세 |
| B17 | `RESULT` | `u8` · 공통 차량 Result 코드 | W2D-012 |
| B18~19 | `RESULT_REASON` | `u16` · 공통 사유 코드 | W2D-013 |


<a id="message-window-fault"></a>
## `WINDOW_FAULT` — `0x330`

**[구현 보류 — 설계 유지]**. 실제 송수신·감시는 활성화 조건 확정 전 보류한다.

**WINDOW → DOMAIN · 22 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetWindowFault`: [WINDOW Data](../02_NODES/WINDOW/10_DATA.md) · [WINDOW API](../02_NODES/WINDOW/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `FAULT_CATEGORY` | `u8` · 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM | W2D-014 |
| B9~10 | `FAULT_CODE` | `u16` · WINDOW 고장 코드 | W2D-014 |
| B11 | `FAULT_STATUS` | `u8` · 0=없음; 1=현재 고장; 2=복구 중 | W2D-014 |
| B12~15 | `OCCURRENCE_ID` | `u32` · 현재/최근 고장 연결 | 공통·추가 상세 |
| B16~19 | `UPDATE_SEQUENCE` | `u32` · 상태 평가 번호 | 공통·추가 상세 |
| B20~21 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |
| B22~23 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-vss-event"></a>
## `VSS_EVENT` — `0x160`

**DOMAIN → VSS · 23 B 사용 / 24 B CAN 전송 / DLC 12 · WIRE_PROPOSED.**

의미 타입 `NetVssEvent`: [VSS Data](../02_NODES/VSS/10_DATA.md) · [VSS API](../02_NODES/VSS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

A1 Event 최초 actual은 검증된 양의 USE_LIMIT·설계 프로필500ms·상한2000ms 중 최소 원 기한 L을 적용하며0은 비사용, L 도달부터 만료다. RX 기준 AGE/후속 cue 구분과 실제 시간 검증 대기는 [VSS 입력 정본](../02_NODES/VSS/10_DATA.md#a1-binding)을 따른다.
| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `EVENT_KIND` | `u8` · 1=WELCOME; 2=GOODBYE; 3=LOCK_COMPLETE; 4=UNLOCK_COMPLETE; 5=LOCK_ERROR | D2V-001;  D2V-002;  D2V-003; D2V-004;  D2V-005 |
| B9~12 | `OCCURRENCE_ID` | `u32` · Domain 기동 내 의미 이벤트 번호 | 공통·추가 상세 |
| B13~14 | `ORIGINAL_AGE` | `u16` · 이벤트 원본 발생 후 누적 ms | D2V-010 |
| B15~16 | `USE_LIMIT` | `u16` · 일회성 이벤트 사용 한도; 초안 500ms | D2V-010 |
| B17 | `VALUE_QUALITY` | `u8` · VALUE_QUALITY | D2V-010 |
| B18 | `INPUT_AVAILABILITY` | `u8` · 0=가용; 1=제한; 2=불가 | D2V-010 |
| B19~22 | `RELATED_COMMAND_ID` | `u32` · 도어 명령 등 연계; 없으면 0 | 공통·추가 상세 |
| B23~23 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-vss-warning-state"></a>
## `VSS_WARNING_STATE` — `0x0C0`

**DOMAIN → VSS · 60 B 사용 / 64 B CAN 전송 / DLC 15 · WIRE_PROPOSED.**

의미 타입 `NetVssWarningState`: [VSS Data](../02_NODES/VSS/10_DATA.md) · [VSS API](../02_NODES/VSS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

A1은 WARNING60(used60/frame64/DLC15)을 단일 설계로 선택했다. WARNING51은 이력이며 DLC/B51로 자동 판별하거나 혼용하지 않는다. format1은 B52..59의 독립 활성 meta를 검증하고 format0은 B50=0 및 B52..59=0의 정규 근거 부재다. 그0을 활성 DISABLED/품질OK 판단으로 소비하지 않고 다른 세 경고는 독립 검증한다. 그 외 marker·비정규 부재는 거부한다. 실제 DOMAIN producer/VSS receiver·ABI·시간 검증은 대기다. [A1 입력·시간](../02_NODES/VSS/10_DATA.md#a1-binding).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8~11 | `ANTIPINCH_SOURCE_BOOT` | `u32` · 원 판단 근거 생성 노드 기동 번호 | D2V-010 |
| B12~15 | `ANTIPINCH_SOURCE_SEQUENCE` | `u32` · 새 유효 판단 근거 번호 | D2V-010 |
| B16~17 | `ANTIPINCH_ORIGINAL_AGE` | `u16` · 원 판단 근거 관측 후 누적 ms | D2V-010 |
| B18 | `ANTIPINCH_STATE` | `u8` · 0=CLEAR; 1=ACTIVE | D2V-006 |
| B19 | `ANTIPINCH_QUALITY` | `u8` · VALUE_QUALITY | D2V-010 |
| B20 | `ANTIPINCH_REASON` | `u8` · 품질 사유 | D2V-010 |
| B21 | `ANTIPINCH_AVAILABILITY` | `u8` · 0=가용; 1=제한; 2=불가 | D2V-010 |
| B22~25 | `OCCUPANT_HAZARD_SOURCE_BOOT` | `u32` · 원 판단 근거 생성 노드 기동 번호 | D2V-010 |
| B26~29 | `OCCUPANT_HAZARD_SOURCE_SEQUENCE` | `u32` · 새 유효 판단 근거 번호 | D2V-010 |
| B30~31 | `OCCUPANT_HAZARD_ORIGINAL_AGE` | `u16` · 원 판단 근거 관측 후 누적 ms | D2V-010 |
| B32 | `OCCUPANT_HAZARD_STATE` | `u8` · 0=CLEAR; 1=ACTIVE | D2V-007 |
| B33 | `OCCUPANT_HAZARD_QUALITY` | `u8` · VALUE_QUALITY | D2V-010 |
| B34 | `OCCUPANT_HAZARD_REASON` | `u8` · 품질 사유 | D2V-010 |
| B35 | `OCCUPANT_HAZARD_AVAILABILITY` | `u8` · 0=가용; 1=제한; 2=불가 | D2V-010 |
| B36~39 | `REAR_OBSTACLE_SOURCE_BOOT` | `u32` · 원 판단 근거 생성 노드 기동 번호 | D2V-010 |
| B40~43 | `REAR_OBSTACLE_SOURCE_SEQUENCE` | `u32` · 새 유효 판단 근거 번호 | D2V-010 |
| B44~45 | `REAR_OBSTACLE_ORIGINAL_AGE` | `u16` · 원 판단 근거 관측 후 누적 ms | D2V-010 |
| B46 | `REAR_OBSTACLE_STATE` | `u8` · 0=CLEAR; 1=CAUTION; 2=EMERGENCY | D2V-008 |
| B47 | `REAR_OBSTACLE_QUALITY` | `u8` · VALUE_QUALITY | D2V-010 |
| B48 | `REAR_OBSTACLE_REASON` | `u8` · 품질 사유 | D2V-010 |
| B49 | `REAR_OBSTACLE_AVAILABILITY` | `u8` · 0=가용; 1=제한; 2=불가 | D2V-010 |
| B50 | `REAR_DETECTION_ACTIVATION` | `u8` · 0=DISABLED; 1=ACTIVE | D2V-009 |
| B51 | `ACTIVATION_META_FORMAT` | `u8` · 1=이번 activation metadata 형식; 0=legacy padding/근거 없음; 그 외=미지원 | 추가 상세·RC-02 |
| B52 | `REAR_ACTIVATION_QUALITY` | `u8` · VALUE_QUALITY. activation 평가의 품질 | D2V-009·RC-02 |
| B53 | `REAR_ACTIVATION_REASON` | `u8` · 기존 품질 사유. OK이면 NONE | D2V-009·RC-02 |
| B54~57 | `REAR_ACTIVATION_SEQUENCE` | `u32` · Domain의 새 기능 활성/허용 평가 번호 | 추가 상세·RC-02 |
| B58~59 | `REAR_ACTIVATION_AGE` | `u16` · 동일 Domain 활성 평가 후 누적 ms | 추가 상세·RC-02 |
| B60~63 | PADDING | 0. 의미 해석하지 않음 | 없음 |


<a id="message-vss-status"></a>
## `VSS_STATUS` — `0x340`

**VSS → DOMAIN · 28 B 사용 / 32 B CAN 전송 / DLC 13 · WIRE_PROPOSED.**

의미 타입 `NetVssStatus`: [VSS Data](../02_NODES/VSS/10_DATA.md) · [VSS API](../02_NODES/VSS/20_API.md) · [DOMAIN API](../02_NODES/DOMAIN/20_API.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).

**A2 내부 선택 / 실제 Binding 대기 — 완전 원 보고:** HEALTH CREATED의 보호된 완전 Snapshot→FLOW→기존 INPUT 한 곳의 11필드 명시 변환→`VssNet_PublishStatus()`→Network 전체 사본 순서다. [VSS 생산·수용 정본](../02_NODES/VSS/10_DATA.md#r2-status-proposal)을 따른다. REEVALUATE는 새 평가가 아니고 state/availability/accepting은 각각 원 근거가 필요하다. 현재 kind만 기존 bit0~4에 대응하고 최근 이력은 원 occurrence/발생 AGE를 보존한다. mask0/최근 고장으로 READY/FULL/current clear를 만들지 않는다. 실제 producer·원 세대·시간·terminal sink·C2는 E-S01~05 대기다.

LAST_FAULT_QUALITY는 기존 기록0/확인된 부재3만 표현한다. C2/로컬 전체 fault INVALID2를 B21=2 또는 이력0/3으로 축약하지 않는다. mandatory current 상태·품질·원 평가 메타가 미확인인 새 Status 투영만 제한하고 독립 Event/Warning의 유효 원 근거를 보존한다. 원 generation과 송신 CAN BOOT의 lifecycle 대응, 완전성 및 최근 주요 기록 선택은 `CROSS_NODE_AGREEMENT`/`FUNCTION_OWNER`, 실제 변환·복사·실패 관측은 `IMPLEMENTATION_BINDING`으로 확인한다. 새 raw/DTO/API 없이 제안한 상태이며 AGREED/IMPLEMENTED/TESTED 증거는 없다.


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~7 | `BOOT_ID`, `TX_SEQUENCE` | u32 LE 두 개. 전송자 BOOT·TX 순서; 관측 근거와 구분 | CAN 공통 송신 헤더(BOOT_ID·TX_SEQUENCE) |
| B8 | `VSS_STATE` | `u8` · 0=STARTUP; 1=READY; 2=PLAYING; 3=FAULT | V2D-001 |
| B9 | `VSS_AVAILABILITY` | `u8` · 0=FULL; 1=DEGRADED; 2=UNAVAILABLE | V2D-002 |
| B10 | `VSS_ACCEPTING_EVENTS` | `u8` · 0=불가; 1=수용 가능 | V2D-003 |
| B11 | `VSS_FAULT_ACTIVE` | `u8` · 0=없음; 1=현재 고장 있음 | V2D-004 |
| B12 | `FAULT_MASK` | `u8` · VSS 고장 비트 0~4 | V2D-006;  V2D-007;  V2D-008; V2D-009;  V2D-010 |
| B13~14 | `VSS_LAST_FAULT` | `u16` · 이번 기동 이후 최근 주요 내부 출력 고장 | V2D-005 |
| B15~18 | `LAST_FAULT_OCCURRENCE` | `u32` · 최근 고장 발생 번호 | V2D-005 |
| B19~20 | `LAST_FAULT_AGE` | `u16` · 최근 고장 발생 후 ms; 포화 허용 | V2D-005 |
| B21 | `LAST_FAULT_QUALITY` | `u8` · 0=기록 있음; 3=NO_DATA | V2D-005 |
| B22~25 | `UPDATE_SEQUENCE` | `u32` · 이번 상태 평가 | 공통·추가 상세 |
| B26~27 | `SOURCE_AGE` | `u16` · 평가 후 ms | 공통·추가 상세 |
| B28~31 | PADDING | 0. 의미 해석하지 않음 | 없음 |
