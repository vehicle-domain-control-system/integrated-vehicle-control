# N04 VSS — 확정된 경고·발생과 실제 음향 보고의 경계

[노드 경계 전체](../08_NODE_BOUNDARIES.md) · [노드·링크 지도](../02_NODE_LINK_MAP.md) · [Layer·Module·Function 색인](../03_LAYER_MODULE_INDEX.md) · [Interface 개요](../../interface/02_NODES/VSS/00_OVERVIEW.md) · [Data](../../interface/02_NODES/VSS/10_DATA.md) · [API](../../interface/02_NODES/VSS/20_API.md)

DOMAIN이 확정한 차량 의미를 별도 S32K344의 VSS에 전달하고 VSS의 실제 서비스·고장 근거를 되돌리는 경계다. Network는 후방 거리로 위험을 재판정하지 않고 음원 선택·재생 시작·완료도 대신 판단하지 않는다. **DRAFT_SHAREABLE / PROPOSED_DESIGN / BINDING_OPEN**이며 Warning은 **CF02 HOLD**를 유지한다.

## 적용 구조·Link와 정확한 기존 선언

N04의 P01은 공유 CAN 물리 경로, R03은 DOMAIN↔VSS 논리 관계, I04는 VSS App↔Network typed 경계다. L02/M03/F10/ST04가 로컬 인계를, L03/M04/F11~13/ST05~07이 사본·시간·순서를, L04/M05/F14~15가 Codec을, L05/M07~09/F19~25/ST09~12가 저장소·Driver·관측을 맡는다. P02~04의 직접 VSS 송수신과 VSS 전용 원격 Query RPC는 **NOT_APPLICABLE**이다. MOBILE의 VSS 표시는 DOMAIN의 기존 projection을 따른다.

| 정확한 기존 원형 | 원 생산자 → 호출·소비 Owner | Wire·하위 상세 |
|---|---|---|
| `void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);` | DOMAIN 위험·활성 판단 → VSS Network → VSS INPUT/App | [VSS_WARNING_STATE](../functions/can/VSS_WARNING_STATE.md), CF02 used51/60 HOLD |
| `void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);` | DOMAIN 확정 발생 → VSS Network → VSS INPUT/App | [VSS_EVENT](../functions/can/VSS_EVENT.md) |
| `NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);` | VSS HEALTH 보고 → FLOW → 기존 INPUT 통신 연결 → VSS Network | [VSS_STATUS](../functions/can/VSS_STATUS.md); 변환 위치 미확정 |
| `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | DOMAIN 판단 → VSS Network 내부 감독 | [DOMAIN_PERMISSION](../functions/can/DOMAIN_PERMISSION.md), App 필수 API가 아님 |

네 원형은 Interface의 **EXISTING_DECLARATION**이며 모두 후보다. 실제 VSS R4/R6 함수·Header 설치의 증거로 사용하지 않는다. typed 구조체·전 필드·enum·Wire offset은 위 Data/메시지 정본을 재사용한다. Rx `message/meta`는 같은 검증 수신의 NULL 불가 IN 사본을 callback 동안 빌려 준다. 이후 저장은 소비자가 전체 필드·원 식별·시각을 복사한다. `void`에는 성공 반환값이 없으므로 실제 bounded 소비/복사 및 누락 관측 계약이 연결되기 전 정상 소비를 약속할 수 없다. ISR은 위 의미 API를 호출하지 않으며 해당 Owner Task/Superloop 직렬화가 선행 조건이다.

Status 입력도 NULL 불가 IN/borrowed다. `NET_SUBMIT_OK` 전에 Network 소유 전체 값과 원 근거·Queue 게시가 완료돼야 한다. INVALID_ARGUMENT/NOT_READY/NO_CAPACITY는 각각 형식/준비/공간 부족이며 새 자료 부분 수용은 없다. CAN 전송 성공·VSS 실제 출력·DOMAIN 반영은 반환으로 보장되지 않는다.

<a id="event-warning"></a>
## DOMAIN Event와 Warning의 서로 다른 사용 근거

| 입력 | Network가 보존하는 근거 | VSS App가 소유한 판단 |
|---|---|---|
| Event | 수용된 DOMAIN Boot + `event_kind/occurrence_id`, 원 `original_age_ms/use_limit_ms/value_quality/input_availability`, 선택 `related_command_id` | 같은 발생 중복 재생 방지, 실제 시작 시점의 원 한도·품질·가용성 확인, 음향 중재 |
| Warning | 끼임·탑승자·후방 각각 source Boot/sequence/Age/quality/reason/availability | 유효 현재 위험에 따른 음향. 거리값으로 위험 재판정하지 않음 |
| 후방 활성 | `rear_detection_activation`과 별도 DOMAIN 평가 `rear_activation_sequence`, `rear_activation_age_ms`, `rear_activation_quality`, `rear_activation_reason` | 유효 활성과 유효 후방 위험을 함께 소비. 활성과 위험 CLEAR는 별개 |
| Permission | 원 DOMAIN 판단·기동·수신 근거 | 기본 소비자는 Network 내부 감독. VssRecoveryPermission으로 자동 변환하지 않음 |

수신은 기존 Codec → [공통 수신 순서](../functions/CAN_TRANSFER_CONTROL.md#rx-order) → 해당 typed 인계다. 잘못된 길이/코드/padding/기동은 격리하며 정상 liveness나 의미 freshness를 갱신하지 않는다. 같은 occurrence에 높은 CAN TX가 와도 새 Event를 만들지 않는다. Event가 한동안 없다는 사실은 통신 고장이 아니다. INVALID/STALE/NO_DATA 같은 형식상 유효한 품질 저하는 해당 상태로 전달할 수 있으나 새 재생·위험 CLEAR를 만들어서는 안 된다.

CF02의 두 후보는 **둘 다 frame64/DLC15**이다. used51은 B51~63 padding, used60은 B51~59 확장 의미와 B60~63 padding이다. 길이·DLC·B51 marker 관측이나 profile enum 선택은 양단 채택 증거가 아니다. 합의 전 encode/decode/typed Warning 소비는 `CONTRACT_HOLD`이며 구조가 맞아 보인다는 이유로 PASS시키지 않는다. 향후 profile이 합의돼도 format0은 후방 활성 근거 부재이며 끼임·탑승자 등 다른 유효 근거를 일괄 지우는 이유가 아니다. WINDOW 끼임 제품 경로는 별도 실행 HOLD다.

`VSS_EVENT.USE_LIMIT=500ms`는 Network 후보이고 원 발생→실제 음향 시작 2초 미만은 구 SysRS 잠정 기준이다. 적용 위치와 합성·우선 규칙은 미합의다. 둘을 서로 바꾸거나 2초 안이라는 이유로 원 500ms 후보 만료 입력을 새로 유효하게 만들지 않는다. 원 한도와 정확한 경계 비교가 채택되지 않은 의존 경로는 HOLD다. 수신 callback 직전과 실제 VSS 출력 시작 사이의 대기도 원 Age에 포함돼야 하며 Network callback 성공으로 실제 시작 Age 준수를 증명하지 않는다.

입력 상실 뒤 일회 Pending·중단된 일회 음향은 자동 재개하지 않는다. 회복 후 새 유효 현재 Warning으로 재중재하며 출력 중단을 차량 위험 CLEAR로 보고하지 않는다. 외부 음향 정책과 실제 종료 결과는 VSS Owner 책임이다.

<a id="status-flow"></a>
## VSS Status 생성 → 보호된 인계 → DOMAIN 투영

1. VSS HEALTH가 실제 서비스·현재/최근 고장과 원 평가 근거를 생성한다. 기존 FLOW→INPUT 통신 연결에서 보호된 자료를 전달한다. Network에 HEALTH가 직접 호출하는 새 경계나 진단 관리자를 만들지 않는다.
2. 기존 Snapshot→`NetVssStatus` 변환은 [권장안](../../interface/02_NODES/VSS/10_DATA.md#r2-status-proposal)이다. 변환 Owner·한 번의 변환 위치·현재 목록 완전성·최근 주요 기록 선택·occurrence/시각·평가 번호를 실제로 확인해야 한다. kind 목록 순서나 현재 mask0만으로 이 근거를 복원할 수 없다.
3. 완전한 DTO가 확보된 경로만 `VssNet_PublishStatus`가 복사·접수한다. `vss_state`, `vss_availability`, `vss_accepting_events`는 서비스 보고이며 Event별 재생 시작/완료 응답이 아니다. `fault_mask/vss_fault_active`는 현재이고 `vss_last_fault/last_fault_occurrence/last_fault_age_ms/last_fault_quality`는 이력이다. 현재 고장이 해제되어도 최근 이력은 남을 수 있다.
4. M04는 송신 대기 경과를 현재 서비스 `source_age_ms`와 활성 최근이력 `last_fault_age_ms`에 각각 누적한다. `update_sequence`는 실제 새 평가에서만 변하며 CAN TX_SEQUENCE로 대체하지 않는다. 확정 불가한 원 시각은 AGE0으로 보충하지 않는다.
5. [기존 VSS_STATUS Codec](../functions/can/VSS_STATUS.md#codec)을 거쳐 불변 frame을 M07/M08에 전달한다. Driver 수명·재기동은 [Service 정본](../functions/NETWORK_SERVICE_LIFETIME.md)을 따른다. Driver accepted 또는 terminal TX는 상대의 Status 소비 성공을 뜻하지 않는다.
6. DOMAIN 수신은 기존 `DomainNetPriv_OnVssStatus`와 [Rx Adapter](../functions/DOMAIN_RX_ADAPTER.md)를 사용한다. `LAST_FAULT_QUALITY` raw0/3은 최근 기록의 존재/부재이며 C2 서비스 품질·전체 Fault 품질과 동등하지 않다. 필요한 원 품질·생산 Generation·같은 관측 연결이 미확인인 투영은 HOLD다. 값이 같다는 사실로 raw와 Getter snapshot을 결합하지 않는다.
7. `DomainIf_RxVssState`와 `DomainIf_RxVssFault` 두 소비의 실제 지원·부분 반영·재호출 정책이 준비되지 않으면 두 호출 전에 제한한다. 일부 반영 뒤 선택 Observer나 다른 소비만 실패하면 성공한 Core를 재실행하지 않는다. 원문 반환과 반영 단계를 보존하고 전체 성공으로 집계하지 않는다.

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

최근 기록 부재3이면 그 이력 Age를 새 발생의 나이로 계산하지 않고 저장값을 보존한다. 입력의 부재 표현은 Owner가 이미 확인해야 하며 임의 기본3을 만들지 않는다. 65535 포화는 하한으로 그대로 남고 이 함수는 두 saturation 결과를 fresh 판정에 쓰지 않는다. Source quality·전체 필드·원 품질 변환 검사는 기존 Publish/Codec 단계에서 별도로 수행한다. 반환 OK는 그 검사나 Queue 게시를 대신하지 않는다.

## 실패·재기동·HOLD와 반례

VSS reset 뒤 이전 DOMAIN Event를 새 occurrence로 재발급하거나 중단된 소리를 자동 재생하지 않는다. 새로운 source Boot의 수용과 기존 occurrence 보관 수명은 Owner 정책·실제 reset 근거가 필요하다. CAN 복구는 서비스 READY가 아니다. Status 송신 실패는 VSS의 기존 FLOW/INPUT 관측에 남길 경계가 미결이며 DOMAIN의 C2 실패 API를 VSS에 복제하지 않는다. 고장 이력·counter를 정상 프레임 한 건으로 지우지 않는다.

| ID | 입력·사건 | 정적 기대·Owner |
|---|---|---|
| B16-VSS-01 / 필수5 | Warning frame64/DLC15, used51/60 합의 없음 | CF02 CONTRACT_HOLD. 길이/marker 자동 추정 없음 |
| B16-VSS-02 | Event 원 Age가 후보500ms를 넘지만 2초 미만 | 시간 계약 충돌 유지, 숫자 교체로 재생 허용하지 않음; DOMAIN/VSS/Network 합의 필요 |
| B16-VSS-03 | PLAYING 또는 callback void 반환 | 해당 Event의 실제 재생 완료 생성 없음; VSS App만 원 출력 사실 생산 |
| B16-VSS-04 | current mask0, 최근 fault3/occurrence5/Age65535 | 현재와 이력 분리. 이력 Age 포화는 새 고장 아님 |
| B16-VSS-05 | Status last_fault_quality=2 | 현재 Wire 이력형식0/3과 불일치 → malformed; 정상0으로 보정 금지 |
| B16-VSS-06 | 서비스 Age10/고장이력 Age1000, 미반영20ms | 30/1020, 평가번호·occurrence 불변. 경과 미상이면 OUT 불변 TIME_HOLD |
| B16-VSS-07 | VSS State Core 반영 뒤 Fault 또는 Observer 실패 | 부분 성공 보존·성공 Core 재실행 없음; 실제 반영단계 미확인 시 재호출 HOLD |
| B16-VSS-08 | INVALID 경고로 출력 중단 | 위험 CLEAR 생성 없음; 복구 후 새 유효 Warning으로 중재 |

유지 미결: CF02, IF-O06/IF-O15/IF-O17의 Status owner/품질, IF-O09/IF-O24의 세대·같은 관측, D-05/G06의 실행 바인딩과 시간 충돌. 협의 내용은 기존 Owner 미결 및 S6 이후 큐에 기록만 한다. 실제 Firmware/Codec/Board/음향 검증은 **NOT_EXECUTED**다.
