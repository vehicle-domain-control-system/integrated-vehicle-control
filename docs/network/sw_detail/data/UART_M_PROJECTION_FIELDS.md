# UART-M CAN 원본 투영 11종 — 필드 정본

이 문서는 S4C/B08 v0.2의 `DOWN_CONTEXT 20B + 원 CAN used bytes` 후보를 모든 byte별로 기록한다. C 선언은 [UART-M 자료 정본](UART_M_DATA.md)과 기존 CAN 자료에 한 번만 둔다. 본 표는 새 Wire/헤더 또는 승인된 양단 구현이 아니다. 다중 byte는 LE, i16은 2의 보수이며 payload B0은 frame B8이다. CAN padding, 구조체 padding, 새 sourceGeneration 필드를 덧붙이지 않는다.

[전체 UART-M](../05_UART_M_OVERVIEW.md) · [원본 연결 흐름](../flows/UART_M_CAN_PROJECTION.md) · [공통 Frame](../functions/UART_FRAME_STREAM.md)

20B 문맥의 현재 DEVICE/SESSION/DOMAIN_BOOT/QUERY와 원 ECU Boot/TX, Pi Boot/VisionSequence는 각기 다른 identity다. `QUERY_ID=0`은 자발 출력이며 Session0의 의미를 임의로 예약·허용하지 않는다. 모든 AGE65535는 포화 하한으로 보존하고 정확한 상한이나 fresh 증거로 사용하지 않는다. 전송 대기 경과는 원 관측 AGE에 한 번만 누적한다. `NOT_SPECIFIED`는 자유로운 기본값 채택이 아니다.

<a id="m_bcm_door_state"></a>
## M_BCM_DOOR_STATE

`WIRE-P02-M_BCM_DOOR_STATE` / `RX-P02-M_BCM_DOOR_STATE-ESP32` · Type `0x30` / VERSION2 후보 · payload 40B / frame 50B. `20+20=40`.

[NetMobileBcmDoorState](UART_M_DATA.md#type-NetMobileBcmDoorState) · [메시지 함수](../functions/uart_m/M_BCM_DOOR_STATE.md) · [원 CAN](../functions/can/BCM_DOOR_STATE.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:391 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:391 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:391 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:391 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:392; T33:175 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:392; T33:175 +20 |
| 28 / 1 | `DOOR_LOCK_STATE` → `state.door_lock_state` | u8 / unsigned / N/A | 0=NET_DOOR_LOCK_LOCKED; 1=NET_DOOR_LOCK_UNLOCKED; 255=NET_DOOR_LOCK_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN; T34:393; T33:176 +20 |
| 29 / 1 | `DOOR_OPEN_STATE` → `state.door_open_state` | u8 / unsigned / N/A | 0=NET_DOOR_OPEN_CLOSED; 1=NET_DOOR_OPEN_OPEN; 255=NET_DOOR_OPEN_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN; T34:394; T33:177 +20 |
| 30 / 1 | `DOOR_COMPOSITE_STATE` → `state.door_composite_state` | u8 / unsigned / N/A | 0=NET_DOOR_COMPOSITE_NORMAL; 1=NET_DOOR_COMPOSITE_INCONSISTENT; 2=NET_DOOR_COMPOSITE_UNTRUSTED; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:395; T33:178 +20 |
| 31 / 1 | `LOCK_QUALITY` → `state.lock_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:396; T33:179 +20 |
| 32 / 1 | `OPEN_QUALITY` → `state.open_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:397; T33:180 +20 |
| 33 / 1 | `COMPOSITE_QUALITY` → `state.composite_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:398; T33:181 +20 |
| 34 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 이번 도어 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:399; T33:182 +20 |
| 38 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 경과 시간, ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:400; T33:183 +20 |

잠금·열림·composite와 세 품질을 독립 보존한다. UNKNOWN raw255를 잠김/닫힘으로 만들지 않는다. C2 DoorState_t의 sourceGeneration 후보가 원 BCM Boot와 대응한다는 증거 및 meta summary 품질 원 평가가 필요하다. DoorState 보고는 특정 Unlock 명령의 confirmation이 아니다.

<a id="m_bcm_climate_state"></a>
## M_BCM_CLIMATE_STATE

`WIRE-P02-M_BCM_CLIMATE_STATE` / `RX-P02-M_BCM_CLIMATE_STATE-ESP32` · Type `0x31` / VERSION2 후보 · payload 44B / frame 54B. `20+24=44`.

[NetMobileBcmClimateState](UART_M_DATA.md#type-NetMobileBcmClimateState) · [메시지 함수](../functions/uart_m/M_BCM_CLIMATE_STATE.md) · [원 CAN](../functions/can/BCM_CLIMATE_STATE.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:413 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:413 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:413 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:413 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:414; T33:196 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:414; T33:196 +20 |
| 28 / 1 | `FAN_COMMAND_LEVEL` → `state.fan_command_level` | u8 / unsigned / N/A | 0=NET_FAN_OFF; 1=NET_FAN_LOW; 2=NET_FAN_MEDIUM; 3=NET_FAN_HIGH; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:415; T33:197 +20 |
| 29 / 1 | `FAN_MEASURED_LEVEL` → `state.fan_measured_level` | u8 / unsigned / N/A | 0=NET_FAN_OFF; 1=NET_FAN_LOW; 2=NET_FAN_MEDIUM; 3=NET_FAN_HIGH; 255=NET_FAN_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN; T34:416; T33:198 +20 |
| 30 / 1 | `THERMAL_DIRECTION_STATE` → `state.thermal_direction_state` | u8 / unsigned / N/A | 0=NET_THERMAL_IDLE; 1=NET_THERMAL_COOL; 2=NET_THERMAL_HEAT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:417; T33:199 +20 |
| 31 / 1 | `THERMAL_OUTPUT_LEVEL` → `state.thermal_output_percent` | u8 / unsigned / N/A | 0~100%, 1%/raw; 단위=% (1 percent/raw) | NOT_SPECIFIED; T34:418; T33:200 +20 |
| 32 / 1 | `HEAT_REMOVAL_STATE` → `state.heat_removal_state` | u8 / unsigned / N/A | 0=NET_HEAT_REMOVAL_NORMAL; 1=NET_HEAT_REMOVAL_OVERHEAT_CUTOFF; 255=NET_HEAT_REMOVAL_UNAVAILABLE; 단위=NOT_SPECIFIED | 255=측정 불가; T34:419; T33:201 +20 |
| 33 / 1 | `FAN_COMMAND_QUALITY` → `state.fan_command_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:420; T33:202 +20 |
| 34 / 1 | `FAN_MEASURED_QUALITY` → `state.fan_measured_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:421; T33:203 +20 |
| 35 / 1 | `THERMAL_DIRECTION_QUALITY` → `state.thermal_direction_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:422; T33:204 +20 |
| 36 / 1 | `THERMAL_OUTPUT_QUALITY` → `state.thermal_output_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:423; T33:205 +20 |
| 37 / 1 | `HEAT_REMOVAL_QUALITY` → `state.heat_removal_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:424; T33:206 +20 |
| 38 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 이번 공조 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:425; T33:207 +20 |
| 42 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 경과 시간, ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:426; T33:208 +20 |

명령 fan은0..3, 측정 fan은0..3 또는255다. commanded255는 잘못된 형식이고 measured255는 UNKNOWN이다. direction/output/heatRemoval 및 다섯 품질을 함께 보존한다. ClimateState_t 후보의 sourceGeneration을 포함한 전체 C2 사본과 원 CAN 관측의 생산 세대 연결을 확인한다. 원 CAN Boot/TX는 같은 입력의 raw/meta에서만 얻으며 sourceGeneration을 Wire Boot로 치환하거나 payload에 덧붙이지 않는다. generation 0 수용 정책·실제 생산 세대 연결 미확정은 해당 투영의 NOT_READY 조건이다.

<a id="m_bcm_light_state"></a>
## M_BCM_LIGHT_STATE

`WIRE-P02-M_BCM_LIGHT_STATE` / `RX-P02-M_BCM_LIGHT_STATE-ESP32` · Type `0x32` / VERSION2 후보 · payload 45B / frame 55B. `20+25=45`.

[NetMobileBcmLightState](UART_M_DATA.md#type-NetMobileBcmLightState) · [메시지 함수](../functions/uart_m/M_BCM_LIGHT_STATE.md) · [원 CAN](../functions/can/BCM_LIGHT_STATE.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:439 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:439 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:439 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:439 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:440; T33:221 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:440; T33:221 +20 |
| 28 / 1 | `INTERIOR_LIGHT_TYPE_STATE` → `state.interior_light_type_state` | u8 / unsigned / N/A | 0=NET_LIGHT_NORMAL; 1=NET_LIGHT_GOODBYE; 2=NET_LIGHT_WARNING; 3=NET_LIGHT_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:441; T33:222 +20 |
| 29 / 1 | `INTERIOR_LIGHT_LEVEL_STATE` → `state.light_level_percent` | u8 / unsigned / N/A | 0~100%; 단위=% (1 percent/raw) | NOT_SPECIFIED; T34:442; T33:223 +20 |
| 30 / 3 | `INTERIOR_LIGHT_COLOR_STATE` → `state.interior_light_color_state` | rgb / unsigned / R/G/B consecutive | R/G/B 각각 0~255; 단위=R/G/B channel 0..255 | 무효 sentinel 없음; 각 채널의 255는 정상 값; T34:443; T33:224 +20 |
| 33 / 1 | `INTERIOR_LIGHT_APPLY_RESULT` → `state.interior_light_apply_result` | u8 / unsigned / N/A | 0=NET_LIGHT_APPLY_APPLIED; 1=NET_LIGHT_APPLY_FAILED; 255=NET_LIGHT_APPLY_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN; T34:444; T33:225 +20 |
| 34 / 1 | `LIGHT_PHYSICAL_FEEDBACK_CAPABILITY` → `state.light_physical_feedback_capability` | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:445; T33:226 +20 |
| 35 / 1 | `LIGHT_TYPE_QUALITY` → `state.light_type_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:446; T33:227 +20 |
| 36 / 1 | `LIGHT_LEVEL_QUALITY` → `state.light_level_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:447; T33:228 +20 |
| 37 / 1 | `LIGHT_COLOR_QUALITY` → `state.light_color_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:448; T33:229 +20 |
| 38 / 1 | `LIGHT_APPLY_QUALITY` → `state.light_apply_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:449; T33:230 +20 |
| 39 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 이번 조명 적용 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:450; T33:231 +20 |
| 43 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 경과 시간, ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:451; T33:232 +20 |

RGB 각 채널255는 정상값이다. apply result가 APPLIED여도 물리 발광 확인을 만들지 않는다. 현재 physical feedback 미지원0과 실제 지원 증거 없는 capability1을 구별한다. InteriorLightState_t 후보의 sourceGeneration을 포함한 전체 C2 사본과 원 CAN 관측의 생산 세대 연결을 확인한다. 원 CAN Boot/TX는 같은 입력의 raw/meta에서만 얻으며 sourceGeneration을 Wire Boot로 치환하거나 payload에 덧붙이지 않는다. generation 0 수용 정책·실제 생산 세대 연결 미확정은 해당 투영의 NOT_READY 조건이다.

<a id="m_bcm_status"></a>
## M_BCM_STATUS

`WIRE-P02-M_BCM_STATUS` / `RX-P02-M_BCM_STATUS-ESP32` · Type `0x33` / VERSION2 후보 · payload 42B / frame 52B. `20+22=42`.

[NetMobileBcmStatus](UART_M_DATA.md#type-NetMobileBcmStatus) · [메시지 함수](../functions/uart_m/M_BCM_STATUS.md) · [원 CAN](../functions/can/BCM_STATUS.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:464 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:464 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:464 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:464 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:465; T33:297 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:465; T33:297 +20 |
| 28 / 1 | `BCM_ECU_STATE` → `state.bcm_ecu_state` | u8 / unsigned / N/A | 0=NET_CONTROLLER_INIT; 1=NET_CONTROLLER_READY; 2=NET_CONTROLLER_DEGRADED; 3=NET_CONTROLLER_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:466; T33:298 +20 |
| 29 / 2 | `FAULT_MASK` → `state.fault_mask` | u16 / unsigned / LE | BCM 고장 비트 0~7; 1=NET_BCM_FAULT_DOOR_SENSOR_FAULT; 2=NET_BCM_FAULT_FAN_SENSOR_FAULT; 4=NET_BCM_FAULT_TEMPERATURE_SENSOR_FAULT; 8=NET_BCM_FAULT_OVERHEAT_FAULT; 16=NET_BCM_FAULT_FAN_FAULT; 32=NET_BCM_FAULT_LOCK_ACTUATOR_FAULT; 64=NET_BCM_FAULT_LIGHT_APPLY_FAULT; 128=NET_BCM_FAULT_COMM_TIMEOUT_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:467; T33:299 +20 |
| 31 / 1 | `ACTIVE_FAULT_CATEGORY` → `state.active_fault_category` | u8 / unsigned / N/A | 0=NET_FAULT_CATEGORY_NONE; 1=NET_FAULT_CATEGORY_SENSOR; 2=NET_FAULT_CATEGORY_FUNCTION; 3=NET_FAULT_CATEGORY_COMM; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:468; T33:300 +20 |
| 32 / 2 | `ACTIVE_FAULT_CODE` → `state.active_fault_code` | u16 / unsigned / LE | 0=NET_BCM_FAULT_CODE_NONE; 1=NET_BCM_FAULT_CODE_DOOR_SENSOR_FAULT; 2=NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT; 3=NET_BCM_FAULT_CODE_TEMPERATURE_SENSOR_FAULT; 4=NET_BCM_FAULT_CODE_OVERHEAT_FAULT; 5=NET_BCM_FAULT_CODE_FAN_FAULT; 6=NET_BCM_FAULT_CODE_LOCK_ACTUATOR_FAULT; 7=NET_BCM_FAULT_CODE_LIGHT_APPLY_FAULT; 8=NET_BCM_FAULT_CODE_COMM_TIMEOUT_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:469; T33:301 +20 |
| 34 / 1 | `AFFECTED_FUNCTION` → `state.affected_function` | u8 / unsigned / N/A | bit0=도어; bit1=공조; bit2=조명; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:470; T33:302 +20 |
| 35 / 1 | `RECOVERING` → `state.recovering` | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:471; T33:303 +20 |
| 36 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 새 상태 평가; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:472; T33:304 +20 |
| 40 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 경과 시간, ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:473; T33:305 +20 |

FAULT_MASK는 bit0..7의 OR이며 enum 하나를 고르는 값이 아니다. affected function은0..7만 허용한다. 원 mask와 active category/code, recovering은 서로 다른 사실이다. 주요고장 선택·summary quality를 C2 snapshot의 일부 필드로 역추정하지 않는다.

<a id="m_cis_environment"></a>
## M_CIS_ENVIRONMENT

`WIRE-P02-M_CIS_ENVIRONMENT` / `RX-P02-M_CIS_ENVIRONMENT-ESP32` · Type `0x34` / VERSION2 후보 · payload 60B / frame 70B. `20+40=60`.

[NetMobileCisEnvironment](UART_M_DATA.md#type-NetMobileCisEnvironment) · [메시지 함수](../functions/uart_m/M_CIS_ENVIRONMENT.md) · [원 CAN](../functions/can/CIS_ENVIRONMENT.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:486 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:486 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:486 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:486 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:487; T33:319 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:487; T33:319 +20 |
| 28 / 2 | `TEMPERATURE` → `state.temperature_centi_c` | i16 / signed / LE | 0.01°C/raw; -4000~12500; -32768=무효; 단위=0.01 degC/raw | -32768=무효; T34:488; T33:320 +20 |
| 30 / 4 | `TEMPERATURE_SEQUENCE` → `state.temperature_sequence` | u32 / unsigned / LE | 해당 값의 새 측정 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:489; T33:321 +20 |
| 34 / 2 | `TEMPERATURE_AGE` → `state.temperature_age_ms` | u16 / unsigned / LE | 해당 값 원본 측정 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:490; T33:322 +20 |
| 36 / 1 | `TEMPERATURE_VALIDITY` → `state.temperature_validity` | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:491; T33:323 +20 |
| 37 / 1 | `TEMPERATURE_REASON` → `state.temperature_reason` | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:492; T33:324 +20 |
| 38 / 2 | `HUMIDITY` → `state.humidity_centi_percent` | u16 / unsigned / LE | 0.01%RH/raw; 0~10000; 65535=무효; 단위=0.01 %RH/raw | 65535=무효; T34:493; T33:325 +20 |
| 40 / 4 | `HUMIDITY_SEQUENCE` → `state.humidity_sequence` | u32 / unsigned / LE | 해당 값의 새 측정 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:494; T33:326 +20 |
| 44 / 2 | `HUMIDITY_AGE` → `state.humidity_age_ms` | u16 / unsigned / LE | 해당 값 원본 측정 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:495; T33:327 +20 |
| 46 / 1 | `HUMIDITY_VALIDITY` → `state.humidity_validity` | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:496; T33:328 +20 |
| 47 / 1 | `HUMIDITY_REASON` → `state.humidity_reason` | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:497; T33:329 +20 |
| 48 / 4 | `ILLUMINANCE` → `state.illuminance_lux` | u32 / unsigned / LE | 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효; 단위=1 lx/raw | 0xFFFFFFFF=무효; T34:498; T33:330 +20 |
| 52 / 4 | `ILLUMINANCE_SEQUENCE` → `state.illuminance_sequence` | u32 / unsigned / LE | 해당 값의 새 측정 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:499; T33:331 +20 |
| 56 / 2 | `ILLUMINANCE_AGE` → `state.illuminance_age_ms` | u16 / unsigned / LE | 해당 값 원본 측정 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:500; T33:332 +20 |
| 58 / 1 | `ILLUMINANCE_VALIDITY` → `state.illuminance_validity` | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:501; T33:333 +20 |
| 59 / 1 | `ILLUMINANCE_REASON` → `state.illuminance_reason` | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:502; T33:334 +20 |

온도·습도·조도의 측정번호, AGE, validity/reason을 각각 보존한다. 정상 부호화된 온도 무효 보고만으로 유효한 습도·조도를 폐기하지 않는다. 기존 CisCabinEnvironmentState_t에 sourceGeneration이 없으며 공통/센서별 Generation 정책 IF-O20은 회신 대기다. 온도·습도의 원 사용기한은 각각2500ms이며 기한 도달부터 비사용이다. 조도 TTL은 별도 미결이다. 원 측정값·seq·시각은 보존한다. 로컬 만료·수신 중단·Boot 변경 후 현재 품질은 원 raw/meta와 [NetCisEnvironmentLocalEvent](CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)의 검증된 파생 관계 및 실제 Core/Getter 연결로만 투영한다. 관계나 기존 Wire 표현 근거가 없으면 해당 투영은 HOLD이며 과거 raw GOOD로 되돌리지 않는다. 새 CAN Boot/TX/seq를 만들지 않는다.

<a id="m_cis_occupant"></a>
## M_CIS_OCCUPANT

`WIRE-P02-M_CIS_OCCUPANT` / `RX-P02-M_CIS_OCCUPANT-ESP32` · Type `0x35` / VERSION2 후보 · payload 45B / frame 55B. `20+25=45`.

[NetMobileCisOccupant](UART_M_DATA.md#type-NetMobileCisOccupant) · [메시지 함수](../functions/uart_m/M_CIS_OCCUPANT.md) · [원 CAN](../functions/can/CIS_OCCUPANT.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:515 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:515 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:515 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:515 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:516; T33:348 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:516; T33:348 +20 |
| 28 / 4 | `PI_BOOT_ID` → `state.pi_boot_id` | u32 / unsigned / LE | Pi 원본 기동 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:517; T33:349 +20 |
| 32 / 4 | `VISION_SEQUENCE` → `state.vision_sequence` | u32 / unsigned / LE | Pi 새 판정 번호, 중계 시 보존; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:518; T33:350 +20 |
| 36 / 2 | `VISION_AGE` → `state.vision_age_ms` | u16 / unsigned / LE | Pi 원본 판정 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:519; T33:351 +20 |
| 38 / 1 | `OCCUPANT_PRESENCE` → `state.occupant_presence` | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가; T34:520; T33:352 +20 |
| 39 / 1 | `PRESENCE_VALIDITY` → `state.presence_validity` | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:521; T33:353 +20 |
| 40 / 1 | `PRESENCE_REASON` → `state.presence_reason` | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:522; T33:354 +20 |
| 41 / 1 | `OCCUPANT_COUNT` → `state.occupant_count` | u8 / unsigned / N/A | 0~5명,255=확인 불가; 단위=명 | 255=확인 불가; T34:523; T33:355 +20 |
| 42 / 1 | `COUNT_VALIDITY` → `state.count_validity` | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:524; T33:356 +20 |
| 43 / 1 | `COUNT_REASON` → `state.count_reason` | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:525; T33:357 +20 |
| 44 / 1 | `VISION_FUNCTION_STATUS` → `state.vision_function_status` | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:526; T33:358 +20 |

Pi BOOT_ID/VISION_SEQUENCE는 CIS CAN BOOT_ID/TX_SEQUENCE와 별개다. count0은 확인한0명, count255는 확인 불가다. presence255를 C2 bool false로 되돌리는 방식은 금지한다. 원 Pi 관측과 C2가 반영한 presence/count의 독립 현재 품질을 구분한다. 같은 원 Pi Boot/관측/값에 적법한 새 CAN TX와 실제 제한 근거가 결합되어 Core/Getter까지 반영됐다는 증거가 있어야 한다. 원 raw VALID를 복사해 최신 제한을 취소하지 않는다. 동일 입력·제한의 연결이 없으면 해당 투영은 HOLD다.

<a id="m_cis_rear"></a>
## M_CIS_REAR

`WIRE-P02-M_CIS_REAR` / `RX-P02-M_CIS_REAR-ESP32` · Type `0x36` / VERSION2 후보 · payload 39B / frame 49B. `20+19=39`.

[NetMobileCisRear](UART_M_DATA.md#type-NetMobileCisRear) · [메시지 함수](../functions/uart_m/M_CIS_REAR.md) · [원 CAN](../functions/can/CIS_REAR.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:539 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:539 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:539 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:539 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:540; T33:372 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:540; T33:372 +20 |
| 28 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 새 거리 관측 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:541; T33:373 +20 |
| 32 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 거리 원본 관측 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:542; T33:374 +20 |
| 34 / 2 | `REAR_DISTANCE` → `state.rear_distance_deci_cm` | u16 / unsigned / LE | 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분; 단위=0.1 cm/raw = 1 mm/raw | 65535=거리 없음/무효; T34:543; T33:375 +20 |
| 36 / 1 | `VALIDITY` → `state.validity` | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:544; T33:376 +20 |
| 37 / 1 | `QUALITY_REASON` → `state.quality_reason` | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:545; T33:377 +20 |
| 38 / 1 | `PROXIMITY_STATUS` → `state.proximity_status` | u8 / unsigned / N/A | 0=NET_PROXIMITY_VALID_DISTANCE; 1=NET_PROXIMITY_NO_OBJECT; 2=NET_PROXIMITY_UNAVAILABLE; 3=NET_PROXIMITY_FAULT; 4=NET_PROXIMITY_RECOVERING; 5=NET_PROXIMITY_INACTIVE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:546; T33:378 +20 |

REAR_DISTANCE raw0.1cm는1mm와 같으므로 raw123은 C2 rearDistanceMm123과 같은 정수다. 정수cm로 절삭하지 않는다. raw65535는 NO_OBJECT/무효 sentinel이며 거리65535mm로 표시하지 않는다. VALIDITY/QUALITY_REASON/PROXIMITY_STATUS와 원 UPDATE_SEQUENCE를 함께 보존한다. B11 수신·AGE·원본 정본은 수정하지 않는다.

<a id="m_cis_status"></a>
## M_CIS_STATUS

`WIRE-P02-M_CIS_STATUS` / `RX-P02-M_CIS_STATUS-ESP32` · Type `0x37` / VERSION2 후보 · payload 54B / frame 64B. `20+34=54`.

[NetMobileCisStatus](UART_M_DATA.md#type-NetMobileCisStatus) · [메시지 함수](../functions/uart_m/M_CIS_STATUS.md) · [원 CAN](../functions/can/CIS_STATUS.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:559 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:559 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:559 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:559 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:560; T33:392 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:560; T33:392 +20 |
| 28 / 1 | `CIS_STATE` → `state.cis_state` | u8 / unsigned / N/A | 0=NET_CIS_STATE_STARTUP; 1=NET_CIS_STATE_READY; 2=NET_CIS_STATE_ACTIVE; 3=NET_CIS_STATE_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:561; T33:393 +20 |
| 29 / 1 | `VISION_STATUS` → `state.vision_status` | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:562; T33:394 +20 |
| 30 / 1 | `TEMPERATURE_STATUS` → `state.temperature_status` | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:563; T33:395 +20 |
| 31 / 1 | `HUMIDITY_STATUS` → `state.humidity_status` | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:564; T33:396 +20 |
| 32 / 1 | `ILLUMINANCE_STATUS` → `state.illuminance_status` | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:565; T33:397 +20 |
| 33 / 1 | `REAR_STATUS` → `state.rear_status` | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:566; T33:398 +20 |
| 34 / 1 | `PI_UART_STATUS` → `state.pi_uart_status` | u8 / unsigned / N/A | 0=NET_INTERFACE_READY; 1=NET_INTERFACE_UNAVAILABLE; 2=NET_INTERFACE_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:567; T33:399 +20 |
| 35 / 1 | `CAN_INTERFACE_STATUS` → `state.can_interface_status` | u8 / unsigned / N/A | 0=NET_INTERFACE_READY; 1=NET_INTERFACE_UNAVAILABLE; 2=NET_INTERFACE_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:568; T33:400 +20 |
| 36 / 2 | `FAULT_MASK` → `state.fault_mask` | u16 / unsigned / LE | CIS 고장 비트 0~8; 1=NET_CIS_FAULT_INITIALIZATION_FAILURE; 2=NET_CIS_FAULT_VISION_FAULT; 4=NET_CIS_FAULT_TEMPERATURE_SENSOR_FAULT; 8=NET_CIS_FAULT_HUMIDITY_SENSOR_FAULT; 16=NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT; 32=NET_CIS_FAULT_PROXIMITY_SENSOR_FAULT; 64=NET_CIS_FAULT_COMMUNICATION_FAULT; 128=NET_CIS_FAULT_DATA_INVALID; 256=NET_CIS_FAULT_OUT_OF_RANGE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:569; T33:401 +20 |
| 38 / 2 | `ACTIVE_FAULT` → `state.active_fault` | u16 / unsigned / LE | 0=NET_CIS_FAULT_CODE_NONE; 1=NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE; 2=NET_CIS_FAULT_CODE_VISION_FAULT; 3=NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT; 4=NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT; 5=NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT; 6=NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT; 7=NET_CIS_FAULT_CODE_COMMUNICATION_FAULT; 8=NET_CIS_FAULT_CODE_DATA_INVALID; 9=NET_CIS_FAULT_CODE_OUT_OF_RANGE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:570; T33:402 +20 |
| 40 / 1 | `RECOVERING` → `state.recovering` | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:571; T33:403 +20 |
| 41 / 2 | `LAST_FAULT` → `state.last_fault` | u16 / unsigned / LE | 0=NET_CIS_FAULT_CODE_NONE; 1=NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE; 2=NET_CIS_FAULT_CODE_VISION_FAULT; 3=NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT; 4=NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT; 5=NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT; 6=NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT; 7=NET_CIS_FAULT_CODE_COMMUNICATION_FAULT; 8=NET_CIS_FAULT_CODE_DATA_INVALID; 9=NET_CIS_FAULT_CODE_OUT_OF_RANGE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:572; T33:404 +20 |
| 43 / 4 | `LAST_FAULT_OCCURRENCE` → `state.last_fault_occurrence` | u32 / unsigned / LE | 최근 고장 발생 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:573; T33:405 +20 |
| 47 / 1 | `AFFECTED_FUNCTION` → `state.affected_function` | u8 / unsigned / N/A | bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:574; T33:406 +20 |
| 48 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 이번 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:575; T33:407 +20 |
| 52 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:576; T33:408 +20 |

센서 상태0..4와 interface 상태0..2를 구별한다. C2 InterfaceState_t의 UNAVAILABLE2/RECOVERING3 숫자를 raw에 직접 cast하지 않는다. CAN 수신 원복사에는 원 raw 상태가 남아 있어야 한다. fault mask bit0..8, active/last fault와 occurrence, affected function0..63을 따로 유지한다. CIS READY는 새 Pi 판정 또는 전 센서 정상의 증거가 아니다.

<a id="m_vss_status"></a>
## M_VSS_STATUS

`WIRE-P02-M_VSS_STATUS` / `RX-P02-M_VSS_STATUS-ESP32` · Type `0x3A` / VERSION2 후보 · payload 48B / frame 58B. `20+28=48`.

[NetMobileVssStatus](UART_M_DATA.md#type-NetMobileVssStatus) · [메시지 함수](../functions/uart_m/M_VSS_STATUS.md) · [원 CAN](../functions/can/VSS_STATUS.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:589 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:589 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:589 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:589 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:590; T33:586 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:590; T33:586 +20 |
| 28 / 1 | `VSS_STATE` → `state.vss_state` | u8 / unsigned / N/A | 0=NET_VSS_STATE_STARTUP; 1=NET_VSS_STATE_READY; 2=NET_VSS_STATE_PLAYING; 3=NET_VSS_STATE_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:591; T33:587 +20 |
| 29 / 1 | `VSS_AVAILABILITY` → `state.vss_availability` | u8 / unsigned / N/A | 0=NET_VSS_AVAILABILITY_FULL; 1=NET_VSS_AVAILABILITY_DEGRADED; 2=NET_VSS_AVAILABILITY_UNAVAILABLE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:592; T33:588 +20 |
| 30 / 1 | `VSS_ACCEPTING_EVENTS` → `state.vss_accepting_events` | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:593; T33:589 +20 |
| 31 / 1 | `VSS_FAULT_ACTIVE` → `state.vss_fault_active` | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:594; T33:590 +20 |
| 32 / 1 | `FAULT_MASK` → `state.fault_mask` | u8 / unsigned / N/A | VSS 고장 비트 0~4; 1=NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE; 2=NET_VSS_FAULT_PLAYBACK_START_FAILURE; 4=NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE; 8=NET_VSS_FAULT_PLAYBACK_STATE_FAILURE; 16=NET_VSS_FAULT_INITIALIZATION_FAILURE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:595; T33:591 +20 |
| 33 / 2 | `VSS_LAST_FAULT` → `state.vss_last_fault` | u16 / unsigned / LE | 0=NET_VSS_FAULT_CODE_NONE; 1=NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE; 2=NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE; 3=NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE; 4=NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE; 5=NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:596; T33:592 +20 |
| 35 / 4 | `LAST_FAULT_OCCURRENCE` → `state.last_fault_occurrence` | u32 / unsigned / LE | 최근 고장 발생 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:597; T33:593 +20 |
| 39 / 2 | `LAST_FAULT_AGE` → `state.last_fault_age_ms` | u16 / unsigned / LE | 최근 고장 발생 후 ms; 포화 허용; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:598; T33:594 +20 |
| 41 / 1 | `LAST_FAULT_QUALITY` → `state.last_fault_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:599; T33:595 +20 |
| 42 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 이번 상태 평가; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:600; T33:596 +20 |
| 46 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:601; T33:597 +20 |

LAST_FAULT_QUALITY는0(실제 이력)/3(확인된 부재)만 허용한다. INVALID2를0이나3으로 축약하지 않는다. 부재3은 code/occurrence/historyAGE가 모두0인 원 조합을 검증하고 그0을 유지하며, 실제 이력0만 원 발생 AGE를 누적한다. 현재 fault와 최근 이력, 수용가능성과 음향 상태는 별개다. 링크 상실을 AUDIO_OUTPUT_FAILURE로 만들지 않는다. State 수용과 Fault 결과는 독립이며 Fault 실패가 이미 수용한 State를 취소하지 않는다. 같은 accepted 원 입력과 Getter/TxPort의 association은 E-S05의 실제 증거가 별도로 필요하다. A1 WARNING60 형식 선택은 완료되었으나 실제 producer/양단 지원 검증은 별도다.

<a id="m_window_state"></a>
## M_WINDOW_STATE

`WIRE-P02-M_WINDOW_STATE` / `RX-P02-M_WINDOW_STATE-ESP32` · Type `0x38` / VERSION2 후보 · payload 42B / frame 52B. `20+22=42`.

[NetMobileWindowState](UART_M_DATA.md#type-NetMobileWindowState) · [메시지 함수](../functions/uart_m/M_WINDOW_STATE.md) · [원 CAN](../functions/can/WINDOW_STATE.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:616 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:616 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:616 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:616 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:617; T33:424 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:617; T33:424 +20 |
| 28 / 1 | `MOTION_STATE` → `state.motion_state` | u8 / unsigned / N/A | 0=NET_WINDOW_MOTION_STOPPED; 1=NET_WINDOW_MOTION_OPENING; 2=NET_WINDOW_MOTION_CLOSING; 3=NET_WINDOW_MOTION_ANTIPINCH_REVERSING; 255=NET_WINDOW_MOTION_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN; T34:618; T33:425 +20 |
| 29 / 1 | `WINDOW_ECU_STATE` → `state.window_ecu_state` | u8 / unsigned / N/A | 0=NET_CONTROLLER_INIT; 1=NET_CONTROLLER_READY; 2=NET_CONTROLLER_DEGRADED; 3=NET_CONTROLLER_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:619; T33:426 +20 |
| 30 / 1 | `WINDOW_POSITION` → `state.position_closed_percent` | u8 / unsigned / N/A | 0~100% 닫힘 비율,255=확인 불가; 단위=% (1 percent/raw) | 255=확인 불가; T34:620; T33:427 +20 |
| 31 / 1 | `WINDOW_VALUE_QUALITY` → `state.window_value_quality` | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:621; T33:428 +20 |
| 32 / 1 | `FULLY_OPEN_STATE` → `state.fully_open_state` | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가; T34:622; T33:429 +20 |
| 33 / 1 | `FULLY_CLOSED_STATE` → `state.fully_closed_state` | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가; T34:623; T33:430 +20 |
| 34 / 1 | `ANTIPINCH_PROTECTION_STATUS` → `state.antipinch_protection_status` | u8 / unsigned / N/A | 0=NET_ANTIPINCH_PROTECTION_IDLE; 1=NET_ANTIPINCH_PROTECTION_IN_PROGRESS; 2=NET_ANTIPINCH_PROTECTION_DONE; 3=NET_ANTIPINCH_PROTECTION_INTERRUPTED; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:624; T33:431 +20 |
| 35 / 1 | `REVERSE_STATE` → `state.reverse_state` | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가; T34:625; T33:432 +20 |
| 36 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 새 상태 평가; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:626; T33:433 +20 |
| 40 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:627; T33:434 +20 |

WINDOW_EXECUTION_HOLD다. 정상 후보 offset을 문서로 보존하지만 실제 송신·수신 소비·표시·timeout·liveness를 활성화하지 않는다. position255/tri-state255를0이나false로 바꾸지 않는다. 재기동/Permission 회복이 과거 이동 재개 또는 이 경로 활성화의 근거가 아니다.

<a id="m_window_fault"></a>
## M_WINDOW_FAULT

`WIRE-P02-M_WINDOW_FAULT` / `RX-P02-M_WINDOW_FAULT-ESP32` · Type `0x39` / VERSION2 후보 · payload 42B / frame 52B. `20+22=42`.

[NetMobileWindowFault](UART_M_DATA.md#type-NetMobileWindowFault) · [메시지 함수](../functions/uart_m/M_WINDOW_FAULT.md) · [원 CAN](../functions/can/WINDOW_FAULT.md)

| Payload offset / 폭 | Wire → typed 멤버 | Raw·부호·순서 | 의미·허용값·단위 | Sentinel·원 근거 |
|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말0예약; 단위=NOT_SPECIFIED | 0 예약; T34:642 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:642 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN기동0예약; 단위=NOT_SPECIFIED | 0 예약 (공통 기동 규칙); T34:642 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0; 단위=NOT_SPECIFIED | 0 자발 출력; T34:642 |
| 20 / 4 | `ORIGINAL_CAN_BOOT_ID` → `source.boot_id` | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약; T34:643; T33:498 +20 |
| 24 / 4 | `ORIGINAL_CAN_TX_SEQUENCE` → `source.tx_sequence` | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:643; T33:498 +20 |
| 28 / 1 | `FAULT_CATEGORY` → `state.fault_category` | u8 / unsigned / N/A | 0=NET_FAULT_CATEGORY_NONE; 1=NET_FAULT_CATEGORY_SENSOR; 2=NET_FAULT_CATEGORY_FUNCTION; 3=NET_FAULT_CATEGORY_COMM; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:644; T33:499 +20 |
| 29 / 2 | `FAULT_CODE` → `state.fault_code` | u16 / unsigned / LE | 0=NET_WINDOW_FAULT_NONE; 1=NET_WINDOW_FAULT_POSITION; 2=NET_WINDOW_FAULT_DRIVE; 3=NET_WINDOW_FAULT_ENDPOINT; 4=NET_WINDOW_FAULT_PROTECTION; 5=NET_WINDOW_FAULT_DOMAIN_TIMEOUT; 6=NET_WINDOW_FAULT_INITIALIZATION; 255=NET_WINDOW_FAULT_OTHER; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:645; T33:500 +20 |
| 31 / 1 | `FAULT_STATUS` → `state.fault_status` | u8 / unsigned / N/A | 0=NET_WINDOW_FAULT_STATE_NONE; 1=NET_WINDOW_FAULT_STATE_ACTIVE; 2=NET_WINDOW_FAULT_STATE_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:646; T33:501 +20 |
| 32 / 4 | `OCCURRENCE_ID` → `state.occurrence_id` | u32 / unsigned / LE | 현재/최근 고장 연결; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:647; T33:502 +20 |
| 36 / 4 | `UPDATE_SEQUENCE` → `state.update_sequence` | u32 / unsigned / LE | 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED; T34:648; T33:503 +20 |
| 40 / 2 | `SOURCE_AGE` → `state.source_age_ms` | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님; T34:649; T33:504 +20 |

WINDOW_EXECUTION_HOLD다. category, code, fault status, occurrence와 update를 독립 보존한다. fault code255는 원 NET_WINDOW_FAULT_OTHER 후보이며 UNKNOWN 또는 새 category로 바꾸지 않는다. 실제 송수신·표시·timeout·liveness는 보류하고 비활성 메시지의 미수신을 상시 고장으로 만들지 않는다.
