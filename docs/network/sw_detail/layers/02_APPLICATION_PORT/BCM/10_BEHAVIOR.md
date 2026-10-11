# BCM — U2 동작 설명

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

<a id="fn-bcm-bcmnetpriv_ondomainpermission"></a>
## FN-BCM-BcmNetPriv_OnDomainPermission — `BcmNetPriv_OnDomainPermission()`

[의사코드](20_PSEUDOCODE.md#fn-bcm-bcmnetpriv_ondomainpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnetpriv_ondomainpermission) · [원 선언](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnetpriv_ondomainpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `void BcmNetPriv_OnDomainPermission( const NetDomainPermission *permission, const NetCanRxMeta *meta );`

**현재 지위:** `PROPOSED_PRIVATE / NETWORK_PRIVATE / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `1fca4f3d4fca86a6e32e7c7f0e77b23bcb363aeb28a69488aa639731a4cce809`.

### 책임·인자·호출

BCM CAN Dispatcher → BCM Network 내부 DOMAIN supervision. BCM 보호·진단의 실제 해석과 차량 실행은 기존 기능 owner가 맡는다.

`permission`과 `meta`는 const IN/borrowed, OUT 없음. 현재 수용된 DOMAIN Boot/TX·로컬 수신시각과 원 품질/AGE/UPDATE를 함께 보존한다. BCM App permission consumer를 새로 설치하지 않는다.

### 정상 경로와 commit

Dispatcher의 구조·DOMAIN generation 검증 이후 owner 문맥에서 들어온 record를 감독 경계에 인계한다. 다른 ECU의 공유 CAN frame을 DOMAIN 생존으로 세지 않는다. 구조·순서가 정상인 열화 record는 DOMAIN 링크 관측과 원 허용 무효를 분리해 보존할 수 있다. 필요한 supervision 사본의 안정 commit 후에는 내부 전달 사실만 관찰한다. 허용 상실·회복의 실제 의미는 기존 보호/진단 owner에 전달할 binding을 확인해야 하며 BCM 로컬 보호를 Network 왕복에 종속시키지 않는다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| B01 | NULL·불법 코드·구조 오류 | 격리/로컬 진단 후 return; DOMAIN 정상 수신 시각 갱신 없음 |
| B02 | 기존 DOMAIN generation 불일치·순서 역전/재생 | 원 감독 자료 보존·격리, 새 generation admission은 별도 owner 규약 |
| B03 | 정상 frame의 열화 품질·오래된 원 허용 | 링크 생존과 허용 최신성 분리; 실제 정책 ALLOW·차량 FAILED 합성 없음 |
| B04 | 감독 연결·bounded 사본 공간 부재 | 진단 후 return; 부분 commit과 void 성공 주장 없음 |
| B05 | 정상 DOMAIN RX와 전체 감독 사본 commit | DOMAIN peer의 로컬 정상 수신 관측만 갱신; BCM App permission 실행은 없음 |
| B06 | bus-off·RX 중단·DOMAIN reset·late callback | 기존 관측과 generation을 분리 보존; owner의 시간경과/무효화 경계 필요, 보호 해제 자동화 없음 |

### 시간·소유권·동시성·복구

받은 원 AGE를 수신시각으로 0부터 다시 시작하지 않는다. 정상 DOMAIN RX 시각은 링크 감독용이고 power/operation 원 기한은 미합의다. generation 변경 때 이전 허용을 새 허용으로 승계하지 않으며 늦은 자료는 현 감독 자료를 덮지 않는다. 최초 상실·회복 관측을 기존 기능 owner에 전달하는 포트와 aging 문맥은 BINDING_OPEN. callback 수명·유한 공간·원자성의 구현은 BOUNDARY_PENDING_U4D다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PROVENANCE` | BCM/Network/U4D: Boot/TX/원 UPDATE의 수용·reset/wrap 연결 |
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/기능 owner: Permission 원 producer·기한, App 추가 소비는 미확인 |
| `BLOCKED_BY: OPEN-SDK/HW/TIME` | BCM/B-track/U10: 실제 감독 이벤트·시간/자원 binding |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
