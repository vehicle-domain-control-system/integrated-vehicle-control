# UART-M DOMAIN 보고 6종의 payload 필드

[UART-M 개요](../05_UART_M_OVERVIEW.md) · [Data 선언](UART_M_DATA.md) · [Frame 자료](UART_FRAME_DATA.md) · [하향 흐름](../flows/UART_M_DOWNLINK.md)

모든 offset은 payload B0 기준이다. Frame에서는 +8, 총 길이는 P+10이다. 다중 byte LE, i16은 2의 보수, RGB는 R/G/B 순서다. 아래 6개 필드표는 B08 v0.2 `uart_m/domain_reports.md` 정본의 행을 그대로 보존한다. typed 선언을 중복 정의하지 않는다. 숫자 후보는 팀 채택이나 실제 Driver 시험 결과가 아니다.

<a id="m_result"></a>
## M_RESULT

`WIRE-P02-M_RESULT` / `RX-P02-M_RESULT-ESP32` · Type0x20, VERSION2 후보, P44/Frame54.

| Offset / 폭(B) | Wire → 선언된 typed 멤버 | Raw·부호·순서 | 코드/범위·단위 | sentinel/unknown | 근거 행 |
|---|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말; 0 예약 | 0 예약 | T34:238 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개 | 별도 특수값 정의 없음 | T34:238 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN 기동; 0 예약 | 0 예약 (공통 기동 규칙) | T34:238 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0 | 0 자발 출력 | T34:238 |
| 20 / 8 | `REQUEST_SESSION` → `request_session` | u64 / unsigned / LE | 원 요청 Session | 별도 특수값 정의 없음 | T34:239 |
| 28 / 4 | `REQUEST_ID` → `request_id` | u32 / unsigned / LE | 원 요청 ID | 별도 특수값 정의 없음 | T34:240 |
| 32 / 1 | `RESULT` → `result` | u8 / unsigned / N/A | 공통 차량 Result; 확인 기록 없으면 255=필드 없음; 허용 raw {0..5,255} | 255=필드 없음 | T34:241 |
| 33 / 2 | `RESULT_REASON` → `result_reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:242 |
| 35 / 1 | `RESULT_CONFIRMATION` → `result_confirmation` | u8 / unsigned / N/A | 0=UNCONFIRMED; 1=CONFIRMED; 허용 raw {0,1} | 별도 특수값 정의 없음 | T34:243 |
| 36 / 1 | `COMMAND_TARGET` → `command_target` | u8 / unsigned / N/A | 0=Domain 설정; 1=BCM; 2=WINDOW; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:244 |
| 37 / 4 | `COMMAND_ID` → `command_id` | u32 / unsigned / LE | 실행 명령 연계; 설정만 반영한 경우 0 | 별도 특수값 정의 없음 | T34:245 |
| 41 / 1 | `ORIGIN` → `origin` | u8 / unsigned / N/A | 요청 출처 코드; 허용 raw {0..5} | 별도 특수값 정의 없음 | T34:246 |
| 42 / 2 | `RESULT_AGE` → `result_age_ms` | u16 / unsigned / LE | 결과 확인 후 ms; 단위=ms | 65535 포화 하한; 새 관측/최신성 증거 아님 | T34:247 |

[메시지 함수·C2 변환](../functions/uart_m/M_RESULT.md) · [NetMobileResult](UART_M_DATA.md#type-NetMobileResult)

<a id="m_warning"></a>
## M_WARNING

`WIRE-P02-M_WARNING` / `RX-P02-M_WARNING-ESP32` · Type0x21, VERSION2 후보, P40/Frame50.

| Offset / 폭(B) | Wire → 선언된 typed 멤버 | Raw·부호·순서 | 코드/범위·단위 | sentinel/unknown | 근거 행 |
|---|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말; 0 예약 | 0 예약 | T34:260 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개 | 별도 특수값 정의 없음 | T34:260 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN 기동; 0 예약 | 0 예약 (공통 기동 규칙) | T34:260 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0 | 0 자발 출력 | T34:260 |
| 20 / 1 | `WARNING_TYPE` → `warning_type` | u8 / unsigned / N/A | 경고 종류; 허용 raw {1..8} | 별도 특수값 정의 없음 | T34:261 |
| 21 / 4 | `OCCURRENCE_ID` → `occurrence_id` | u32 / unsigned / LE | Domain 기동 내 경고 발생 번호 | 별도 특수값 정의 없음 | T34:262 |
| 25 / 1 | `SEVERITY` → `severity` | u8 / unsigned / N/A | 0=INFO; 1=CAUTION; 2=EMERGENCY; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:263 |
| 26 / 1 | `WARNING_STATE` → `warning_state` | u8 / unsigned / N/A | 0=CLEAR; 1=ACTIVE; 허용 raw {0,1} | 별도 특수값 정의 없음 | T34:264 |
| 27 / 1 | `READ_STATE` → `read_state` | u8 / unsigned / N/A | 0=UNREAD; 1=READ; 허용 raw {0,1} | 별도 특수값 정의 없음 | T34:265 |
| 28 / 1 | `VALUE_QUALITY` → `value_quality` | u8 / unsigned / N/A | VALUE_QUALITY; 허용 raw {0..3} | 별도 특수값 정의 없음 | T34:266 |
| 29 / 1 | `QUALITY_REASON` → `quality_reason` | u8 / unsigned / N/A | 품질 사유; 허용 raw {0..9} | 별도 특수값 정의 없음 | T34:267 |
| 30 / 4 | `SOURCE_BOOT` → `source_boot` | u32 / unsigned / LE | 원 관측 생성 노드 기동 번호 | 별도 특수값 정의 없음 | T34:268 |
| 34 / 4 | `SOURCE_SEQUENCE` → `source_sequence` | u32 / unsigned / LE | 원 관측 번호 | 별도 특수값 정의 없음 | T34:269 |
| 38 / 2 | `SOURCE_AGE` → `source_age_ms` | u16 / unsigned / LE | 원 관측 후 누적 ms; 단위=ms | 65535 포화 하한; 새 관측/최신성 증거 아님 | T34:270 |

[메시지 함수·C2 변환](../functions/uart_m/M_WARNING.md) · [NetMobileWarning](UART_M_DATA.md#type-NetMobileWarning)

<a id="m_availability"></a>
## M_AVAILABILITY

`WIRE-P02-M_AVAILABILITY` / `RX-P02-M_AVAILABILITY-ESP32` · Type0x22, VERSION2 후보, P62/Frame72.

| Offset / 폭(B) | Wire → 선언된 typed 멤버 | Raw·부호·순서 | 코드/범위·단위 | sentinel/unknown | 근거 행 |
|---|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말; 0 예약 | 0 예약 | T34:285 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개 | 별도 특수값 정의 없음 | T34:285 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN 기동; 0 예약 | 0 예약 (공통 기동 규칙) | T34:285 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0 | 0 자발 출력 | T34:285 |
| 20 / 1 | `FUNCTION_ID[0]` → `items[0].function_id` | u8 / unsigned / N/A | 1=DOOR; 허용 raw {1} | 별도 특수값 정의 없음 | T34:286 |
| 21 / 1 | `AVAILABILITY[0]` → `items[0].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 22 / 2 | `REASON[0]` → `items[0].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 24 / 1 | `FUNCTION_ID[1]` → `items[1].function_id` | u8 / unsigned / N/A | 2=CLIMATE; 허용 raw {2} | 별도 특수값 정의 없음 | T34:286 |
| 25 / 1 | `AVAILABILITY[1]` → `items[1].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 26 / 2 | `REASON[1]` → `items[1].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 28 / 1 | `FUNCTION_ID[2]` → `items[2].function_id` | u8 / unsigned / N/A | 3=INTERIOR_LIGHT; 허용 raw {3} | 별도 특수값 정의 없음 | T34:286 |
| 29 / 1 | `AVAILABILITY[2]` → `items[2].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 30 / 2 | `REASON[2]` → `items[2].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 32 / 1 | `FUNCTION_ID[3]` → `items[3].function_id` | u8 / unsigned / N/A | 4=DIGITAL_KEY; 허용 raw {4} | 별도 특수값 정의 없음 | T34:286 |
| 33 / 1 | `AVAILABILITY[3]` → `items[3].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 34 / 2 | `REASON[3]` → `items[3].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 36 / 1 | `FUNCTION_ID[4]` → `items[4].function_id` | u8 / unsigned / N/A | 5=OCCUPANT; 허용 raw {5} | 별도 특수값 정의 없음 | T34:286 |
| 37 / 1 | `AVAILABILITY[4]` → `items[4].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 38 / 2 | `REASON[4]` → `items[4].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 40 / 1 | `FUNCTION_ID[5]` → `items[5].function_id` | u8 / unsigned / N/A | 6=ENVIRONMENT; 허용 raw {6} | 별도 특수값 정의 없음 | T34:286 |
| 41 / 1 | `AVAILABILITY[5]` → `items[5].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 42 / 2 | `REASON[5]` → `items[5].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 44 / 1 | `FUNCTION_ID[6]` → `items[6].function_id` | u8 / unsigned / N/A | 7=REAR_WARNING; 허용 raw {7} | 별도 특수값 정의 없음 | T34:286 |
| 45 / 1 | `AVAILABILITY[6]` → `items[6].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 46 / 2 | `REASON[6]` → `items[6].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 48 / 1 | `FUNCTION_ID[7]` → `items[7].function_id` | u8 / unsigned / N/A | 8=WINDOW; 허용 raw {8} | 별도 특수값 정의 없음 | T34:286 |
| 49 / 1 | `AVAILABILITY[7]` → `items[7].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 50 / 2 | `REASON[7]` → `items[7].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 52 / 1 | `FUNCTION_ID[8]` → `items[8].function_id` | u8 / unsigned / N/A | 9=VSS; 허용 raw {9} | 별도 특수값 정의 없음 | T34:286 |
| 53 / 1 | `AVAILABILITY[8]` → `items[8].availability` | u8 / unsigned / N/A | 0가용/1제한/2불가; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:286 |
| 54 / 2 | `REASON[8]` → `items[8].reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:286 |
| 56 / 4 | `UPDATE_SEQUENCE` → `update_sequence` | u32 / unsigned / LE | u32, 새 가용성 평가 번호 | 별도 특수값 정의 없음 | T34:287 |
| 60 / 2 | `SOURCE_AGE` → `source_age_ms` | u16 / unsigned / LE | u16, 평가 후 ms; 단위=ms | 65535 포화 하한; 새 관측/최신성 증거 아님 | T34:288 |

[메시지 함수·C2 변환](../functions/uart_m/M_AVAILABILITY.md) · [NetMobileAvailability](UART_M_DATA.md#type-NetMobileAvailability)

<a id="m_digital_status"></a>
## M_DIGITAL_STATUS

`WIRE-P02-M_DIGITAL_STATUS` / `RX-P02-M_DIGITAL_STATUS-ESP32` · Type0x23, VERSION2 후보, P30/Frame40.

| Offset / 폭(B) | Wire → 선언된 typed 멤버 | Raw·부호·순서 | 코드/범위·단위 | sentinel/unknown | 근거 행 |
|---|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말; 0 예약 | 0 예약 | T34:301 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개 | 별도 특수값 정의 없음 | T34:301 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN 기동; 0 예약 | 0 예약 (공통 기동 규칙) | T34:301 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0 | 0 자발 출력 | T34:301 |
| 20 / 1 | `DIGITAL_KEY_SETTING_STATE` → `digital_key_setting_state` | u8 / unsigned / N/A | 0=OFF; 1=ON; 허용 raw {0,1} | 별도 특수값 정의 없음 | T34:302 |
| 21 / 1 | `DIGITAL_KEY_AVAILABILITY` → `digital_key_availability` | u8 / unsigned / N/A | 0=AVAILABLE; 1=LIMITED; 2=UNAVAILABLE; 허용 raw {0..2} | 별도 특수값 정의 없음 | T34:303 |
| 22 / 2 | `REASON` → `reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:304 |
| 24 / 4 | `UPDATE_SEQUENCE` → `update_sequence` | u32 / unsigned / LE | 새 판단/설정 평가 | 별도 특수값 정의 없음 | T34:305 |
| 28 / 2 | `SOURCE_AGE` → `source_age_ms` | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 새 관측/최신성 증거 아님 | T34:306 |

[메시지 함수·C2 변환](../functions/uart_m/M_DIGITAL_STATUS.md) · [NetMobileDigitalStatus](UART_M_DATA.md#type-NetMobileDigitalStatus)

<a id="m_digital_result"></a>
## M_DIGITAL_RESULT

`WIRE-P02-M_DIGITAL_RESULT` / `RX-P02-M_DIGITAL_RESULT-ESP32` · Type0x24, VERSION2 후보, P35/Frame45.

| Offset / 폭(B) | Wire → 선언된 typed 멤버 | Raw·부호·순서 | 코드/범위·단위 | sentinel/unknown | 근거 행 |
|---|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말; 0 예약 | 0 예약 | T34:319 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개 | 별도 특수값 정의 없음 | T34:319 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN 기동; 0 예약 | 0 예약 (공통 기동 규칙) | T34:319 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0 | 0 자발 출력 | T34:319 |
| 20 / 4 | `AUTO_OCCURRENCE_ID` → `auto_occurrence_id` | u32 / unsigned / LE | 자동 Unlock 실행 식별 | 별도 특수값 정의 없음 | T34:320 |
| 24 / 4 | `RELATED_DOOR_COMMAND_ID` → `related_door_command_id` | u32 / unsigned / LE | BCM 도어 명령 연계 | 별도 특수값 정의 없음 | T34:321 |
| 28 / 1 | `ORIGIN` → `origin` | u8 / unsigned / N/A | 2=PROXIMITY_AUTO; 허용 raw {2} | 별도 특수값 정의 없음 | T34:322 |
| 29 / 1 | `RESULT` → `result` | u8 / unsigned / N/A | 공통 Result; 기록 없으면 255=필드 없음; 허용 raw {0..5,255} | 255=필드 없음 | T34:323 |
| 30 / 2 | `RESULT_REASON` → `result_reason` | u16 / unsigned / LE | 공통 사유 코드; 허용 raw {0..25} | 별도 특수값 정의 없음 | T34:324 |
| 32 / 1 | `RESULT_CONFIRMATION` → `result_confirmation` | u8 / unsigned / N/A | 0=UNCONFIRMED; 1=CONFIRMED; 허용 raw {0,1} | 별도 특수값 정의 없음 | T34:325 |
| 33 / 2 | `RESULT_AGE` → `result_age_ms` | u16 / unsigned / LE | 결과 확인 후 ms; 단위=ms | 65535 포화 하한; 새 관측/최신성 증거 아님 | T34:326 |

[메시지 함수·C2 변환](../functions/uart_m/M_DIGITAL_RESULT.md) · [NetMobileDigitalResult](UART_M_DATA.md#type-NetMobileDigitalResult)

<a id="m_user_settings"></a>
## M_USER_SETTINGS

`WIRE-P02-M_USER_SETTINGS` / `RX-P02-M_USER_SETTINGS-ESP32` · Type0x25, VERSION2 후보, P36/Frame46.

| Offset / 폭(B) | Wire → 선언된 typed 멤버 | Raw·부호·순서 | 코드/범위·단위 | sentinel/unknown | 근거 행 |
|---|---|---|---|---|---|
| 0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / unsigned / LE | 현재 수신 단말; 0 예약 | 0 예약 | T34:339 |
| 4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / unsigned / LE | 현재 수신 Session; 원 요청 Session과 별개 | 별도 특수값 정의 없음 | T34:339 |
| 12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / unsigned / LE | 현재 DOMAIN 기동; 0 예약 | 0 예약 (공통 기동 규칙) | T34:339 |
| 16 / 4 | `QUERY_ID` → `context.query_id` | u32 / unsigned / LE | 조회 원 ID; 자발 출력0 | 0 자발 출력 | T34:339 |
| 20 / 2 | `TARGET_TEMPERATURE` → `target_temperature_centi_c` | i16 / signed / LE | Domain이 반영한 목표 온도, 0.01°C/raw, -32768=미확인; 단위=0.01 degC/raw | -32768=미확인 | T34:340 |
| 22 / 1 | `CLIMATE_AUTO` → `climate_auto` | u8 / unsigned / N/A | 0=OFF; 1=ON; 255=미확인; 허용 raw {0,1,255} | 255=미확인 | T34:341 |
| 23 / 1 | `USER_FAN_LEVEL` → `user_fan_level` | u8 / unsigned / N/A | 사용자가 설정한 FAN 코드; 허용 raw {0..3,255} | 255=미확인 Fan 설정; 명령에서의UNKNOWN과 구분 | T34:342 |
| 24 / 1 | `INTERIOR_LIGHT_ENABLED` → `interior_light_enabled` | u8 / unsigned / N/A | 0=OFF; 1=ON; 255=미확인; 허용 raw {0,1,255} | 255=미확인 | T34:343 |
| 25 / 1 | `NORMAL_LIGHT_LEVEL` → `normal_light_level_percent` | u8 / unsigned / N/A | 사용자 NORMAL 밝기,0~100%; 단위=1 percent/raw | 별도 특수값 정의 없음 | T34:344 |
| 26 / 3 | `NORMAL_LIGHT_RGB` → `normal_light_rgb` | rgb / unsigned / R/G/B consecutive | 사용자 NORMAL 색상 R/G/B; 단위=R/G/B each0..255 | 무효 sentinel 없음; 각255는 정상 값 | T34:345 |
| 29 / 1 | `VALUE_QUALITY` → `value_quality` | u8 / unsigned / N/A | 설정 확인 품질; 허용 raw {0..3} | 별도 특수값 정의 없음 | T34:346 |
| 30 / 4 | `UPDATE_SEQUENCE` → `update_sequence` | u32 / unsigned / LE | 현재 설정 확인 평가 번호 | 별도 특수값 정의 없음 | T34:347 |
| 34 / 2 | `SOURCE_AGE` → `source_age_ms` | u16 / unsigned / LE | 설정 확인 평가 후 ms; 단위=ms | 65535 포화 하한; 새 관측/최신성 증거 아님 | T34:348 |

[메시지 함수·C2 변환](../functions/uart_m/M_USER_SETTINGS.md) · [NetMobileUserSettings](UART_M_DATA.md#type-NetMobileUserSettings)
