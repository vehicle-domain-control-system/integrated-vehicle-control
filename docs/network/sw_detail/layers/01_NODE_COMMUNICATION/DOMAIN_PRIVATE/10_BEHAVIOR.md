# DOMAIN — U2 동작 설명

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

<a id="fn-domain-domainnet_publishpermission"></a>
## FN-DOMAIN-DomainNet_PublishPermission — `DomainNet_PublishPermission()`

[의사코드](20_PSEUDOCODE.md#fn-domain-domainnet_publishpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-domain-domainnet_publishpermission) · [원 선언](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnet_publishpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);`

**현재 지위:** `PROPOSED_DESIGN / NETWORK_PRIVATE / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `f99f1af5271c677e14c68bfbf3e9ab9d8f8ca8a68d62d5f1777eb9c122394cb2`.

### 책임·인자·호출

기존 Network 내부 composer → DOMAIN Network stable handoff → 단일 CAN DOMAIN_PERMISSION → CIS/BCM/VSS 및 보류 WINDOW. 공개 Application API를 신설하지 않는다.

`message`는 const NetDomainPermission IN/borrowed, OUT 없음. 7개 필드의 원 값·독립 품질·합성 update_sequence·공통 source_age_ms와 내부 보존 provenance가 일치해야 한다. 이 함수는 독립 C2 3종을 인자로 받지 않으므로 합성 정책 자체를 본체 안으로 숨기지 않는다.

### 정상 경로와 commit

기존 composer는 전원/operation/alive 원 입력을 독립 보관하고 동일 t_eval에서 한 번 고정해 읽는다. 읽기 중 tuple이 바뀌면 결과를 제출하지 않고 다음 owner 진행에서 다시 평가한다. 필요한 생성원·원 기한·coherence가 없는 허용 분기는 BLOCKED_BY로 남긴다. 확인 가능한 원 AGE 최대값 포화는 기존 합성 후보이며 없는 근거의 AGE를 지어내지 않는다. 이 함수는 준비된 typed 결과의 구조·원 provenance·인계 가능성을 검증하고 전체 bounded 복사/기준점 commit 후에만 OK를 반환한다. 근거가 없는 ALLOW 후보는 수용하지 않고 NOT_READY로 제한한다. 정상 형식의 불허/INVALID/STALE/NO_DATA 진단 record는 원 근거가 있는 범위에서 인계할 수 있다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| C01 | NULL·예약 code·불법 flag/품질·필드 조합 | NET_SUBMIT_INVALID_ARGUMENT |
| C02 | ALLOW 의존 필수 근거/원 기한/coherence 미확인 | NET_SUBMIT_NOT_READY; BLOCKED_BY OPEN-PERM, ALLOW 생성 없음 |
| C03 | 원 AGE 또는 합성 UPDATE와 내부 원 입력 연결 미확인 | NET_SUBMIT_NOT_READY; age0/65535를 근거 없이 만들어 보완하지 않음 |
| C04 | 정상 형식의 확인된 불허·열화·NO_DATA | 실제 존재하는 시간근거를 보존하여 diagnostic-only로 수용 가능; 정책 DENY를 새로 합성하지 않음 |
| C05 | 현재 링크/clock/인계 binding 미준비 또는 유한 공간 없음 | 각각 NET_SUBMIT_NOT_READY / NET_SUBMIT_NO_CAPACITY; partial admission 없음 |
| C06 | 전체 안정 인계 commit 완료 | NET_SUBMIT_OK; 네 소비자 실제 수신 완료 비증명 |
| C07 | composer tuple 변경·전송 반복·late terminal·reset | 원 tuple/의미 변할 때만 합성 UPDATE 후보 변경. 반복TX로 AGE0/새 UPDATE 없음 |

### 시간·소유권·동시성·복구

source_age_ms는 확인 가능한 원 근거를 동일 t_eval까지 한 번 누적한 후보다. u32→u16은 modulo가 아닌 포화이며 필드별 원 기한을 공유 AGE가 입증하지 못하면 사용을 제한한다. 값 없는 입력은 별도 품질로 표시하며 임의 AGE를 만들지 않는다. 메시지와 provenance·시각은 같은 snapshot 경계로 인계해야 한다. 구체 슬롯/큐·capacity·락·wrap·TX retry는 BOUNDARY_PENDING_U4D다. WINDOW operation/alive는 DEFERRED_REFERENCE로 근거만 연결하고 구현하지 않는다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/CIS/보류 WINDOW: 필수 입력·원 producer·150 ms source limit/coherence·state 생성원 |
| `BLOCKED_BY: OPEN-PROVENANCE` | DOMAIN/Codec/U4D: 3입력 tuple·공통 AGE/UPDATE와 내부 원 메타 연결 |
| `BLOCKED_BY: OPEN-WIRE / OPEN-PORT-CAPABILITY` | 양단 Network/B-track/U10: 후보 채택·인계 능력·terminal 관측 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
