# UART-C v1 — Pi↔CIS 3 메시지 후보

Interface v2 · r1 · [규격과 연동 조건](../07_CROSS_NODE_INTERFACE_RULES.md#status)

UART-C v1의 TYPE·바이트 배치·CRC와 수신·소비 조건을 정의한다. 세부 권장 프로필은 양단 연동 검증이 필요하다.

**경로·VERSION 1·초기 115200 bps 적용 기준 / 상세 Wire WIRE_PROPOSED.** Pi(camera) ↔ UART-C ↔ CIS(sensor ECU) ↔ 공유 CAN ↔ DOMAIN이다. Pi-DOMAIN 직결은 없다.

[시작 안내](../00_START_HERE.md) · [Pi Data/API](../02_NODES/PI/00_OVERVIEW.md) · [CIS Data/API](../02_NODES/CIS/00_OVERVIEW.md) · [UART 공통 Frame/CRC](20_UART_M.md#section-shared-framing) · [공통 AGE](../04_COMMON/10_DATA_AND_ID_RULES.md#section-quality-freshness)

## 시작 설정·원본 품질

통신 처리 규칙 — 같은 수용 BOOT에서 같은 전송 순번·같은 raw인 정확한 중복은 수신감시를 갱신하지 않는다. 새 TX_SEQUENCE/LINK_SEQUENCE로 수용한 정상 반복은 같은 원 관측이라도 기존 수신 콜백으로 전달하여 기능 수신감시를 갱신한다. 원 생성시각·관측번호는 유지하고 현재 AGE는 경과에 따라 증가한다. 콜백 생략은 별도 감시 경로가 같은 수용 사실을 전달한다는 연결 증거가 있을 때만 허용한다. CRC/길이/코드 오류와 구세대는 감시 근거가 아니다. UART-C Pi Result/Status는 기존 두 CisApp 수신 콜백의 감시 연결을 따른다. CAN에서 같은 원 관측·같은 값·같은 품질인 정상 반복은 감시/AGE에 반영하되 C2의 새 관측으로 재발행하지 않는다. 확인된 품질 저하는 품질 저하 인계 분기로 별도 처리한다. CIS의 원 Pi record와 후속 제한 근거를 함께 유지하고 [필드별 품질·사유](../02_NODES/CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)를 적용한다.

| 항목 | 적용·제안 |
| --- | --- |
| VERSION |1. UART-M v2와 독립|
| baud / 외형 |115200 bps 시작. 8N1 / 전이중 / RTS·CTS 없음은 PROPOSED|
| Frame/CRC |P+10B,최대 payload 128 B,[공통 외형](20_UART_M.md#section-shared-framing)·[CRC](20_UART_M.md#section-shared-crc)|
| Port/핀/전압 |OPEN. 실물 확인 전 임의 확정 금지|

P_VISION_RESULT/STATUS의 값·validity·reason·vision 상태·fault는 **Pi 비전 원 생산자가 작성**한다. 코드 어휘가 CIS 기능 품질과 같아도 Pi→CIS에서 CIS가 미리 작성한 평가라는 뜻은 아니다. Pi BOOT는 runtime/Network 단일 owner가 생성하고 VisionSequence는 새 카메라 판정, StatusSequence는 새 기능 상태 평가에서만 증가한다. 재전송은 이 번호와 원본 AGE 시작점을 유지한다.

CIS는 원 Pi BOOT/VisionSequence/값/누적 AGE를 보존한다. CAN에는 최종 validity/reason 한 벌을 사용하고 원 판정 사유와 이후 CIS 제한 근거는 별도 보존한다. 같은 판정의 두 필드가 각각 사용 가능한 경우에만 상호 모순을 검사한다. 이후 통신 단절·만료·기동 변경·확인된 STATUS 고장은 현재 품질을 제한할 수 있다. 상세 품질 권장 규칙은 [CIS Data](../02_NODES/CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)를 따른다. 새 Pi 관측을 만들어 내거나 NO_DATA를 인원 0·없음으로 바꾸지 않는다.

P_PERMISSION은 CIS private relay가 검증한 원 DOMAIN 허용을 전달한다. 후방 활성 원 판단은 DOMAIN이 약 200ms마다 실제 재평가하며 원 사용 기한은 500ms다. CIS 기동 번호나 새 relay revision으로 원 판단 AGE를 다시 시작하지 않는다. 원 AGE에는 아직 반영하지 않은 전달·대기 경과만 한 번 누적한다. 다른 Power/Operation 의미에 500ms를 일괄 적용하지 않는다.

**권장 프로필 — 기존 3종의 수신·실패 처리:** VERSION/TYPE/방향/정확한 Payload 길이·CRC·지원 코드·수용한 세대와 순서를 검사한 뒤 기존 Pi/CIS typed 소비자에게 전달한다. 잘못된 프레임은 정상 수신이나 원 관측을 갱신하지 않는다. 정상 형식의 INVALID/NO_DATA 보고는 오류 프레임과 구분해 전달한다. 원 Pi BOOT/VisionSequence/AGE와 각 필드 품질은 보존한다. STATUS READY만으로 이전 판정을 갱신하지 않으며 구세대 프레임으로 새 관측이나 AGE0을 만들지 않는다. BOOT 생성·구세대 기억 범위·실제 수용 경계는 구현 검증이 필요하다.

`P_PERMISSION`은 새 원 평가·수용한 세대 변경·실제 품질 제한 시 중계 record를 갱신해 가능한 즉시 제출한다. 반복 전송에서는 기존 relay revision과 증가한 원 AGE를 유지한다. DOMAIN–Network가 유지하기로 한 UART-C 500ms 반복/1500ms 수신 중단은 원 사용 기한과 별개다.

`USE_LIMIT` 필드 값은 기존 1500ms 후보를 유지한다. 같은 후방 Power 원본임을 확인한 경우 500ms로 전달하는 조건부 권장 프로필이 있지만, 전송값은 양단 검증 전 미확정이다. 수신 해석은 검증된 원 수명과 전달 USE_LIMIT 중 짧은 한도를 따르므로 1500ms 후보가 원 500ms를 연장하지 않는다. 기한에 도달하면 ALLOW로 사용하지 않는다.

원 기한과 반복 간격이 모두 500ms라는 사실만으로 연속 허용이 보장되지는 않는다. 실제 새 평가의 즉시 중계·지연·실패·무수신을 검증해야 한다. Pi의 통신 해석과 비전 기능 적용은 분리하며 카메라 정지·프로세스 종료를 강제하지 않는다.  Power/Operation 생성원·공유 AGE 합성 및 실제 기능 연결은 미결이다.

<a id="delta-uart-permission"></a>
**DOMAIN 실패 정책 수용·실제 연결 대기:** 실패 즉시 UNKNOWN 권장안을 적용할 경우 같은 원 성공 평가의 새 품질 하향도 기존 CIS relay로 전달 요청하며 다음 성공 평가·원500ms 만료·500ms 반복까지 유예하지 않는 안이다. 원 성공 Sequence와 원 AGE 시작점은 보존하고 P_PERMISSION relay UPDATE_SEQUENCE·UART LINK_SEQUENCE를 새 DOMAIN 성공 평가로 보지 않는다. Pi는 원500ms 이내라도 수용한 무효 품질이면 이전 ALLOW를 사용하지 않는다. 적체/송신 실패 뒤 구 ALLOW 재전달 방지와 실제 수신 시점은 검증 대기이며 동시 전달을 보증하지 않는다. 기존3TYPE·필드·USE_LIMIT 후보 및 조건부500ms 지위는 유지한다. [정책·전달 경계](../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

## 메시지 목록 — 전송·감시와 원본 사용 기한 구분

P_PERMISSION의 반복 500ms/수신 중단 1500ms는 통신 감시 규칙이다. 나머지 보고·감시 시간은 기존 후보이며, 표의 시간은 원본 정보의 사용 기한을 대신하지 않는다. TYPE·Payload 길이·필드 오프셋·CRC는 아래에 정의한다.

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
| B0~3 | `PI_BOOT_ID` | `u32` · Pi 기동 번호 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B4~7 | `VISION_SEQUENCE` | `u32` · 새 카메라 판정 번호 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B8~9 | `VISION_AGE` | `u16` · 판정 원본 후 ms | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B10 | `OCCUPANT_PRESENCE` | `u8` · 0=없음; 1=있음; 255=미확인 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B11 | `PRESENCE_VALIDITY` | `u8` · 0=INVALID; 1=VALID | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B12 | `PRESENCE_REASON` | `u8` · Pi 비전 서비스가 생산한 원본 품질 사유 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B13 | `OCCUPANT_COUNT` | `u8` · 0~5명,255=미확인 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B14 | `COUNT_VALIDITY` | `u8` · 0=INVALID; 1=VALID | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B15 | `COUNT_REASON` | `u8` · Pi 비전 서비스가 생산한 원본 품질 사유 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B16 | `VISION_STATUS` | `u8` · Pi 비전 서비스 기능 상태 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B17~18 | `VISION_FAULT_CODE` | `u16` · 0=없음; 1=초기화; 2=카메라; 3=판정; 4=데이터 오류 | 메시지별 부가 필드(현 UART-C Wire 계약) |


<a id="message-p-vision-status"></a>
## `P_VISION_STATUS` — `0x61`

**PI → CIS · 16 B 사용 / 26 B 전체 frame · WIRE_PROPOSED.**

의미 타입 `NetVisionStatus`: [Pi Data](../02_NODES/PI/10_DATA.md) · [CIS Data](../02_NODES/CIS/10_DATA.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~3 | `PI_BOOT_ID` | `u32` · Pi 기동 번호 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B4~7 | `STATUS_SEQUENCE` | `u32` · 새 기능 상태 평가 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B8 | `VISION_STATUS` | `u8` · Pi 비전 서비스 기능 상태 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B9~10 | `ACTIVE_FAULT` | `u16` · 비전 고장 코드 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B11~12 | `LAST_FAULT` | `u16` · Pi 이번 기동 이후 최근 비전 고장 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B13~14 | `STATUS_AGE` | `u16` · 상태 평가 후 ms | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B15 | `AFFECTED_FUNCTION` | `u8` · bit0=탑승자 존재; bit1=인원수 | 메시지별 부가 필드(현 UART-C Wire 계약) |


<a id="message-p-permission"></a>
## `P_PERMISSION` — `0x62`

<a id="r1-p-permission-consumer"></a>
Pi 소비는 실제 원 Power 평가의 AGE와 relay 이후 경과를 적용한다. 원 사용 기한 500ms는 499ms/500ms/501ms 중 각각 조건부 사용 가능/사용 제한/사용 제한으로 판정하며 새 LINK_SEQUENCE가 기한을 재시작하지 않는다. USE_LIMIT 1500ms 설계 후보와 500ms CONDITIONAL 변경안을 구분하고, 어느 경우에도 원 Power 기한을 늘리는 근거로 쓰지 않는다. 공유 Permission 합성과 Pi 카메라 제어의 기능 owner 연결은 별도 검증 대기다.

**CIS → PI · 15 B 사용 / 25 B 전체 frame · WIRE_PROPOSED.**

의미 타입 `NetPiPermission`: [Pi Data](../02_NODES/PI/10_DATA.md) · [CIS Data](../02_NODES/CIS/10_DATA.md) · [양 끝 명명 함수](../05_MESSAGE_INDEX.md).


| payload byte | 필드 | Raw 표현·유효 조건 | 논리 근거 |
| --- | --- | --- | --- |
| B0~3 | `CIS_BOOT_ID` | `u32` · CIS 기동 번호 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B4~7 | `UPDATE_SEQUENCE` | `u32` · 새 허용 relay 평가 번호. 원 Domain 세대/허용 근거 변화 시 갱신하고 반복 frame에서 유지 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B8 | `VEHICLE_POWER_PERMISSION` | `u8` · 0=불허; 1=허용; 255=확인 불가 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B9 | `VALUE_QUALITY` | `u8` · 허용 상태 품질 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B10 | `QUALITY_REASON` | `u8` · 품질 사유 | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B11~12 | `USE_LIMIT` | `u16` · 초안 1500ms | 메시지별 부가 필드(현 UART-C Wire 계약) |
| B13~14 | `SOURCE_AGE` | `u16` · Domain 허용 판단 후 누적 ms | 메시지별 부가 필드(현 UART-C Wire 계약) |


<!-- 기존 문서 anchor 호환; 본문은 위 전체 선언과 설명으로 승계 -->
<a id="메시지-목록--시간은-proposed"></a>
