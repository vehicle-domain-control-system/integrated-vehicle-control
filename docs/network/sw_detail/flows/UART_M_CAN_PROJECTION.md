# UART-M CAN 원본 투영 — 동일 입력에서 수신 표시 경계까지

BCM4·CIS4·VSS1·WINDOW2의 11 Wire와 11 ESP32 Rx 후보를 연결한다. 실제 CAN 동작, C2 commit, 양단 VERSION2 채택 또는 ESP32 소비를 검증했다는 뜻이 아니다. 기존 CAN 원본·AGE 설계 기준을 보존하며 WINDOW 두 경로는 실제 실행·표시·timeout·liveness HOLD다.

[UART-M 전체](../05_UART_M_OVERVIEW.md) → [M01 원본 연결](../modules/M01_SOURCE_CONTEXT.md) → [M03 typed 인계](../modules/M03_TYPED_HANDOFF.md) → [M02 Query](../modules/M02_QUERY_CONTROL.md) / [M06 UART Codec](../modules/M06_UART_CODEC.md) → [필드 정본](../data/UART_M_PROJECTION_FIELDS.md) → [공통 Router](../functions/UART_M_ROUTER.md) / [송수신·Driver](../functions/UART_M_TX_RX_CONTROL.md).

## 메시지별 원 CAN·Data·Function

| UART-M Wire / ESP32 Rx | 원 CAN used → payload / frame | typed DTO·전체 필드 / 함수 |
|---|---|---|
| WIRE-P02-M_BCM_DOOR_STATE / RX-P02-M_BCM_DOOR_STATE-ESP32 | BCM_DOOR_STATE20 →40/50B | [NetMobileBcmDoorState 필드](../data/UART_M_PROJECTION_FIELDS.md#m_bcm_door_state) / [함수](../functions/uart_m/M_BCM_DOOR_STATE.md) |
| WIRE-P02-M_BCM_CLIMATE_STATE / RX-P02-M_BCM_CLIMATE_STATE-ESP32 | BCM_CLIMATE_STATE24 →44/54B | [NetMobileBcmClimateState 필드](../data/UART_M_PROJECTION_FIELDS.md#m_bcm_climate_state) / [함수](../functions/uart_m/M_BCM_CLIMATE_STATE.md) |
| WIRE-P02-M_BCM_LIGHT_STATE / RX-P02-M_BCM_LIGHT_STATE-ESP32 | BCM_LIGHT_STATE25 →45/55B | [NetMobileBcmLightState 필드](../data/UART_M_PROJECTION_FIELDS.md#m_bcm_light_state) / [함수](../functions/uart_m/M_BCM_LIGHT_STATE.md) |
| WIRE-P02-M_BCM_STATUS / RX-P02-M_BCM_STATUS-ESP32 | BCM_STATUS22 →42/52B | [NetMobileBcmStatus 필드](../data/UART_M_PROJECTION_FIELDS.md#m_bcm_status) / [함수](../functions/uart_m/M_BCM_STATUS.md) |
| WIRE-P02-M_CIS_ENVIRONMENT / RX-P02-M_CIS_ENVIRONMENT-ESP32 | CIS_ENVIRONMENT40 →60/70B | [NetMobileCisEnvironment 필드](../data/UART_M_PROJECTION_FIELDS.md#m_cis_environment) / [함수](../functions/uart_m/M_CIS_ENVIRONMENT.md) |
| WIRE-P02-M_CIS_OCCUPANT / RX-P02-M_CIS_OCCUPANT-ESP32 | CIS_OCCUPANT25 →45/55B | [NetMobileCisOccupant 필드](../data/UART_M_PROJECTION_FIELDS.md#m_cis_occupant) / [함수](../functions/uart_m/M_CIS_OCCUPANT.md) |
| WIRE-P02-M_CIS_REAR / RX-P02-M_CIS_REAR-ESP32 | CIS_REAR19 →39/49B | [NetMobileCisRear 필드](../data/UART_M_PROJECTION_FIELDS.md#m_cis_rear) / [함수](../functions/uart_m/M_CIS_REAR.md) |
| WIRE-P02-M_CIS_STATUS / RX-P02-M_CIS_STATUS-ESP32 | CIS_STATUS34 →54/64B | [NetMobileCisStatus 필드](../data/UART_M_PROJECTION_FIELDS.md#m_cis_status) / [함수](../functions/uart_m/M_CIS_STATUS.md) |
| WIRE-P02-M_VSS_STATUS / RX-P02-M_VSS_STATUS-ESP32 | VSS_STATUS28 →48/58B | [NetMobileVssStatus 필드](../data/UART_M_PROJECTION_FIELDS.md#m_vss_status) / [함수](../functions/uart_m/M_VSS_STATUS.md) |
| WIRE-P02-M_WINDOW_STATE / RX-P02-M_WINDOW_STATE-ESP32 | WINDOW_STATE22 →42/52B, 실행 HOLD | [NetMobileWindowState 필드](../data/UART_M_PROJECTION_FIELDS.md#m_window_state) / [함수](../functions/uart_m/M_WINDOW_STATE.md) |
| WIRE-P02-M_WINDOW_FAULT / RX-P02-M_WINDOW_FAULT-ESP32 | WINDOW_FAULT22 →42/52B, 실행 HOLD | [NetMobileWindowFault 필드](../data/UART_M_PROJECTION_FIELDS.md#m_window_fault) / [함수](../functions/uart_m/M_WINDOW_FAULT.md) |

Climate/Light 후보 C2는 동일 Vehicle v0.3/Domain Interface v0.6 선언을 요구한다. sourceGeneration은 원 생산 세대이며 실제 BCM Boot 대응·generation 0 정책·제품 전체 재빌드/ABI는 미검증이다. Getter/TxPort 성공 사본의 세대와 원 CAN/C2 commit 연결이 부족하면 해당 투영만 NOT_READY다. 환경/VSS/WINDOW 타입으로 필드 추가를 확대하지 않는다.

기존 원 CAN used layout의 필드만 DOWN_CONTEXT20B 뒤에 배치한다. 원 관측 값·식별은 유지하며 입증된 현재 품질 제한은 같은 원 raw와 실제 제한 사건·C2/Core/Getter의 연결에 따라 반영한다. 원 raw의 GOOD/VALID로 현재 제한을 되돌리지 않는다. payload B12 DOMAIN_BOOT_ID는 현재 DOMAIN 기동, B20/B24는 원 CAN BOOT_ID/TX_SEQUENCE다. Pi Boot/VisionSequence는 CIS_OCCUPANT 내부 B28/B32에 별도로 남는다. CAN padding·C 구조체 padding·새 generation 필드는 전송하지 않는다.

## 원본 연결의 조건과 실패

| 단계 | 입증할 사실과 수행 | 증거가 없을 때 |
|---|---|---|
| 원 CAN 수신 | B12의 exact ID/flags/DLC/length/padding·필드·Boot/순서 검사를 통과한 Net 객체와 같은 NetCanRxMeta | 정상 입력/관측으로 게시하지 않음 |
| C2 ingress | B12의 명시 enum·단위·품질 변환으로 같은 객체를 C2에 인계 | Callback 접수 성공과 의미 투영 성공을 분리 |
| DOMAIN commit | 실제 Manager가 그 입력을 반영한 결과, 다른 Core/Observer 단계와의 구분 | `DomainIf_Rx*`의 OK만으로 snapshot commit을 가정하지 않음 |
| Getter/TxPort | 읽은 C2 값과 보존한 원 raw/meta가 같은 원 관측이며, 현재 제한은 실제 제한 사건과 Core/Getter revision으로 입증됨; 동기화된 Owner 문맥에서 확인 | 그 투영만 NET_SUBMIT_NOT_READY / Query 일부 미제공 |
| 의미 자료 사본 | 현재 recipient/Domain Boot/Query 문맥과 원 관측 객체·시각 근거를 함께 복사 | 빌린 pointer만 저장하거나 서로 다른 입력을 합치지 않음 |
| AGE | 원 raw 기준시각 또는 Getter가 이미 증가시킨 시각을 선택해 아직 반영되지 않은 구간만 한 번 더함 | 미측정 Driver 지연을0으로 두지 않음; 필요한 freshness 소비 TIME_HOLD |
| frame·Driver | 메시지별 Validate/Encode → immutable frame → 수용/최종TX/해제 사실 별도 보존 | local OK 또는 accepted로 Query count를 증가시키지 않음 |
| ESP32 소비 | 같은 version/Type/P/CRC·전체 필드, 현재 Session/Boot·순서 수용·Gateway subtype convention 확인 | 일부 형식 성공으로 표시·정상 liveness·Query 완료 없음 |

이 연결은 M01/ST01의 원 입력 근거와 관측 메타를 유지하는 책임이다. 새로운 차량 Shadow cache를 만들거나 C2와 별도 차량 상태의 정본을 두지 않는다. 원 관측이 같은 경우에도 높은 TX 재전송은 AGE 기준을 새로 시작하지 않는다. 값이 같으나 UPDATE/VISION 번호가 다른 경우는 별도 관측이며 단순 값 비교로 association하지 않는다. 소유자의 원자적 반영·직렬화·Generation 대응 증거가 없는 현재 경로는 IF-O24/IF-O09/D-02/C2-H01 HOLD다.

## 대표 CIS_REAR 수직 경로

[B11 CIS_REAR 144→344](CIS_REAR_144_TO_344.md)와 [B12 CAN 확장](CAN_CIS_WINDOW_VSS.md)을 그대로 따른다. raw123(0.1cm)=123mm이며 원 UPDATE_SEQUENCE, 품질, PROXIMITY_STATUS를 유지한다. C2 `CisRearState_t.rearDistanceMm=123`이라는 값만으로 최신 CAN boot/tx를 붙이지 않는다.

```c
// 기존 API의 사용 순서 설명이다. 새로운 공개 함수/상태/Owner를 만들지 않는다.
CisRearState_t snapshot;
DomainIf_Status_t got = DomainIf_QueryGetCisRear(nowMs, &snapshot);
if (got != DOMAIN_IF_OK) {
    // snapshot을 읽지 않는다. 해당 Query 응답은 미제공/부분 상태로 관측한다.
} else if (/* snapshot이 어느 원 CAN 수신·C2 commit인지 입증되지 않음 */) {
    // 값123이 같아도 새 raw의 Boot/TX를 붙이지 않는다. 이 항목만 source 미제공.
} else {
    // query는 같은 Device/Session/DOMAIN Boot/0이 아닌 QueryID·scope가 검증된 기존 문맥.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileCisRear(&snapshot, &query);
    if (submitted == NET_SUBMIT_OK) {
        // 자기 소유 보관이 완료된 사실만 남긴다. ITEM_COUNT는 그대로다.
        // 메시지 Encode는 payload B20/B24 원 CAN Boot/TX, B28 Update,
        // B32 누적 AGE, B34 raw123, B36~38 품질/상태를 직렬화한다.
        // Driver 최종 TX가 현재 ticket/세대·Query에 귀속된 경우에만 완료수 후보에 반영한다.
    } else {
        // NOT_READY/NO_CAPACITY/INVALID_ARGUMENT 원 반환을 보존한다. 성공 항목으로 세지 않는다.
    }
}
```

위 `nowMs`는 Getter 로컬 단조시각, `query`는 기존 DomainQueryResponseContext_t이며 값 생성 API가 아니다. snapshot의 AGE와 원 raw AGE는 서로 다른 기준시각일 수 있다. 원 raw를 이어 쓸 때에는 원 raw 관측시각에서 미반영 경과를 계산하고, Getter가 반영한 경과를 다시 더하지 않는다. 원 관측 순번/Boot/거리/품질은 바뀌지 않는다. unknown 시간·uint16 포화65535로 사용 한도를 통과시켜서는 안 된다.

## Query와 자발 상태의 차이

자발 TxPort callback은 기존 submit의 `query=NULL`이며 QUERY_ID0이다. Query는 공통 passive 문맥 검증을 먼저 거친 성공 Getter OUT에 같은 submit과 non-NULL query를 사용한다. Getter는 DOMAIN cache의 로컬 복사이며 ECU RPC가 아니다. 대상 목록·snapshot 직렬화·원본 연결·세대 정책은 Owner 계약으로 남는다.

association 실패로11개 중1개가 미제공이면 그 항목을 Query 부분으로 기록한다. 나머지 독립 항목을 자동 거부하거나 전체 count를11로 미리 정하지 않는다. WINDOW2는 현 실행 HOLD 때문에 실제 응답 생산을 하지 않는다. scope에 따라 필요한 Window 항목이 제공되지 않는다면 진짜 빈 집합이 아닌 미제공 사실을 보존한다. Query End status/count는 [Query 제어](../modules/M02_QUERY_CONTROL.md)에 따르며 enqueue 수·End·중복·QUERY_ID0은 최종 응답수에 넣지 않는다.

Driver accepted 후 결과가 미상인 항목은 원 Query 귀속을 보존한다. 새 LINK_SEQUENCE로 재송신해 성공 count를 늘리거나 End를 먼저 내보내지 않는다. 실제 final TX와 순서를 입증할 수 있을 때만 기존 status0/1/2/3의 조건을 적용한다. ESP32의 유효 RX·BLE 전달·App 목록 완료는 별개이며 늦은 End로 닫힌 Query를 다시 열지 않는다.

## 노드별 표현 손실과 HOLD

| 대상 | 보존·금지 및 영향 |
|---|---|
| BCM Door/Climate/Light | 값별 품질 유지. C2 enum 숫자와 Net raw 코드 직접 cast 금지. Climate/Light는 Vehicle v0.3 후보의 sourceGeneration을 선언~Rx~전체 복사~Getter에 보존하고 같은 원 관측 생산 세대와 대조. CAN Boot 자동 대입·Wire 필드 추가 금지 |
| BCM Status | 전체 mask/주요 고장/affected function/원 meta 결합이 필요. 일부 Snapshot에서 나머지를0으로 복원 금지 |
| CIS Environment | 세 센서 seq/AGE/quality 독립. 온습도 각2500ms 비사용 경계, 조도 TTL 미결. [로컬 제한 근거](../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)로 현재 품질을 연결하고 DTO generation 추가 없음. [r1 조건부 저장 순서](../functions/can/CIS_ENVIRONMENT.md#environment-storage-order)는 존재하며 실제 DOMAIN 근거 미확인으로 [BG-R05-BOOT](../functions/can/CIS_ENVIRONMENT.md#boot-binding-gap)는 BLOCKED_DESIGN 유지 |
| CIS Occupant | Pi와 CIS identity 분리. presence255·count255를 false/0으로 바꾸지 않음. [원 관측의 현재 제한 근거](../data/CAN_NODE_DATA.md#type-NetCanOccupantRestrictionEvidence)를 Getter 품질과 연결하며 raw VALID로 회복 금지. C2 bool의 표현 손실이 있으면 원 association 없이는 중계 불가 |
| CIS Rear | 1raw=1mm. NO_OBJECT65535와 무효/FAULT를 상태·품질로 구분 |
| CIS Status | CIS_READY와 센서 건강, UART/CAN interface 상태는 별개. 지원되지 않는 C2 interface 값은 새 raw로 매핑하지 않음 |
| VSS Status | 최근 고장 품질0/3 한정. C2 state/fault 분리 반영의 같은 입력 증거 필요. CF02 warning51/60 문제를 해결한 것으로 취급하지 않음 |
| WINDOW State/Fault | 문서와 Rx ID 추적만 유지. 실제 Codec/Router/submit에서 명시 HOLD. 표시/감시/물리 동작 미활성 |

현재 UART-M v2 profile 전역 채택 증거가 없다는 CF01과 각 projection의 association 미입증은 서로 다른 제한이다. S6 후 회신이 확보되어도 명시 승인·전체 영향 재검수 없이 프로필·헤더·Wire·Callback을 적용하지 않는다.

## 반례와 복구

| 반례 | 예상 처리·복구 |
|---|---|
| 같은값·다른Update/VisionSequence | 같은 관측으로 합치지 않음; 정확한 원 입력 증거 재확인 |
| DOMAIN만 재기동 | 새 outer DOMAIN_BOOT를 기록하되 원 ECU/Pi Boot를 새 DOMAIN Boot로 바꾸지 않음; 이전 Query와 pending 항목 재귀속 없음 |
| 동일 CAN 관측의 높은 TX 재전송 | 새 측정·AGE0 생성 없음. B12의 raw 기준선/보수적 나이 원자 갱신과 이어짐 |
| CRC 정상이나 enum/문맥 오류 | 정상 typed/Gateway/liveness commit 없음. 원자료를 임의 정정하지 않음 |
| Queue 부족 | 전체 안정 사본 접수 전 NO_CAPACITY; 기존 discrete 응답을 조용히 덮어쓰지 않음 |
| final TX 전 timeout 또는 옛 Driver callback | Driver release와 최종 결과는 독립. 참조 중 버퍼 재사용 금지·old callback이 새 Query count에 참여하지 않음 |
| Getter 실패 또는 성공 OUT의 association 부재 | OUT 오독/전0상태/성공0건 End 금지; 해당 항목 부분/미제공 |
| WINDOW 미수신 또는 Permission 회복 | HOLD 유지. timeout Fault·liveness/표시 활성화·과거 이동 재개 없음 |

전문 근거: `S4C evidence/b08/UART_M_ROUTING.md#provenance`, `UART_M_CONTRACT.md#projection`, `uart_m/{bcm_projection,cis_projection,vss_window_projection}.md`, `S4C TIME_IDENTITY_CONTRACT.md#projection`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`, `S4A api/domain_submit.md`, `api/domain_query.md`, `api/callbacks.md`, `api/esp32.md`. 원 Owner35와 기존 요구/결정은 변경하지 않는다.

### PG6-SW02 환경 Boot 전이의 projection

환경의 B 정상 인계는 A 미실행 입력 차단·구 실행/후속 비동기 쓰기 배제·NO_DATA의 VSM/독립 Meta 실제 commit 뒤에만 열린다. Queue 접수·callback 반환·C2 OK·TX local OK를 저장 완료로 읽지 않는다. 무효화 거부/완료불명에서 B가 차단돼도 A GOOD가 지워졌다고 주장하거나 현재 source B와 구 A raw를 합쳐 보내지 않는다. 해당 Getter/TxPort의 정상 projection은 HOLD/Query 부분으로 남긴다. 실제 Core 완료 뒤 Observer 실패만으로 commit을 다시 요청하지 않는다. 동일 원본 연결이 확인된 B는 원seq0·원 기준 AGE를 보존하며2500ms 도달 온습도를 정상으로 복원하지 않는다. 일반 NO_DATA는 Boot/reset 근거가 아니다.
