# CIS — U2 한국어 C형 의사코드

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

코드 블록의 한국어 조건·단계·객체명은 의미 설명용이며 실제 helper/변수/SDK 심볼 또는 새 ABI가 아니다. 함수의 반환형·이름·전체 인자는 원형 그대로다. 상태·공간·인계 완료 조건이 실제 후속 binding에서 성립한 경우만 정상 return에 도달한다. 상세 조건/오류·commit의 뜻은 같은 FN-ID 설명서를 따른다. 수치 없는 bounded 용량은 무한 공간이나 항상 성공을 뜻하지 않는다.

<a id="fn-cis-cisnet_publishrear"></a>
## FN-CIS-CisNet_PublishRear — `CisNet_PublishRear()`

[동작 설명](10_BEHAVIOR.md#fn-cis-cisnet_publishrear) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-cis-cisnet_publishrear) · [원 선언](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishrear) · [CIS_REAR](../../../flows/10_CIS_REAR_FLOW.md)

**원형:** `NetSubmitResult CisNet_PublishRear( const NetCisRear *message );`

**현재 지위:** `PROPOSED_DESIGN / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `7db6c5c3dacac4b39c2b28b030e40c1fd60b44fe200a69b7568e8f6da57cf5f6`.

```c
/* FN-CIS-CisNet_PublishRear: 원형을 보존한 문서상 설계 후보. */
NetSubmitResult CisNet_PublishRear( const NetCisRear *message )
{
    if (message == NULL) { /* R01 */
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    if (기존_표현범위_또는_필드조합_검사에서_구조오류) { /* R02 */
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    switch (message->proximity_status) { /* R03: 원 코드, 상태별 품질 보존 */
    case NET_PROXIMITY_VALID_DISTANCE:
        /* 단계: 실제 거리와 원 validity/reason을 함께 유지한다. */
        break;
    case NET_PROXIMITY_NO_OBJECT:
        /* 단계: 유효한 무검출을 측정오류·위험 CLEAR와 구분한다. */
        break;
    case NET_PROXIMITY_UNAVAILABLE:
    case NET_PROXIMITY_FAULT:
    case NET_PROXIMITY_RECOVERING:
    case NET_PROXIMITY_INACTIVE:
        /* 단계: 열화 의미와 원 품질을 유지한다. 65535를 실제 거리로 쓰지 않는다. */
        break;
    default:
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    if (원_관측_시간근거_또는_로컬_인계경계가_미준비) { /* R04 */
        /* BLOCKED_BY: OPEN-PROVENANCE / OPEN-PORT-CAPABILITY */
        return NET_SUBMIT_NOT_READY;
    }
    if (유한_독립저장공간을_확보할_수_없음) { /* R05 */
        return NET_SUBMIT_NO_CAPACITY;
    }
    /* R06 단계: 원 필드 전체와 인계시각을 잠정 저장한다. caller 주소를 보관하지 않는다. */
    if (전체_복사와_기준점의_원자적_commit_조건이_충족되지_않음) {
        /* 단계: 잠정 예약을 해제한다. 이전 pending/진행 중 TX는 그대로 보존한다. */
        return NET_SUBMIT_NOT_READY;
    }
    /* 단계: bounded Network 소유권 commit. 구체 저장/동시성은 BOUNDARY_PENDING_U4D. */
    /* R07: 미전송 current pending의 교체만 가능. 재제출로 UPDATE_SEQUENCE/원 AGE를 재설정하지 않는다. */
    return NET_SUBMIT_OK;
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| R01 — NULL | 코드의 `R01` 표식 | NET_SUBMIT_INVALID_ARGUMENT, 새 자료 commit 없음 |
| R02 — 예약 validity/reason/proximity 코드·명시 범위 밖 필드 조합 | 코드의 `R02` 표식 | NET_SUBMIT_INVALID_ARGUMENT; 알려진 INVALID/STALE 보고와 구별 |
| R03 — NO_OBJECT 또는 UNAVAILABLE/FAULT/RECOVERING/INACTIVE의 정상 형식 | 코드의 `R03` 표식 | 원 상태·품질·거리 sentinel 보존, 후방 CLEAR 또는 0 mm 합성 없음 |
| R04 — 원 시점/순번·기동 연결 또는 stable handoff 경계 미준비 | 코드의 `R04` 표식 | NET_SUBMIT_NOT_READY; 원 자료는 caller가 소유 |
| R05 — 유한 인계 공간 없음 | 코드의 `R05` 표식 | NET_SUBMIT_NO_CAPACITY; 기존 자료/진행 중 TX를 부분 덮어쓰지 않음 |
| R06 — 전체 복사·시각 기준점·소유권 commit 완료 | 코드의 `R06` 표식 | NET_SUBMIT_OK; 실제 CAN submit/terminal/수신/차량 결과는 비증명 |
| R07 — 동일 원 평가의 재제출·이미 진행 중인 TX·링크 재기동 | 코드의 `R07` 표식 | 새 관측으로 재라벨하지 않음. 미전송 current pending 교체만 기존 계약 후보, 상세는 U4D |

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

[동작 설명](10_BEHAVIOR.md#fn-cis-cisapp_ondomainpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-cis-cisapp_ondomainpermission) · [원 선언](../../../../interface/02_NODES/CIS/20_API.md#api-cisapp_ondomainpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `void CisApp_OnDomainPermission( const NetDomainPermission *message, const NetCanRxMeta *meta );`

**현재 지위:** `PROPOSED_DESIGN / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `7db6c5c3dacac4b39c2b28b030e40c1fd60b44fe200a69b7568e8f6da57cf5f6`.

```c
/* FN-CIS-CisApp_OnDomainPermission: 원형을 보존한 문서상 설계 후보. */
void CisApp_OnDomainPermission( const NetDomainPermission *message, const NetCanRxMeta *meta )
{
    if (message == NULL || meta == NULL || typed_구조오류) { /* A01 */
        /* 단계: 입력 오류를 로컬 관측한다. 허용·App cache·liveness는 갱신하지 않는다. */
        return;
    }
    if (!수용된_DOMAIN_세대 || 역전_재생_또는_원UPDATE_충돌) { /* A02 */
        /* BLOCKED_BY: OPEN-PROVENANCE; 상세 순서/wrap은 BOUNDARY_PENDING_U4D */
        return;
    }
    /* A03 단계: 원 power 값/품질·공통 AGE와 meta를 같은 자료로 취급한다. */
    if (message->vehicle_power_permission != 1 || power_품질이_비유효) {
        /* 단계: 기존 0=불허, 255=확인불가의 원 값과 품질을 보존한다. 허용 사용은 제한한다. */
    } else if (원_producer_기한_coherence를_입증하지_못함) { /* A04 */
        /* BLOCKED_BY: OPEN-PERM. 자료 자체는 보존하되 실제 허용 사용 경로를 열지 않는다. */
    } else {
        /* 단계: 기존 기능 owner에 근거를 전달할 수 있는 조건일 뿐, Network가 ALLOW를 판정하지 않는다. */
    }
    if (!기존_소비자_연결 || 반환후_필요한_전체자료_복사공간이_없음) { /* A05 */
        /* 단계: 인계 실패를 기존 진단 경계에 남긴다. 부분 저장/성공 보고를 하지 않는다. */
        return;
    }
    /* 단계: 값·품질·원 UPDATE/AGE·meta.source·received_at_ms의 독립 사본을 함께 commit. */
    /* A06: 같은 원 UPDATE의 새 TX는 원 평가를 갱신하지 않는다. */
    /* callback 밖의 aging/reset은 기존 owner가 처리. Pi relay/전원 정책을 여기 구현하지 않는다. */
    return;
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| A01 — NULL·예약 코드·형식 오류 | 코드의 `A01` 표식 | 로컬 입력 오류 관측 후 return; App 자료/liveness/허용 갱신 없음 |
| A02 — 수용되지 않은 DOMAIN Boot 또는 중복/역전 TX·원 UPDATE 충돌 | 코드의 `A02` 표식 | 격리/진단, 과거 허용을 최신으로 재사용하지 않음; 상세 순서규약 U4D |
| A03 — power=0/확인불가 또는 STALE/INVALID/NO_DATA | 코드의 `A03` 표식 | 원 값을 보존하고 허용 사용을 제한; 0/OK는 확인된 원 불허일 때만 그 의미 |
| A04 — power 허용처럼 보이나 원 producer/기한/coherence를 입증 못함 | 코드의 `A04` 표식 | BLOCKED_BY OPEN-PERM; 자료 인계와 실제 허용 사용을 분리 |
| A05 — 반환 후 저장 필요·저장 용량/consumer 미준비 | 코드의 `A05` 표식 | 전체 복사 미완료면 commit 없음; 진단·기존 연결로 전달, void를 성공으로 해석하지 않음 |
| A06 — 같은 원 UPDATE의 새 TX/late callback/통신 중단 | 코드의 `A06` 표식 | 링크 관측과 원 최신성 분리. 타이머 owner가 기존 자료를 aging, 합성 ALLOW/가짜 RX callback 없음 |

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/CIS: 원 producer·power 원 사용 기한·품질/coherence |
| `BLOCKED_BY: OPEN-PROVENANCE` | CIS/Network/U4D: meta와 원 UPDATE/AGE 연결·generation admission |
| `BLOCKED_BY: OPEN-PEER-OWNER` | CIS/Pi/U7: 동일 검증 입력 relay의 별도 계약; 이번 구현 제외 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
