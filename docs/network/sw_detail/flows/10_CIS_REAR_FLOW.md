# CIS_REAR — 원 관측부터 DOMAIN 입력까지

[↑ Flow 색인](../04_FLOW_INDEX.md#flow-cis-rear) · [Network](../00_START_HERE.md) · [Function](../03_FUNCTION_INDEX.md) · [최소 CAN Port](../layers/06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / WIRE_PROPOSED / G2_INDEPENDENT=PENDING.** 기준 `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 아래는 문서상 기대 처리이며 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`. CAN 계약 채택·실제 양단 통신·위험 정책 완료가 아니다.

<a id="flow-cis-rear-detail"></a>
## 책임과 함수 쌍

| 단계 | 생산·소비 책임 | 정확한 출처 / 이번 상세 |
|---|---|---|
| 원 관측 | CIS 센서 기능 owner가 거리·측정 상태·유효성·사유·새 관측 순번·원 경과를 생산 | [NetCisRear](../../interface/02_NODES/CIS/10_DATA.md#type-netcisrear) |
| typed 제출 | CIS Application → CIS Network | [원 선언](../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishrear) · [FN-CIS-CisNet_PublishRear 설명](../layers/02_APPLICATION_PORT/CIS/10_BEHAVIOR.md#fn-cis-cisnet_publishrear) · [의사코드](../layers/02_APPLICATION_PORT/CIS/20_PSEUDOCODE.md#fn-cis-cisnet_publishrear) |
| 보관·직렬화 | Message Service/Codec/Transport는 원 근거를 보존하며 bounded 인계와 명시 Codec를 제공해야 함 | [원 소유·반환](../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-ownership) · `BOUNDARY_PENDING_U4D` |
| CAN 경계 | 현재 두 메시지에 필요한 metadata·payload·stable submit·terminal·RX/health | [Port 의미 후보](../layers/06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md) · [Wire 후보](../../interface/03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) |
| RX/투영 | DOMAIN Network가 frame 구조·기동·순서·원 의미를 확인한 후 명시 typed 객체로 투영 | [기존 투영](../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#rx-projection) · [원 계보 연결](../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-origin-join) |
| C2 인계 | 검증된 typed 입력 → DOMAIN 기능 owner | [원 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisrear) · [FN-DOMAIN-DomainIf_RxCisRear 설명](../layers/02_APPLICATION_PORT/DOMAIN/10_BEHAVIOR.md#fn-domain-domainif_rxcisrear) · [의사코드](../layers/02_APPLICATION_PORT/DOMAIN/20_PSEUDOCODE.md#fn-domain-domainif_rxcisrear) |

DOMAIN이 CAUTION/EMERGENCY/CLEAR를 판단한다. Network가 NO_OBJECT를 위험 CLEAR로 바꾸거나 VSS가 후방 거리를 다시 판단하도록 하지 않는다. VSS의 음향 정책은 이번 흐름의 구현 범위 밖이다.

<a id="normal-path"></a>
## 정상 경로의 조건부 순서

1. CIS owner는 원 관측을 `NetCisRear`로 제공한다. NO_OBJECT는 유효한 무검출이며 UNAVAILABLE/FAULT/RECOVERING/INACTIVE와 다르다. 거리 sentinel 65535는 상태·품질과 함께 해석하고 실제 0 또는 최대 거리로 치환하지 않는다.
2. `CisNet_PublishRear`는 구조·현재 원 문맥과 로컬 저장 조건을 검사한다. caller 주소를 남기지 않고 원 필드와 인계시각의 전체 독립 복사/소유권 commit을 완료한 경우만 `NET_SUBMIT_OK`다. 실패는 partial admission이 없다.
3. Service는 아직 TX 시작 전인 동일 current key의 pending을 교체할 수 있다는 기존 계약 후보만 사용한다. 이미 Port에 accepted된 frame이나 새 관측 identity를 덮지 않는다. 실제 교체/큐·락·순서/wrap은 U4D다.
4. Codec는 [기존 CAN 후보](../../interface/03_COMMUNICATION/10_CAN_FD.md#message-cis-rear)의 ID·필드·little-endian·사용 길이/전송 길이를 명시적으로 따른다. 현재 후보는 19 B 사용, 20 B 전송, DLC 11이며 마지막 한 바이트 padding은 0이다. 상위 `sizeof(NetCisRear)`나 메모리 cast가 Wire 길이·배열 경계를 결정하지 않는다. 새 ID/DLC나 응용 CRC를 만들지 않는다.
5. TX 직전 원 AGE에 인계 이후 미계상 대기만 한 번 더해 포화한다. Port가 받아들인 사실과 local controller terminal outcome은 각각 관측한다. 어느 단계도 DOMAIN 실제 수신·C2 수용·차량 결과를 증명하지 않는다.
6. DOMAIN RX는 standard ID/FDF/BRS/RTR·DLC/실제 길이·padding·필수 code·원 BOOT와 TX 순서를 검증한다. CAN FD controller의 오류 검출을 사용한다는 기존 후보이며 별도 응용 CRC/ACK를 추가하지 않는다. 구조 오류를 typed callback이나 정상 peer 수신으로 연결하지 않는다.
7. 수용한 raw record와 원 CIS generation·UPDATE·AGE를 보존한 뒤 명시 C2 투영을 준비한다. unit/enum/provenance·실제 target Header가 확인되지 않은 분기는 `BLOCKED_BY`이며 raw cast로 통과시키지 않는다.
8. exact typed 입력의 인계 조건이 갖춰진 경우 `DomainIf_RxCisRear`가 C2 Core에 한 번 commit한다. `DOMAIN_IF_OK`는 이 로컬 C2 입력 결과다. 기존 optional observer가 이 경로에 실제 선언·연결된 경우만 commit 뒤에 통지하며 observer 오류로 재적용하지 않는다.

현재 미합의 때문에 7의 바인딩과 실제 실행 가능성은 완료 판정하지 않는다. 앞의 확정 가능한 검사·소유·결과 경계와 조건부 정상 처리 후보는 문서로 완성했으며, 조건이 해결되지 않았다는 이유로 허위 OK를 반환하지 않는다.

<a id="unit-and-quality"></a>
## 현재 원문의 거리·품질과 과거 OPEN

현재 [CisRearState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-cisrearstate-t)는 **`rearDistanceMm`**를 명시한다. `NetCisRear.rear_distance_deci_cm`의 주석도 0.1 cm/raw이며 두 표기를 이름만 보고 정수 cm로 나누지 않는다. 과거 `mm↔DOMAIN cm` OPEN은 역사적 바인딩 위험으로 보존한다. 이 Markdown의 필드 확인은 실제 누락 target Header 확보나 양단 단위 합의가 아니다. source 선언과 실제 target의 일치·sentinel·범위/반올림 정책 확인 전 `OPEN-UNIT`를 해결로 바꾸지 않는다.

[RearMeasurementState_t 후보 mapping](../../interface/04_COMMON/30_ENUM_WIRE_MAPPING.md#map-rearmeasurementstate-t)은 의미별 변환이며 숫자 cast 승인이 아니다. 원 validity/reason·[ValueQuality_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-valuequality-t)·SignalMeta의 차이는 명시 mapping과 근거가 필요하다. 표현할 수 없는 enum을 정상 또는 UNKNOWN이라는 임의 숫자로 바꾸지 않는다. 정상 형식의 열화 상태는 오류 정보이며 구조적 인자 오류와 구별해 보존한다.

<a id="negative-cases"></a>
## 문서상 오류·경계 시나리오

| 상황 | 검출 경계 | 기대 처리와 결과 | 원 근거/미결 |
|---|---|---|---|
| NULL/예약 코드/불법 조합 | CIS 제출 또는 DOMAIN typed 입력 | 각각 INVALID_ARGUMENT, 새 commit 없음 | 함수 설명 R01/R02, D02 |
| NO_OBJECT·FAULT·INACTIVE 정상 형식 | producer → typed 경계 | 원 의미·품질 보존, 0 mm/VALID/CLEAR 합성 없음 | 원 CIS Data/API |
| 공간 부족 | typed stable handoff 또는 C2 저장 경계 | NO_CAPACITY/QUEUE_FULL, partial admission 없음 | R05/D07, 구체 저장은 U4D |
| Port 미준비/FD 능력 부재 | admission 또는 이후 TX 경계 | 로컬 미준비 구분; admission 이후라면 과거 OK를 다시 쓰지 않고 실제 terminal/health 기록 | 최소 Port, 타깃 미검증 |
| accepted 이후 fail/unknown/bus-off | local terminal/health | 원 generation/correlation 보존, 차량 FAILED/DONE으로 합성 없음 | `BOUNDARY_PENDING_U4D` |
| 잘못된 길이/DLC/flag/padding/code·controller 오류 | DOMAIN RX Network | 격리/진단, C2 정상 호출·peer liveness 갱신 없음 | 기존 CAN 후보 |
| 같은 BOOT와 TX_SEQUENCE의 중복/역전 | RX transport 순서 | replay/순서 관측, 새 원 자료/정상 수신 기한으로 재생하지 않음 | admission/wrap 규약 OPEN |
| 새 TX_SEQUENCE, 같은 원 UPDATE | RX 원 평가 경계 | 링크 새 수신과 원 최신성을 분리. 같은 원 자료의 AGE를 낮추지 않음 | 원 ID·AGE 규칙 |
| 같은 원 identity인데 내용 충돌 | source binding | 기존 자료 보존·충돌 진단, 최신 자료로 임의 선택 없음 | OPEN-PROVENANCE |
| AGE 포화/미확인·원 사용 기한 입증 실패 | RX 투영·DOMAIN 기능 owner | 원 정보 보존, fresh/새 실행으로 승인하지 않음 | 원 기한·홉 여유 OPEN |
| CIS 재기동·오래된 boot frame·늦은 terminal | generation owner | 이전 자료를 새 기동 관측으로 승계하지 않음. 과거 correlation 격리 | Boot admission 및 U4D |
| 새 frame 없음/회복 frame 수신 | Network 관측 → 기능 owner | 수신 중단/정상 수신 사실만 관측; 센서 회복·새 관측은 owner 근거 필요 | 시간/health binding OPEN |
| typed unit/enum/계보 mapping 미확정 | DOMAIN Adapter, C2 호출 전 | `BLOCKED_BY`, raw 진단 보존; 가짜 C2 header/prototype 없음 | OPEN-UNIT/ENUM/C2-SOURCE/PROVENANCE |
| observer 실패/late 통지 | C2 commit 이후 | 기존 commit 재적용 없음. 전체 처리 실패와 observer 실패를 분리 | 기존 C2 ports |

<a id="time-and-identity"></a>
## 시간·순번·소유권

`BOOT_ID`는 Network 기동, `UPDATE_SEQUENCE`는 원 센서의 새 관측, `TX_SEQUENCE`는 같은 메시지의 전송 순번이다. 동일 원 평가를 반복 전송해도 UPDATE가 늘거나 SOURCE_AGE가 0이 되지 않는다. 실제 wrap/새 boot 수용 규약은 U4D·binding 증거가 필요하며 signed subtraction 같은 임의 알고리즘을 확정하지 않는다.

AGE는 원 producer부터 증가한다. TX admission 시각 이후 대기, 아직 포함되지 않은 홉 전달 여유, RX 이후 로컬 대기를 각각 한 번만 반영한다. 노드 간 단조시각을 직접 뺄 수 없다. u16 포화값은 실제 경과의 하한이고 fresh 상한 증명이 아니다. 50/150 ms·5 ms 등의 기존 후보를 실측 또는 안전 보장으로 쓰지 않는다. 모든 stable 자료는 값·identity·AGE 기준점을 같이 소유하고 callback borrowed 주소를 저장하지 않는다.

<a id="blocked-by"></a>
## BLOCKED_BY / 다음 증거

| 미결 | 담당 | 필요한 후속 근거 |
|---|---|---|
| OPEN-UNIT / OPEN-ENUM | CIS·DOMAIN·Network | 현재 Markdown과 정확한 target 선언/양단 의미·raw/sentinel mapping 일치 |
| OPEN-C2-SOURCE | DOMAIN/C2 | 원 target Header·Vehicle Types 원본 및 SHA; 구 헤더와 등치 금지 |
| OPEN-PROVENANCE | 원 source/Codec/C2 owner | CIS generation·UPDATE·AGE와 C2 snapshot의 명시 연결 |
| OPEN-WIRE / OPEN-PORT-CAPABILITY | 양단 Network/B-track/U10 | Wire 채택, 실제 metadata/용량·stable accepted/terminal/RX/clock 능력 |
| OPEN-SDK/HW/TIME | 보드/RTOS 담당 | B0~B3/U10 실제 능력·시간/자원 증거. U2에 SDK 구현 없음 |

WINDOW 보류·EXTERIOR 제외·BCM INTERIOR 유지, UART-C/M와 B2 실험 경계 및 외부 Pi/ESP32 owner는 이 흐름으로 변경되지 않는다.
