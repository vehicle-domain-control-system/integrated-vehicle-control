# BCM — U2 한국어 C형 의사코드

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

코드 블록의 한국어 조건·단계·객체명은 의미 설명용이며 실제 helper/변수/SDK 심볼 또는 새 ABI가 아니다. 함수의 반환형·이름·전체 인자는 원형 그대로다. 상태·공간·인계 완료 조건이 실제 후속 binding에서 성립한 경우만 정상 return에 도달한다. 상세 조건/오류·commit의 뜻은 같은 FN-ID 설명서를 따른다. 수치 없는 bounded 용량은 무한 공간이나 항상 성공을 뜻하지 않는다.

<a id="fn-bcm-bcmnetpriv_ondomainpermission"></a>
## FN-BCM-BcmNetPriv_OnDomainPermission — `BcmNetPriv_OnDomainPermission()`

[동작 설명](10_BEHAVIOR.md#fn-bcm-bcmnetpriv_ondomainpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnetpriv_ondomainpermission) · [원 선언](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnetpriv_ondomainpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `void BcmNetPriv_OnDomainPermission( const NetDomainPermission *permission, const NetCanRxMeta *meta );`

**현재 지위:** `PROPOSED_PRIVATE / NETWORK_PRIVATE / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `1fca4f3d4fca86a6e32e7c7f0e77b23bcb363aeb28a69488aa639731a4cce809`.

```c
/* FN-BCM-BcmNetPriv_OnDomainPermission: 원형을 보존한 문서상 설계 후보. */
void BcmNetPriv_OnDomainPermission( const NetDomainPermission *permission, const NetCanRxMeta *meta )
{
    if (permission == NULL || meta == NULL || typed_구조오류) { /* B01 */
        /* 단계: 진단만 남기고 DOMAIN 정상 수신 시각을 갱신하지 않는다. */
        return;
    }
    if (!수용된_DOMAIN_세대 || 역전_재생_또는_원UPDATE_충돌) { /* B02 */
        /* BLOCKED_BY: OPEN-PROVENANCE; 과거 generation의 감독 자료로 현 세대를 갱신하지 않는다. */
        return;
    }
    /* B03: 정상 형식의 원 열화와 오래된 허용을 보존한다. */
    /* 구조적으로 정상인 DOMAIN frame의 링크 관측은 원 기능 허용 최신성과 별개다. */
    if (!기존_내부감독_연결 || 전체_사본의_유한공간이_없음) { /* B04 */
        /* 단계: 기존 진단 경계에 인계 불가를 관측, 부분 저장은 하지 않는다. */
        return;
    }
    /* B05 단계: 원 record·DOMAIN Boot/TX·수신시각을 일관된 내부 감독 사본으로 commit. */
    /* 실제 검증된 DOMAIN 수신 관측만 반영한다. 다른 ECU CAN activity로 대체하지 않는다. */
    /* B06: 상실/reset/late 사건은 기존 owner 연결이 필요. 자동 보호해제·App permission·복구 owner 신설 없음. */
    /* BLOCKED_BY: OPEN-PERM / OPEN-SDK/HW/TIME; 저장/동시성은 BOUNDARY_PENDING_U4D */
    return;
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| B01 — NULL·불법 코드·구조 오류 | 코드의 `B01` 표식 | 격리/로컬 진단 후 return; DOMAIN 정상 수신 시각 갱신 없음 |
| B02 — 기존 DOMAIN generation 불일치·순서 역전/재생 | 코드의 `B02` 표식 | 원 감독 자료 보존·격리, 새 generation admission은 별도 owner 규약 |
| B03 — 정상 frame의 열화 품질·오래된 원 허용 | 코드의 `B03` 표식 | 링크 생존과 허용 최신성 분리; 실제 정책 ALLOW·차량 FAILED 합성 없음 |
| B04 — 감독 연결·bounded 사본 공간 부재 | 코드의 `B04` 표식 | 진단 후 return; 부분 commit과 void 성공 주장 없음 |
| B05 — 정상 DOMAIN RX와 전체 감독 사본 commit | 코드의 `B05` 표식 | DOMAIN peer의 로컬 정상 수신 관측만 갱신; BCM App permission 실행은 없음 |
| B06 — bus-off·RX 중단·DOMAIN reset·late callback | 코드의 `B06` 표식 | 기존 관측과 generation을 분리 보존; owner의 시간경과/무효화 경계 필요, 보호 해제 자동화 없음 |

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PROVENANCE` | BCM/Network/U4D: Boot/TX/원 UPDATE의 수용·reset/wrap 연결 |
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/기능 owner: Permission 원 producer·기한, App 추가 소비는 미확인 |
| `BLOCKED_BY: OPEN-SDK/HW/TIME` | BCM/B-track/U10: 실제 감독 이벤트·시간/자원 binding |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
