# VSS — U2 동작 설명

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

<a id="fn-vss-vssnetpriv_ondomainpermission"></a>
## FN-VSS-VssNetPriv_OnDomainPermission — `VssNetPriv_OnDomainPermission()`

[의사코드](20_PSEUDOCODE.md#fn-vss-vssnetpriv_ondomainpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-vss-vssnetpriv_ondomainpermission) · [원 선언](../../../../interface/02_NODES/VSS/20_API.md#api-vssnetpriv_ondomainpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);`

**현재 지위:** `PROPOSED_PRIVATE / NETWORK_PRIVATE / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `a0b225e56c917b0848a0f2adfab1a862b5472bc5c7958dcca1f5cc67f2421a06`.

### 책임·인자·호출

VSS CAN의 검증된 수신 경로 → VSS Network DOMAIN 감독. 음향 시작·우선순위·정지·복구 허용은 VSS 기능 owner다.

`message`와 `meta`는 IN/read-only, callback 반환까지 borrowed, OUT 없음. length/enum/generation/순서 검증된 DOMAIN 원 record와 로컬 RX 메타를 감독이 소비한다. 반환 뒤 보존할 경우 원 값과 메타의 전체 사본이 필요하다.

### 정상 경로와 commit

구조·수용 문맥을 확인하고 원 품질·generation·UPDATE·AGE와 로컬 DOMAIN 수신 관측을 감독에 전달한다. 현재 VSS App permission ingress는 필수 소비자가 아니며 새 공개 callback을 추가하지 않는다. 이 record를 VssRecoveryPermission이나 새로운 음향 정책 입력으로 자동 변환하지 않는다. 정상 열화 상태도 품질대로 저장할 수 있으며 내부 감독 commit을 음향 실행 성공으로 보고하지 않는다.

### 실패·경계와 결과

| 분기 | 조건 | 처리·반환·commit 경계 |
|---|---|---|
| V01 | NULL·controller 오류/length 검사 이후 남은 typed 구조 오류 | 격리/진단 후 return; 새 정상 감독·음향 결과 없음. 응용 CRC 추가 없음 |
| V02 | 수용되지 않은 DOMAIN generation·역전/재생·원 UPDATE 충돌 | 현재 감독 자료 보존; 새 허용/회복으로 해석하지 않음 |
| V03 | 원 품질 INVALID/STALE/NO_DATA 또는 만료 | 원 열화 의미 유지. CLEAR/DONE/READY/Fault없음으로 보정하지 않음 |
| V04 | 감독 binding·유한 사본 공간 미준비 | 진단 후 return; void는 보관 성공 반환이 아님 |
| V05 | 전체 감독 사본 안정 commit | 검증된 DOMAIN 수신 감독만 반영; 기능 App 수용·음향 시작/완료 비증명 |
| V06 | 통신 상실/재기동/late callback | 타이머/generation owner가 감독을 제한; AUDIO_OUTPUT_FAILURE 합성·자동 복구 허용 없음 |

### 시간·소유권·동시성·복구

로컬 RX 시각은 peer 감독, SOURCE_AGE는 원 판단 최신성을 나타내며 혼동하지 않는다. 같은 원 UPDATE 재수신은 AGE를 낮추지 않는다. callback은 비차단 owner 처리 문맥이며 consumer가 값·identity·시각을 함께 복사한다. SDK callback/worker·bounded cache·wrap은 BOUNDARY_PENDING_U4D 및 타깃 미검증이다. VSS 음향 정책과 복구 주체의 실행은 이번 범위 밖이다.

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-PROVENANCE` | VSS/Network/U4D: generation/순서/AGE 수용·감독 연결 |
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/VSS 기능 owner: 원 producer·기한·실제 App 소비 필요성 |
| `BLOCKED_BY: OPEN-SDK/HW/TIME` | VSS/B-track/U10: 실제 시간·이벤트·자원 측정 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
