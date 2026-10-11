# DOMAIN — U2 동작 설명

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

<a id="fn-domain-domainif_rxcisrear"></a>
## FN-DOMAIN-DomainIf_RxCisRear — `DomainIf_RxCisRear()`

[의사코드](20_PSEUDOCODE.md#fn-domain-domainif_rxcisrear) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-domain-domainif_rxcisrear) · [원 선언](../../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisrear) · [CIS_REAR](../../../flows/10_CIS_REAR_FLOW.md)

**원형:** `DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);`

**현재 지위:** `DECLARED_SOURCE / MISSING_SOURCE_HEADER / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `03a4e8dadfd78a41f73ce13af44eccb9a2504a48924d024b91570a48e6d0a96d`.

### 책임·인자·호출

DOMAIN의 검증된 CAN 수신·typed Adapter → DOMAIN C2 입력 → 후방 위험 판단 owner. CIS 원격 함수 주소를 직접 호출하지 않는다.

`state`는 const CisRearState_t IN/borrowed, OUT 없음. 현재 Markdown의 rearDistanceMm, measurementState, distanceQuality, sourceGeneration를 받는다. distanceQuality.meta의 원 AGE/UPDATE와 admitted CIS lineage를 연결해야 한다.

### 정상 경로와 commit

이 함수 이전에 raw 길이/FD flags/코드/BOOT/TX 순서와 원 CIS 계보를 검사하고 명시 typed 투영을 마쳐야 한다. C2 함수는 초기화·NULL·typed 표현 범위·수용 문맥을 검사한다. 품질이 낮은 정상 record도 원 상태대로 C2에 commit하며 후방 판단은 기능 owner에게 맡긴다. Core commit은 한 번만 수행한다. 적용 가능한 기존 optional observer가 있다면 commit 이후 통지하며 실패로 Core를 다시 적용하지 않는다. 이 U2는 새 onCisRear observer를 선언하지 않는다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| D01 | C2 초기화 전 | DOMAIN_IF_NOT_INITIALIZED; commit 없음 |
| D02 | NULL·typed enum/필드 조합의 구조 오류 | DOMAIN_IF_INVALID_ARGUMENT; 기존 record 보존 |
| D03 | raw→typed 원형·단위·enum·계보 binding 미확인 | Adapter에서 호출 차단. 직접 인계 조건 미준비는 DOMAIN_IF_NOT_AVAILABLE |
| D04 | 정상 형식의 NO_OBJECT/열화/STALE/NO_DATA | 원 정보 그대로 로컬 수용 가능. DOMAIN 위험 해제나 새 관측으로 승격 없음 |
| D05 | 수용되지 않은 CIS generation·역전/충돌 update | 기존 C2 의미 거부 DOMAIN_IF_REJECTED 후보; upstream 검증과 중복 수행 방지 |
| D06 | 지원하지 않는 typed 의미 | DOMAIN_IF_UNSUPPORTED; Wire numeric cast·정상 치환 금지 |
| D07 | 필요한 bounded 보관 미확보 | DOMAIN_IF_QUEUE_FULL 후보; 부분 commit 없음 |
| D08 | 전체 Core 입력 commit 완료 | DOMAIN_IF_OK는 로컬 C2 인계만. optional observer 실패는 commit 재시도 근거 아님 |

### 시간·소유권·동시성·복구

C2는 Task/owner 문맥에서 비차단 호출한다. callback 포인터는 반환까지 borrowed; 캐시가 필요하면 값·원 sourceGeneration·메타를 복사한다. 현재 Markdown은 rearDistanceMm를 명시하므로 정수 cm 나눗셈을 넣지 않는다. 실제 target Header 2개가 없고 과거 mm↔cm 바인딩은 OPEN이다. TX_SEQUENCE는 state의 새 원 관측 번호가 아니다. 같은 원 기록의 effective AGE를 낮추지 않으며 시계/wrap·중복·cache commit 세부는 BOUNDARY_PENDING_U4D 및 기존 C2 binding 확인 사항이다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-C2-SOURCE` | DOMAIN/C2 owner: 정확한 target 헤더와 실제 Core 입력 경계 |
| `BLOCKED_BY: OPEN-UNIT / OPEN-ENUM` | CIS/DOMAIN/Network: 원문 필드와 실제 target의 의미 투영 합의 |
| `BLOCKED_BY: OPEN-PROVENANCE` | DOMAIN/Codec/U4D: CIS BOOT/원 평가와 C2 sourceGeneration 연결 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)

<a id="fn-domain-domainif_txcispowerpermission"></a>
## FN-DOMAIN-DomainIf_TxCisPowerPermission — `DomainIf_TxCisPowerPermission()`

[의사코드](20_PSEUDOCODE.md#fn-domain-domainif_txcispowerpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-domain-domainif_txcispowerpermission) · [원 선언](../../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txcispowerpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);`

**현재 지위:** `DECLARED_SOURCE / MISSING_SOURCE_HEADER / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `03a4e8dadfd78a41f73ce13af44eccb9a2504a48924d024b91570a48e6d0a96d`.

### 책임·인자·호출

DOMAIN 전원 허용 기능 owner → C2 → 등록된 기존 txCisPowerPermission TxPort → Network 내부 입력 보관. private composer가 후속에 DOMAIN_PERMISSION을 구성한다.

`permission`은 IN/read-only, OUT 없음. vehiclePowerAllowed, quality, meta를 전원 기능 owner가 생산한다. 이 입력은 완성된 NetDomainPermission 전체가 아니며 operation/alive 근거를 대신하지 않는다. 함수가 producer 정책을 재평가하지 않는다.

### 정상 경로와 commit

C2 초기화·인자/품질 코드·현재 TxPort 준비를 확인한다. 원 품질과 SignalMeta를 손실 없이 보관할 수 있는 기존 typed TxPort에 인계한다. quality와 meta.quality의 충돌은 진단 근거를 보존하여 composer가 제한하도록 하며 임의 OK를 선택하지 않는다. TxPort가 전체 입력과 수용 시각을 bounded 독립 저장소에 commit했을 때에만 DOMAIN_IF_OK를 반환한다. 안정 입력 인계 이후의 composer 실패가 소급하여 입력 admission 실패가 되지는 않는다. Application이 DomainNet_PublishPermission을 함께 호출할 의무는 없다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| P01 | C2 초기화 전 | DOMAIN_IF_NOT_INITIALIZED |
| P02 | NULL·예약 품질 코드·형식/범위 오류 | DOMAIN_IF_INVALID_ARGUMENT; 새 입력 commit 없음 |
| P03 | STALE/INVALID/NO_DATA 또는 quality/meta.quality 충돌의 정상 형식 | 원 값과 두 품질을 보존하는 로컬 인계 가능; 이를 ALLOW 근거로 사용하지 않음 |
| P04 | 등록 TxPort·원 시각 수용 기준·전체 보관 binding 없음 | DOMAIN_IF_NOT_AVAILABLE; 원 주소 보관·부분 성공 없음 |
| P05 | 로컬 인계 결과 | 기존 TxPort의 명시 DomainIf_Status_t를 그대로 분기. Net enum 숫자 cast 없음 |
| P06 | 새 전원 입력 안정 commit 완료 | DOMAIN_IF_OK는 이 입력의 로컬 수용. 완성 Permission Wire/TX/4개 수신/차량 ALLOW 비증명 |
| P07 | 같은 원 입력 반복 또는 composer 입력들 간 세대·시점 차이 | 원 meta.ageMs/updateSequence 고정; 필수 입력/coherence/원 기한 미합의는 composer BLOCKED_BY |

### 시간·소유권·동시성·복구

C2 Task-only 비차단 호출이다. 실제 TxPort는 OK 전에 전체 필드와 수용 시각을 복사하며 borrowed 주소를 남기지 않는다. meta.ageMs는 원 판단부터의 u32 경과이고 이후 인계·composer·TX 대기를 한 번씩 누적한다. 서로 다른 입력의 수용 시각을 원 생성 시각 동일성으로 사용하지 않는다. 수치 150 ms는 SOURCE_LIMIT_NOT_AGREED이며 원 producer가 없는 age0을 만들지 않는다. 독립 입력 슬롯과 동시성의 실제 구현은 BOUNDARY_PENDING_U4D다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-C2-SOURCE` | DOMAIN/C2: 정확한 target 헤더와 등록 TxPort 구현 |
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/CIS/보류 WINDOW 기능 owner: 원 producer/필수 입력/coherence/원 기한 |
| `BLOCKED_BY: OPEN-PROVENANCE` | DOMAIN/Network: meta와 내부 원 계보·수용시각의 연결 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
