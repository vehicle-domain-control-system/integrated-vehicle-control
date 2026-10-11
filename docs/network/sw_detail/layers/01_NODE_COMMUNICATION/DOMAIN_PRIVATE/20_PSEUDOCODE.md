# DOMAIN — U2 한국어 C형 의사코드

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

코드 블록의 한국어 조건·단계·객체명은 의미 설명용이며 실제 helper/변수/SDK 심볼 또는 새 ABI가 아니다. 함수의 반환형·이름·전체 인자는 원형 그대로다. 상태·공간·인계 완료 조건이 실제 후속 binding에서 성립한 경우만 정상 return에 도달한다. 상세 조건/오류·commit의 뜻은 같은 FN-ID 설명서를 따른다. 수치 없는 bounded 용량은 무한 공간이나 항상 성공을 뜻하지 않는다.

<a id="fn-domain-domainnet_publishpermission"></a>
## FN-DOMAIN-DomainNet_PublishPermission — `DomainNet_PublishPermission()`

[동작 설명](10_BEHAVIOR.md#fn-domain-domainnet_publishpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-domain-domainnet_publishpermission) · [원 선언](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnet_publishpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);`

**현재 지위:** `PROPOSED_DESIGN / NETWORK_PRIVATE / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `f99f1af5271c677e14c68bfbf3e9ab9d8f8ca8a68d62d5f1777eb9c122394cb2`.

```c
/* FN-DOMAIN-DomainNet_PublishPermission: 원형을 보존한 문서상 설계 후보. */
NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message)
{
    if (message == NULL || 기존_코드범위_또는_필드조합_구조오류) { /* C01 */
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    if (이_record의_허용값이_미확인_필수근거에_의존함) { /* C02 */
        /* BLOCKED_BY: OPEN-PERM. 완성된 ALLOW를 가장해 인계하지 않는다. */
        return NET_SUBMIT_NOT_READY;
    }
    if (!원_입력tuple_및_확인가능한_AGE와_UPDATE_연결) { /* C03 */
        /* BLOCKED_BY: OPEN-PROVENANCE. 없는 source AGE를 0/65535로 생성하지 않는다. */
        return NET_SUBMIT_NOT_READY;
    }
    /* C04 단계: 원 불허/UNKNOWN·품질 열화는 보존한다. 품질 숫자 max나 bool 승격 없음. */
    if (!clock_및_현재_로컬_인계경계_준비) { /* C05 */
        return NET_SUBMIT_NOT_READY;
    }
    if (유한_독립저장공간을_확보할_수_없음) {
        return NET_SUBMIT_NO_CAPACITY;
    }
    /* C06 단계: 7필드·실제 원근거 연결·인계시각을 잠정 복사한다. borrowed 주소 저장 금지. */
    if (!전체_snapshot의_안정인계_commit_조건) {
        /* 단계: 잠정 예약 해제, 기존 pending/진행 중 TX 보존. */
        return NET_SUBMIT_NOT_READY;
    }
    /* 단계: bounded Network 소유권 commit. 저장/교체/동시성은 BOUNDARY_PENDING_U4D. */
    /* C07: 합성은 본 함수 이전 composer 경계. tuple 변경 중 결과는 제출하지 않는다. */
    /* 반복 TX/late callback/reset으로 원 AGE·UPDATE 또는 기능 허용을 새로 만들지 않는다. */
    return NET_SUBMIT_OK;
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| C01 — NULL·예약 code·불법 flag/품질·필드 조합 | 코드의 `C01` 표식 | NET_SUBMIT_INVALID_ARGUMENT |
| C02 — ALLOW 의존 필수 근거/원 기한/coherence 미확인 | 코드의 `C02` 표식 | NET_SUBMIT_NOT_READY; BLOCKED_BY OPEN-PERM, ALLOW 생성 없음 |
| C03 — 원 AGE 또는 합성 UPDATE와 내부 원 입력 연결 미확인 | 코드의 `C03` 표식 | NET_SUBMIT_NOT_READY; age0/65535를 근거 없이 만들어 보완하지 않음 |
| C04 — 정상 형식의 확인된 불허·열화·NO_DATA | 코드의 `C04` 표식 | 실제 존재하는 시간근거를 보존하여 diagnostic-only로 수용 가능; 정책 DENY를 새로 합성하지 않음 |
| C05 — 현재 링크/clock/인계 binding 미준비 또는 유한 공간 없음 | 코드의 `C05` 표식 | 각각 NET_SUBMIT_NOT_READY / NET_SUBMIT_NO_CAPACITY; partial admission 없음 |
| C06 — 전체 안정 인계 commit 완료 | 코드의 `C06` 표식 | NET_SUBMIT_OK; 네 소비자 실제 수신 완료 비증명 |
| C07 — composer tuple 변경·전송 반복·late terminal·reset | 코드의 `C07` 표식 | 원 tuple/의미 변할 때만 합성 UPDATE 후보 변경. 반복TX로 AGE0/새 UPDATE 없음 |

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/CIS/보류 WINDOW: 필수 입력·원 producer·150 ms source limit/coherence·state 생성원 |
| `BLOCKED_BY: OPEN-PROVENANCE` | DOMAIN/Codec/U4D: 3입력 tuple·공통 AGE/UPDATE와 내부 원 메타 연결 |
| `BLOCKED_BY: OPEN-WIRE / OPEN-PORT-CAPABILITY` | 양단 Network/B-track/U10: 후보 채택·인계 능력·terminal 관측 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
