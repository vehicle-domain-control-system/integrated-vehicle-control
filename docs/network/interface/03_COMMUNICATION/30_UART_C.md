# UART-C v1 — Pi↔CIS 3 메시지 후보

**경로·VERSION 1·초기 115200 bps 적용 기준 / 상세 Wire WIRE_PROPOSED.** Pi(camera) ↔ UART-C ↔ CIS(sensor ECU) ↔ 공유 CAN ↔ DOMAIN이다. Pi-DOMAIN 직결은 없다.

[시작 안내](../00_START_HERE.md) · [Pi Data/API](../02_NODES/PI/00_OVERVIEW.md) · [CIS Data/API](../02_NODES/CIS/00_OVERVIEW.md) · [UART 공통 Frame/CRC](20_UART_M.md#section-shared-framing) · [공통 AGE](../04_COMMON/10_DATA_AND_ID_RULES.md#section-quality-freshness)

## 시작 설정·원본 품질

| 항목 | 적용·제안 |
| --- | --- |
| VERSION |1. UART-M v2와 독립|
| baud / 외형 |115200 bps 시작. 8N1 / 전이중 / RTS·CTS 없음은 PROPOSED|
| Frame/CRC |P+10B,최대 payload 128 B,[공통 외형](20_UART_M.md#section-shared-framing)·[CRC](20_UART_M.md#section-shared-crc)|
| Port/핀/전압 |OPEN. 실물 확인 전 임의 확정 금지|

P_VISION_RESULT/STATUS의 값·validity·reason·vision 상태·fault는 **Pi 비전 원 생산자가 작성**한다. 코드 어휘가 CIS 기능 품질과 같아도 Pi→CIS에서 CIS가 미리 작성한 평가라는 뜻은 아니다. Pi BOOT는 runtime/Network 단일 owner가 생성하고 VisionSequence는 새 카메라 판정, StatusSequence는 새 기능 상태 평가에서만 증가한다. 재전송은 이 번호와 원본 AGE 시작점을 유지한다.

CIS는 원 Pi Boot / VisionSeq / 값 / 누적 AGE를 CAN CIS_OCCUPANT에 보존한다. Pi 원 품질과 CIS 추가 검사를 보수적으로 결합하는 최소안은 PROPOSED이며 한 벌의 Wire validity/reason으로 두 품질의 모든 근거를 동시에 전달하는 방법은 OPEN이다. CIS 품질 검사로 새 Pi 관측을 만들거나 NO_DATA를 인원 0·없음으로 바꾸지 않는다.

P_PERMISSION은 CIS private relay가 원 DOMAIN 허용을 검증해 전달한다. CIS runtime Boot와 새 relay 평가 sequence는 원 Domain AGE를 새로 시작하지 않는다. source_age에는 CAN 전달·CIS 대기·UART 전달을 한 번씩 누적한다. use_limit 1500 ms는 후보이며 DOMAIN 허용 자체의 150 ms 사용 한도가 지난 미확인 상태를 1500 ms까지 정상 허용으로 연장하지 않는다. 정보 무효 시 차량용 결과를 유효 사용하지 않으며 프로세스 종료·물리 전원 차단을 새 요구로 만들지 않는다.

**PROPOSED — 기존 3 schema의 수신·실패 순서(O05/O13):** Network는 VERSION/TYPE/방향/정확한 payload 길이·CRC·허용 코드·수용한 원 세대를 검증한 후 기존 Pi/CIS typed 소비자에 전달한다. 잘못된 frame은 정상 수신/원 관측을 갱신하지 않으며 정상 형식의 INVALID/NO_DATA는 그 품질로 전달한다. `P_VISION_RESULT`의 Pi Boot/VisionSequence/원 AGE와 Presence/Count 원 품질은 CIS 수신 record에 보존한다. CAN 취합은 각 값별 원 INVALID의 사유를 우선 유지하고 원 VALID에 실제 CIS 사용 제한이 있을 때만 추가 제한하는 권장안이다. `P_VISION_STATUS` READY는 원 판정을 갱신하지 않는다. 구 Boot·지연 반복으로 새 관측 또는 AGE0을 만들지 않는다. 상세 처리와 역할 확인은 [CIS의 원 품질·relay](../02_NODES/CIS/10_DATA.md) 및 [Pi의 원 관측·소비](../02_NODES/PI/10_DATA.md)를 따른다.

`P_PERMISSION`은 CIS의 새 원 DOMAIN 평가/세대·품질 제한 때 기존 private relay record를 갱신·로컬 제출하고, 반복은 relay revision 유지·원 AGE 증가로 처리하는 안을 권장한다. Pi는 `min(원 DOMAIN 150ms 사용 상한 후보, USE_LIMIT)`와 실제 수신 이후 경과를 함께 확인한다. 500ms 반복/1500ms 후보는 유지하지만 이 조합만으로 연속 허용을 보증하지 않는다. 원 기한/새 평가 전달·실패 관측·Pi consumer 무효화는 `CROSS_NODE_AGREEMENT/IMPLEMENTATION_BINDING` 확인 사항이다. 로컬 제출 실패나 만료를 허용 성공으로 치환하지 않는다. 새로운 값·주기·byte는 확정하지 않으며 이 권고의 실제 Firmware/Codec/Board 시험은 `NOT_EXECUTED`다.

## 메시지 목록 — 시간은 PROPOSED

| 메시지 | ID/Type | 송신 → 수신 | 사용 / 전체 B | 주기 / 수신 중단 |
| --- | --- | --- | --- | --- |
| [P_VISION_RESULT](#message-p-vision-result) | 0x60 | PI → CIS | 19 / 29 | 100ms / 300ms |
| [P_VISION_STATUS](#message-p-vision-status) | 0x61 | PI → CIS | 16 / 26 | 1000ms / 3000ms |
| [P_PERMISSION](#message-p-permission) | 0x62 | CIS → PI | 15 / 25 | 500ms / 1500ms |


<a id="message-p-vision-result"></a>
## `P_VISION_RESULT` — `0x60`

**PI → CIS · 19 B 사용 / 29 B 전체 frame · WIRE_PROPOSED.**

의미 타입 `NetVisionResult`: [Pi Data](../02_NODES/PI/10_DATA.md) · [CIS Data](../02_NODES/CIS/10_DATA.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~3 | `PI_BOOT_ID` | `u32` · Pi 기동 번호 | 관련 기능/Stage B 추가 정보 |
| B4~7 | `VISION_SEQUENCE` | `u32` · 새 카메라 판정 번호 | 관련 기능/Stage B 추가 정보 |
| B8~9 | `VISION_AGE` | `u16` · 판정 원본 후 ms | 관련 기능/Stage B 추가 정보 |
| B10 | `OCCUPANT_PRESENCE` | `u8` · 0=없음; 1=있음; 255=미확인 | 관련 기능/Stage B 추가 정보 |
| B11 | `PRESENCE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | 관련 기능/Stage B 추가 정보 |
| B12 | `PRESENCE_REASON` | `u8` · Pi 비전 서비스가 생산한 원본 품질 사유 | 관련 기능/Stage B 추가 정보 |
| B13 | `OCCUPANT_COUNT` | `u8` · 0~5명,255=미확인 | 관련 기능/Stage B 추가 정보 |
| B14 | `COUNT_VALIDITY` | `u8` · 0=INVALID; 1=VALID | 관련 기능/Stage B 추가 정보 |
| B15 | `COUNT_REASON` | `u8` · Pi 비전 서비스가 생산한 원본 품질 사유 | 관련 기능/Stage B 추가 정보 |
| B16 | `VISION_STATUS` | `u8` · Pi 비전 서비스 기능 상태 | 관련 기능/Stage B 추가 정보 |
| B17~18 | `VISION_FAULT_CODE` | `u16` · 0=없음; 1=초기화; 2=카메라; 3=판정; 4=데이터 오류 | 관련 기능/Stage B 추가 정보 |


<a id="message-p-vision-status"></a>
## `P_VISION_STATUS` — `0x61`

**PI → CIS · 16 B 사용 / 26 B 전체 frame · WIRE_PROPOSED.**

의미 타입 `NetVisionStatus`: [Pi Data](../02_NODES/PI/10_DATA.md) · [CIS Data](../02_NODES/CIS/10_DATA.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~3 | `PI_BOOT_ID` | `u32` · Pi 기동 번호 | 관련 기능/Stage B 추가 정보 |
| B4~7 | `STATUS_SEQUENCE` | `u32` · 새 기능 상태 평가 | 관련 기능/Stage B 추가 정보 |
| B8 | `VISION_STATUS` | `u8` · Pi 비전 서비스 기능 상태 | 관련 기능/Stage B 추가 정보 |
| B9~10 | `ACTIVE_FAULT` | `u16` · 비전 고장 코드 | 관련 기능/Stage B 추가 정보 |
| B11~12 | `LAST_FAULT` | `u16` · Pi 이번 기동 이후 최근 비전 고장 | 관련 기능/Stage B 추가 정보 |
| B13~14 | `STATUS_AGE` | `u16` · 상태 평가 후 ms | 관련 기능/Stage B 추가 정보 |
| B15 | `AFFECTED_FUNCTION` | `u8` · bit0=탑승자 존재; bit1=인원수 | 관련 기능/Stage B 추가 정보 |


<a id="message-p-permission"></a>
## `P_PERMISSION` — `0x62`

**CIS → PI · 15 B 사용 / 25 B 전체 frame · WIRE_PROPOSED.**

의미 타입 `NetPiPermission`: [Pi Data](../02_NODES/PI/10_DATA.md) · [CIS Data](../02_NODES/CIS/10_DATA.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~3 | `CIS_BOOT_ID` | `u32` · CIS 기동 번호 | 관련 기능/Stage B 추가 정보 |
| B4~7 | `UPDATE_SEQUENCE` | `u32` · 새 허용 relay 평가 번호. 원 Domain 세대/허용 근거 변화 시 갱신하고 반복 frame에서 유지 | 관련 기능/Stage B 추가 정보 |
| B8 | `VEHICLE_POWER_PERMISSION` | `u8` · 0=불허; 1=허용; 255=확인 불가 | 관련 기능/Stage B 추가 정보 |
| B9 | `VALUE_QUALITY` | `u8` · 허용 상태 품질 | 관련 기능/Stage B 추가 정보 |
| B10 | `QUALITY_REASON` | `u8` · 품질 사유 | 관련 기능/Stage B 추가 정보 |
| B11~12 | `USE_LIMIT` | `u16` · 초안 1500ms | 관련 기능/Stage B 추가 정보 |
| B13~14 | `SOURCE_AGE` | `u16` · Domain 허용 판단 후 누적 ms | 관련 기능/Stage B 추가 정보 |

