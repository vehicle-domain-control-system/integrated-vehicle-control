# CIS — U2 동작 설명

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

<a id="fn-cis-cisnet_publishrear"></a>
## FN-CIS-CisNet_PublishRear — `CisNet_PublishRear()`

[의사코드](20_PSEUDOCODE.md#fn-cis-cisnet_publishrear) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-cis-cisnet_publishrear) · [원 선언](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishrear) · [CIS_REAR](../../../flows/10_CIS_REAR_FLOW.md)

**원형:** `NetSubmitResult CisNet_PublishRear( const NetCisRear *message );`

**현재 지위:** `PROPOSED_DESIGN / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `7db6c5c3dacac4b39c2b28b030e40c1fd60b44fe200a69b7568e8f6da57cf5f6`.

### 책임·인자·호출

CIS 초음파 관측 Application → CIS Network → CAN CIS_REAR. DOMAIN 위험 판단은 수신 기능 owner가 담당한다.

`message`는 IN/read-only, OUT 없음. 호출 동안 borrowed다. 전체 NetCisRear의 거리·측정상태·validity·quality_reason·UPDATE_SEQUENCE·SOURCE_AGE를 원 센서 owner가 생산한다.

### 정상 경로와 commit

Network owner 문맥에서 인자와 기존 표현 범위를 확인한다. 유효한 열화 보고를 정상 측정으로 바꾸지 않는다. 로컬 단조시각과 원 자료를 함께 안정적으로 인계할 수 있는지 확인하고 유한 공간을 확보한다. 원 필드 전체의 독립 복사와 AGE 기준점의 일관된 commit을 완료한 뒤에만 NET_SUBMIT_OK를 반환한다. Codec/Port 전송은 이후 별개 단계다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| R01 | NULL | NET_SUBMIT_INVALID_ARGUMENT, 새 자료 commit 없음 |
| R02 | 예약 validity/reason/proximity 코드·명시 범위 밖 필드 조합 | NET_SUBMIT_INVALID_ARGUMENT; 알려진 INVALID/STALE 보고와 구별 |
| R03 | NO_OBJECT 또는 UNAVAILABLE/FAULT/RECOVERING/INACTIVE의 정상 형식 | 원 상태·품질·거리 sentinel 보존, 후방 CLEAR 또는 0 mm 합성 없음 |
| R04 | 원 시점/순번·기동 연결 또는 stable handoff 경계 미준비 | NET_SUBMIT_NOT_READY; 원 자료는 caller가 소유 |
| R05 | 유한 인계 공간 없음 | NET_SUBMIT_NO_CAPACITY; 기존 자료/진행 중 TX를 부분 덮어쓰지 않음 |
| R06 | 전체 복사·시각 기준점·소유권 commit 완료 | NET_SUBMIT_OK; 실제 CAN submit/terminal/수신/차량 결과는 비증명 |
| R07 | 동일 원 평가의 재제출·이미 진행 중인 TX·링크 재기동 | 새 관측으로 재라벨하지 않음. 미전송 current pending 교체만 기존 계약 후보, 상세는 U4D |

### 시간·소유권·동시성·복구

source_age_ms는 호출까지의 원 관측 경과다. TX는 인계 후 대기만 한 번 더해 u16 포화하며 65535를 작은 수로 wrap하지 않는다. 재시도는 UPDATE_SEQUENCE 고정, TX_SEQUENCE 별도 증가다. 50/150 ms와 홉 5 ms는 후보이며 실시간 보장값이 아니다. 호출은 비차단 owner 문맥, ISR 직접 호출 금지. queue 크기·priority·락·교체 알고리즘은 BOUNDARY_PENDING_U4D다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-WIRE` | 양단 Network: 실제 Wire 채택과 필드 조합 검사 근거 |
| `BLOCKED_BY: OPEN-PROVENANCE` | CIS/Network/U4D: 원 BOOT/UPDATE/AGE 연결·wrap·원 시점 전달 |
| `BLOCKED_BY: OPEN-PORT-CAPABILITY` | B-track/U10: stable handoff 및 CAN 능력 |
| `BLOCKED_BY: OPEN-UNIT / OPEN-ENUM` | CIS/DOMAIN: 수신 typed 투영의 실제 헤더·의미 mapping; 여기서 변환하지 않음 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)

<a id="fn-cis-cisapp_ondomainpermission"></a>
## FN-CIS-CisApp_OnDomainPermission — `CisApp_OnDomainPermission()`

[의사코드](20_PSEUDOCODE.md#fn-cis-cisapp_ondomainpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-cis-cisapp_ondomainpermission) · [원 선언](../../../../interface/02_NODES/CIS/20_API.md#api-cisapp_ondomainpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `void CisApp_OnDomainPermission( const NetDomainPermission *message, const NetCanRxMeta *meta );`

**현재 지위:** `PROPOSED_DESIGN / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `7db6c5c3dacac4b39c2b28b030e40c1fd60b44fe200a69b7568e8f6da57cf5f6`.

### 책임·인자·호출

CIS CAN Network Dispatcher → CIS 기존 전원 허용 소비 Application. 같은 원 입력의 Pi relay는 Network-private 별도 경계이며 UART-C 구현은 U7이다.

`message`와 `meta`는 IN/read-only, callback 동안 borrowed, OUT 없음. 원 power 값/품질·공통 AGE·UPDATE와 meta.source의 DOMAIN Boot/TX, received_at_ms를 함께 읽는다. App이 반환 뒤 사용할 자료는 전체 의미와 시간근거를 독립 복사한다.

### 정상 경로와 commit

Dispatcher가 frame 구조와 수용된 DOMAIN 세대를 검증한 owner 문맥에서만 호출한다. App 인계는 power 품질과 원 AGE를 보존하고 operation 정보로 새 전원 판단을 만들지 않는다. 유효한 열화 record도 원 의미로 복사·전달할 수 있다. 소비자가 동작 허용을 쓰려면 기존 원 producer·기한·정책 근거가 별도로 확인돼야 하며 현재 OPEN-PERM이 그 경로를 차단한다. void 반환은 로컬 값 전달만 관찰하고 기능 수락/사용·전원 실행 성공을 증명하지 않는다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| A01 | NULL·예약 코드·형식 오류 | 로컬 입력 오류 관측 후 return; App 자료/liveness/허용 갱신 없음 |
| A02 | 수용되지 않은 DOMAIN Boot 또는 중복/역전 TX·원 UPDATE 충돌 | 격리/진단, 과거 허용을 최신으로 재사용하지 않음; 상세 순서규약 U4D |
| A03 | power=0/확인불가 또는 STALE/INVALID/NO_DATA | 원 값을 보존하고 허용 사용을 제한; 0/OK는 확인된 원 불허일 때만 그 의미 |
| A04 | power 허용처럼 보이나 원 producer/기한/coherence를 입증 못함 | BLOCKED_BY OPEN-PERM; 자료 인계와 실제 허용 사용을 분리 |
| A05 | 반환 후 저장 필요·저장 용량/consumer 미준비 | 전체 복사 미완료면 commit 없음; 진단·기존 연결로 전달, void를 성공으로 해석하지 않음 |
| A06 | 같은 원 UPDATE의 새 TX/late callback/통신 중단 | 링크 관측과 원 최신성 분리. 타이머 owner가 기존 자료를 aging, 합성 ALLOW/가짜 RX callback 없음 |

### 시간·소유권·동시성·복구

effective AGE는 Wire 원 AGE에 합의된 미계상 홉 여유와 로컬 수신 후 경과만 한 번 더한다. received_at_ms는 다른 ECU 시계와 뺄 수 없다. 150 ms/1500 ms 후보는 원 기한을 연장하는 근거가 아니다. callback 밖의 timeout/reset supervision owner 연결은 BINDING_OPEN이며 U2에서 Task/주기·복구 policy를 만들지 않는다. bounded cache/동시성은 BOUNDARY_PENDING_U4D, 실제 기능 정책은 CIS owner다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/CIS: 원 producer·power 원 사용 기한·품질/coherence |
| `BLOCKED_BY: OPEN-PROVENANCE` | CIS/Network/U4D: meta와 원 UPDATE/AGE 연결·generation admission |
| `BLOCKED_BY: OPEN-PEER-OWNER` | CIS/Pi/U7: 동일 검증 입력 relay의 별도 계약; 이번 구현 제외 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
