# N04 VSS — 확정된 경고·발생과 실제 음향 보고의 경계

[노드 경계 전체](../08_NODE_BOUNDARIES.md) · [노드·링크 지도](../02_NODE_LINK_MAP.md) · [Layer·Module·Function 색인](../03_LAYER_MODULE_INDEX.md) · [Interface 개요](../../interface/02_NODES/VSS/00_OVERVIEW.md) · [Data](../../interface/02_NODES/VSS/10_DATA.md) · [API](../../interface/02_NODES/VSS/20_API.md)

DOMAIN이 확정한 차량 의미를 별도 S32K344의 VSS에 전달하고 VSS의 실제 서비스·고장 근거를 되돌리는 경계다. Network는 후방 거리로 위험을 재판정하지 않고 음원 선택·재생 시작·완료도 대신 판단하지 않는다. **DRAFT_SHAREABLE / PROPOSED_DESIGN / BINDING_OPEN**이며 Warning은 A1 WARNING60 선택 후 **CF02 실제 양단 지원 검증 HOLD**를 유지한다.

## 적용 구조·Link와 정확한 기존 선언

N04의 P01은 공유 CAN 물리 경로, R03은 DOMAIN↔VSS 논리 관계, I04는 VSS App↔Network typed 경계다. L02/M03/F10/ST04가 로컬 인계를, L03/M04/F11~13/ST05~07이 사본·시간·순서를, L04/M05/F14~15가 Codec을, L05/M07~09/F19~25/ST09~12가 저장소·Driver·관측을 맡는다. P02~04의 직접 VSS 송수신과 VSS 전용 원격 Query RPC는 **NOT_APPLICABLE**이다. MOBILE의 VSS 표시는 DOMAIN의 기존 projection을 따른다.

| 정확한 기존 원형 | 원 생산자 → 호출·소비 Owner | Wire·하위 상세 |
|---|---|---|
| `void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);` | DOMAIN 위험·활성 판단 → VSS Network → VSS INPUT/App | [VSS_WARNING_STATE](../functions/can/VSS_WARNING_STATE.md), WARNING60 선택, 실제 양단 Binding HOLD |
| `void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);` | DOMAIN 확정 발생 → VSS Network → VSS INPUT/App | [VSS_EVENT](../functions/can/VSS_EVENT.md) |
| `NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);` | VSS HEALTH 보고 → FLOW → 기존 INPUT 통신 연결 → VSS Network | [VSS_STATUS](../functions/can/VSS_STATUS.md); 기존 INPUT 단일 변환 선택, E-S01~05 대기 |
| `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | DOMAIN 판단 → VSS Network 내부 감독 | [DOMAIN_PERMISSION](../functions/can/DOMAIN_PERMISSION.md), App 필수 API가 아님 |

네 원형은 Interface의 **EXISTING_DECLARATION**이며 모두 후보다. 실제 VSS R4/R6 함수·Header 설치의 증거로 사용하지 않는다. typed 구조체·전 필드·enum·Wire offset은 위 Data/메시지 정본을 재사용한다. Rx `message/meta`는 같은 검증 수신의 NULL 불가 IN 사본을 callback 동안 빌려 준다. 이후 저장은 소비자가 전체 필드·원 식별·시각을 복사한다. `void`에는 성공 반환값이 없으므로 실제 bounded 소비/복사 및 누락 관측 계약이 연결되기 전 정상 소비를 약속할 수 없다. ISR은 위 의미 API를 호출하지 않으며 해당 Owner Task/Superloop 직렬화가 선행 조건이다.

Status 입력도 NULL 불가 IN/borrowed다. `NET_SUBMIT_OK` 전에 Network 소유 전체 값과 원 근거·Queue 게시가 완료돼야 한다. INVALID_ARGUMENT/NOT_READY/NO_CAPACITY는 각각 형식/준비/공간 부족이며 새 자료 부분 수용은 없다. CAN 전송 성공·VSS 실제 출력·DOMAIN 반영은 반환으로 보장되지 않는다.

<a id="event-warning"></a>
## DOMAIN Event와 Warning의 서로 다른 사용 근거

| 입력 | Network가 보존하는 근거 | VSS App가 소유한 판단 |
|---|---|---|
| Event | 수용된 DOMAIN Boot + occurrence_id(필요한 PRODUCT/TEST 문맥 포함)가 Key; event_kind는 같은 Key 내용 검증. 원 original_age_ms/use_limit_ms/value_quality/input_availability, 선택 related_command_id 보존 | 같은 발생 중복 재생 방지, 최초 actual 원 L·품질·가용성 확인, 음향 중재 |
| Warning | 끼임·탑승자·후방 각각 source Boot/sequence/Age/quality/reason/availability | 유효 현재 위험에 따른 음향. 거리값으로 위험 재판정하지 않음 |
| 후방 활성 | `rear_detection_activation`과 별도 DOMAIN 평가 `rear_activation_sequence`, `rear_activation_age_ms`, `rear_activation_quality`, `rear_activation_reason` | 유효 활성과 유효 후방 위험을 함께 소비. 활성과 위험 CLEAR는 별개 |
| Permission | 원 DOMAIN 판단·기동·수신 근거 | 기본 소비자는 Network 내부 감독. VssRecoveryPermission으로 자동 변환하지 않음 |

수신은 기존 Codec → [공통 수신 순서](../functions/CAN_TRANSFER_CONTROL.md#rx-order) → 해당 typed 인계다. 잘못된 길이/코드/padding/기동은 격리하며 정상 liveness나 의미 freshness를 갱신하지 않는다. 같은 occurrence에 높은 CAN TX가 와도 새 Event를 만들지 않는다. Event가 한동안 없다는 사실은 통신 고장이 아니다. INVALID/STALE/NO_DATA 같은 형식상 유효한 품질 저하는 해당 상태로 전달할 수 있으나 새 재생·위험 CLEAR를 만들어서는 안 된다.

A1의 WARNING60은 used60/frame64/DLC15 단일 선택이며 WARNING51은 이력이다. DLC·B51 marker 자동 인식/fallback은 없다. format1은 독립 activation 근거를 검증하고 format0은 B50=0·B52..59=0인 정규 근거 부재다. 부재를 활성 OK/정상 CLEAR로 만들지 않고 다른 유효 위험은 보존한다. 실제 DOMAIN producer/VSS receiver·clock/직렬 owner 지원은 CF02/E01~05 검증 대기다. WINDOW 실행 HOLD는 그대로다.

최초 actual 기한은 원 발생부터 `L=min(검증된 양의 USE_LIMIT,500ms,2000ms)`이며 USE_LIMIT0은 사용 불가, L 도달은 만료다. 적법하게 시작한 동일 실행의 후속 cue마다 새 Start 기한을 부여하지 않는다. callback/INPUT/출력 대기는 원 Age에 포함하고 RX 기준 AGE와 경과를 중복 누적하지 않는다. 이 내부 시간 선택과 실제 first actual/clock 검증은 별개다.

입력 상실 뒤 일회 Pending·중단된 일회 음향은 자동 재개하지 않는다. Event 침묵 자체는 timeout이 아니고 실제 DOMAIN supervision·품질·계보·시각 상실을 구분한다. 회복 후 현재 유효 Warning을 독립 재중재하며 출력 중단은 차량 위험 CLEAR가 아니다. 출력 장치 Fault만 있었고 입력 신뢰/replayAllowed가 유지된 동일 occurrence는 전체 NO_START·retirement·실제 HEALTH 제한 해제·원 L 안을 모두 확인해야만 재평가한다. Started/Interrupted/uncertain final은 자동 resume하지 않는다. 실제 음향 정책·종료 결과는 VSS Owner 소유다.

<a id="status-flow"></a>
## VSS Status 생성 → 보호된 인계 → DOMAIN 투영

1. VSS HEALTH가 실제 서비스·현재/최근 고장과 원 평가 근거를 생성한다. 기존 FLOW→INPUT 통신 연결에서 보호된 자료를 전달한다. Network에 HEALTH가 직접 호출하는 새 경계나 진단 관리자를 만들지 않는다.
2. 완전 Snapshot→`NetVssStatus` 변환은 [A2 정본](../../interface/02_NODES/VSS/10_DATA.md#r2-status-proposal)의 기존 INPUT 한 곳으로 선택했다. HEALTH의 완전 current/history·최초 발생·평가/기동 근거를 보호하고 불완전/시간 불명은 REEVALUATE다. kind 목록 순서나 current mask0만으로 이력을 만들지 않으며 실제 E-S01~05 연결은 대기다.
3. 완전한 DTO가 확보된 경로만 `VssNet_PublishStatus`가 복사·접수한다. `vss_state`, `vss_availability`, `vss_accepting_events`는 서비스 보고이며 Event별 재생 시작/완료 응답이 아니다. `fault_mask/vss_fault_active`는 현재이고 `vss_last_fault/last_fault_occurrence/last_fault_age_ms/last_fault_quality`는 이력이다. 현재 고장이 해제되어도 최근 이력은 남을 수 있다.
4. M04는 송신 대기 경과를 현재 서비스 `source_age_ms`와 활성 최근이력 `last_fault_age_ms`에 각각 누적한다. `update_sequence`는 실제 새 평가에서만 변하며 CAN TX_SEQUENCE로 대체하지 않는다. 확정 불가한 원 시각은 AGE0으로 보충하지 않는다.
5. [기존 VSS_STATUS Codec](../functions/can/VSS_STATUS.md#codec)을 거쳐 불변 frame을 M07/M08에 전달한다. Driver 수명·재기동은 [Service 정본](../functions/NETWORK_SERVICE_LIFETIME.md)을 따른다. Driver accepted 또는 terminal TX는 상대의 Status 소비 성공을 뜻하지 않는다.
6. DOMAIN 수신은 기존 `DomainNetPriv_OnVssStatus`와 [Rx Adapter](../functions/DOMAIN_RX_ADAPTER.md)를 사용한다. `LAST_FAULT_QUALITY` raw0/3은 최근 기록의 존재/부재이며 C2 서비스 품질·전체 Fault 품질과 동등하지 않다. 필요한 원 품질·생산 Generation·같은 관측 연결이 미확인인 투영은 HOLD다. 값이 같다는 사실로 raw와 Getter snapshot을 결합하지 않는다.
7. [A2 per-K](../functions/can/VSS_STATUS.md#a2-domain-contract)대로 State→Fault의 미호출·원 raw·실제 수용/commit/callback을 독립 보존한다. 같은 K 반복 호출0, State 수용 뒤 Fault 미등록/실패로 State를 재실행하지 않는다. 불명 결과 자동 재호출은 금지하며 실제 profile·association·old K 후속 쓰기 차단이 없는 해당 경계만 HOLD한다. v0.5 service-only/observer-only는 확장 C2 전체 이력 소비가 아니다.

미확인 Status 부분을 READY/품질 정상/mask0로 채우지 않는다. 마지막 확정 보고는 원 평가/Age를 유지하는 이력일 뿐 새로운 fresh 근거가 아니다. 이 제한을 원본 근거가 독립인 모든 Event/Warning에 확대하지 않는다.

<a id="status-age-function"></a>
## 내부 의사코드 — Status의 두 Age를 같은 시점까지 누적

**PROPOSED_PRIVATE_FUNCTION**, L03/M04/F12/ST05의 좁은 조합 예다. 두 시간 의미를 혼동하지 않고 기존 [NetCan_AccumulateAge](../functions/CAN_TRANSFER_CONTROL.md#age)를 호출한다. 새로운 시간원·TTL·Status 변환 API를 정의하지 않는다.

```c
NetCanResult NetVssBoundary_AdvanceStatusAge(
    const NetVssStatus *source,
    uint32_t unaccountedLocalElapsedMs,
    bool elapsedKnown,
    NetVssStatus *out);

NetCanResult NetVssBoundary_AdvanceStatusAge(
    const NetVssStatus *source,
    uint32_t unaccountedLocalElapsedMs,
    bool elapsedKnown,
    NetVssStatus *out)
{
    if (source == NULL || out == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (source->last_fault_quality != 0 && source->last_fault_quality != 3)
        return NET_CAN_MALFORMED;
    if (source->last_fault_quality == 3 &&
        (source->vss_last_fault != 0 || source->last_fault_occurrence != 0 || source->last_fault_age_ms != 0))
        return NET_CAN_MALFORMED;
    if (source->last_fault_quality == 0 &&
        (source->vss_last_fault == 0 || source->vss_last_fault > 5 || source->last_fault_occurrence == 0))
        return NET_CAN_MALFORMED;
    NetVssStatus next = *source;
    bool serviceSaturated;
    NetCanResult rc = NetCan_AccumulateAge(source->source_age_ms,
        unaccountedLocalElapsedMs, elapsedKnown,
        &next.source_age_ms, &serviceSaturated);
    if (rc != NET_CAN_OK) return rc;
    if (source->last_fault_quality == 0) {
        bool historySaturated;
        rc = NetCan_AccumulateAge(source->last_fault_age_ms,
            unaccountedLocalElapsedMs, elapsedKnown,
            &next.last_fault_age_ms, &historySaturated);
        if (rc != NET_CAN_OK) return rc;
    }
    *out = next;
    return NET_CAN_OK; // 시간값 보완만 완료. 서비스 정상/송신 성공을 뜻하지 않는다.
}
```

IN은 호출 동안 불변인 Network 소유 Status 사본이다. OUT은 NULL 불가 caller 저장소이며 IN과 중첩하지 않는다. 실패 시 OUT 전체 불변, 성공 시 두 Age와 나머지 원 필드가 함께 반영된다. 두 Age는 **이미 동일한 로컬 기준점까지 반영된** 사본이어야 하고 `unaccountedLocalElapsedMs`는 그 기준 이후의 미반영 경과다. 각 Age 기준점이 다르거나 clock epoch/wrap·Driver 공백이 불명확하면 단일 elapsed를 전달해서는 안 되며 해당 시간 바인딩 HOLD다. `elapsedKnown`은 기존 공통 함수의 확인된 시간 산술 조건이며 팀 합의·기능 정상·Driver 완료를 승인하는 flag가 아니다.

최근 기록 부재3은 code/occurrence/이력 AGE0의 원 조합을 먼저 확인하고0을 유지한다. 비정상 조합을 보정하거나 임의 기본3을 만들지 않는다. quality0에만 원 이력 AGE를 누적하고 source AGE는 독립 누적한다. 65535는 포화 하한이며 이 함수의 OK는 전체 producer/공개 인계 검증이나 Queue 게시 성공을 뜻하지 않는다.

## 실패·재기동·HOLD와 반례

VSS reset 뒤 이전 DOMAIN Event를 새 occurrence로 재발급하거나 중단된 소리를 자동 재생하지 않는다. 새로운 source Boot 수용과 occurrence 보관 수명은 E05 실제 근거가 필요하다. 신뢰할 재시작 전 ledger 복원 또는 VSS 동기화 이후 DOMAIN 실제 새 승인 세대 전에는 Event admission을 닫으며 첫 수신 Boot를 새 세대로 추정하지 않는다. CIS Epoch 상태기계를 VSS에 복제하지 않는다. CAN 복구는 서비스 READY가 아니다. Status terminal failure는 RX/새 CREATED 유무와 무관하게 M09→기존 INPUT 진단→FLOW가 원 K/BOOT/TX/raw/release를 보존한 뒤 재수집한다. 실제 sink E-S04는 대기이며 DOMAIN C2 실패 API를 VSS에 복제하지 않는다. 고장 이력·counter를 정상 프레임 한 건으로 지우지 않는다.

| ID | 입력·사건 | 정적 기대·Owner |
|---|---|---|
| B16-VSS-01 / 필수5 | WARNING60 선택, 실제 양단 지원 증거 없음 | CF02 실행 CONTRACT_HOLD. WARNING51·길이/marker 자동 추정 없음 |
| B16-VSS-02 | Event 원 Age가 L=500ms에 도달,2초 미만 | 최초 actual 불가. 원 L을2초로 연장하거나 AGE를0으로 만들지 않음 |
| B16-VSS-03 | PLAYING 또는 callback void 반환 | 해당 Event의 실제 재생 완료 생성 없음; VSS App만 원 출력 사실 생산 |
| B16-VSS-04 | current mask0, 최근 fault3/occurrence5/Age65535 | 현재와 이력 분리. 이력 Age 포화는 새 고장 아님 |
| B16-VSS-05 | Status last_fault_quality=2 | 현재 Wire 이력형식0/3과 불일치 → malformed; 정상0으로 보정 금지 |
| B16-VSS-06 | 서비스 Age10/고장이력 Age1000, 미반영20ms | 30/1020, 평가번호·occurrence 불변. 경과 미상이면 OUT 불변 TIME_HOLD |
| B16-VSS-07 | VSS State Core 반영 뒤 Fault 또는 Observer 실패 | 부분 성공 보존·성공 Core 재실행 없음; 실제 반영단계 미확인 시 재호출 HOLD |
| B16-VSS-08 | INVALID 경고로 출력 중단 | 위험 CLEAR 생성 없음; 복구 후 새 유효 Warning으로 중재 |

유지 미결: CF02/E01~05 실제 Warning producer·시간·양단 지원, E-S01~05 Status 원기록/세대·시각·복사/terminal sink·설치 C2/부분 수용/Getter association. A1/A2 내부 선택은 완료되었지만 실제 Firmware/Codec/Board/음향 및 Binding 검증은 **NOT_EXECUTED**다.
