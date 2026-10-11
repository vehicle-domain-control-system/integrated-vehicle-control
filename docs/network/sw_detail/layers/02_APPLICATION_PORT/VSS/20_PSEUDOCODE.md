# VSS — U2 한국어 C형 의사코드

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

코드 블록의 한국어 조건·단계·객체명은 의미 설명용이며 실제 helper/변수/SDK 심볼 또는 새 ABI가 아니다. 함수의 반환형·이름·전체 인자는 원형 그대로다. 상태·공간·인계 완료 조건이 실제 후속 binding에서 성립한 경우만 정상 return에 도달한다. 상세 조건/오류·commit의 뜻은 같은 FN-ID 설명서를 따른다. 수치 없는 bounded 용량은 무한 공간이나 항상 성공을 뜻하지 않는다.

<a id="fn-vss-vssnetpriv_ondomainpermission"></a>
## FN-VSS-VssNetPriv_OnDomainPermission — `VssNetPriv_OnDomainPermission()`

[동작 설명](10_BEHAVIOR.md#fn-vss-vssnetpriv_ondomainpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-vss-vssnetpriv_ondomainpermission) · [원 선언](../../../../interface/02_NODES/VSS/20_API.md#api-vssnetpriv_ondomainpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);`

**현재 지위:** `PROPOSED_PRIVATE / NETWORK_PRIVATE / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `a0b225e56c917b0848a0f2adfab1a862b5472bc5c7958dcca1f5cc67f2421a06`.

```c
/* FN-VSS-VssNetPriv_OnDomainPermission: 원형을 보존한 문서상 설계 후보. */
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)
{
    if (message == NULL || meta == NULL || typed_구조오류) { /* V01 */
        /* 단계: 로컬 진단만 남기며 정상 DOMAIN 감독 자료를 갱신하지 않는다. */
        return;
    }
    if (!수용된_DOMAIN_세대 || 역전_재생_또는_원UPDATE_충돌) { /* V02 */
        /* BLOCKED_BY: OPEN-PROVENANCE */
        return;
    }
    /* V03 단계: 원 값·품질·UPDATE·AGE는 그대로 보존한다. */
    /* 만료/UNKNOWN을 CLEAR·DONE·READY·음향 복구 허용으로 바꾸지 않는다. */
    if (!기존_내부감독_연결 || 전체_사본의_유한공간이_없음) { /* V04 */
        /* 단계: 인계 미준비/용량 문제를 기존 진단 경계에서 관측한다. */
        return;
    }
    /* V05 단계: record·DOMAIN Boot/TX·로컬 RX시각을 내부 감독 사본으로 함께 commit. */
    /* V06: callback 밖의 상실/reset 감독은 기존 owner와 binding을 확인한다. */
    /* App permission callback, VssRecoveryPermission 변환, AUDIO_OUTPUT_FAILURE 생성 없음. */
    /* BLOCKED_BY: OPEN-PERM / OPEN-SDK/HW/TIME; 세부 저장은 BOUNDARY_PENDING_U4D */
    return;
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| V01 — NULL·controller 오류/length 검사 이후 남은 typed 구조 오류 | 코드의 `V01` 표식 | 격리/진단 후 return; 새 정상 감독·음향 결과 없음. 응용 CRC 추가 없음 |
| V02 — 수용되지 않은 DOMAIN generation·역전/재생·원 UPDATE 충돌 | 코드의 `V02` 표식 | 현재 감독 자료 보존; 새 허용/회복으로 해석하지 않음 |
| V03 — 원 품질 INVALID/STALE/NO_DATA 또는 만료 | 코드의 `V03` 표식 | 원 열화 의미 유지. CLEAR/DONE/READY/Fault없음으로 보정하지 않음 |
| V04 — 감독 binding·유한 사본 공간 미준비 | 코드의 `V04` 표식 | 진단 후 return; void는 보관 성공 반환이 아님 |
| V05 — 전체 감독 사본 안정 commit | 코드의 `V05` 표식 | 검증된 DOMAIN 수신 감독만 반영; 기능 App 수용·음향 시작/완료 비증명 |
| V06 — 통신 상실/재기동/late callback | 코드의 `V06` 표식 | 타이머/generation owner가 감독을 제한; AUDIO_OUTPUT_FAILURE 합성·자동 복구 허용 없음 |

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PROVENANCE` | VSS/Network/U4D: generation/순서/AGE 수용·감독 연결 |
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/VSS 기능 owner: 원 producer·기한·실제 App 소비 필요성 |
| `BLOCKED_BY: OPEN-SDK/HW/TIME` | VSS/B-track/U10: 실제 시간·이벤트·자원 측정 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
