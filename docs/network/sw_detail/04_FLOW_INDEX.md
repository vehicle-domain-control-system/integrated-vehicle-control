# Flow 출처 색인

[↑ Network 시작](00_START_HERE.md) · [Layer](01_LAYER_MAP.md) · [Node](02_NODE_MAP.md) · [Function](03_FUNCTION_INDEX.md)

두 Flow의 현재 지위는 **`U2_DESIGN_CANDIDATE / G2_INDEPENDENT_PENDING`**다. 화살표는 기존 문서의 논리 책임·출처를 찾는 순서이며 Codec·Port·변환 알고리즘 또는 실제 ECU 교환 성공을 뜻하지 않는다. 기존 Source 내용은 보존하며 G1 인수 기준은 `ad934964618c7f11260ed58dcbd86f881cb99086`다.

<a id="flow-cis-rear"></a>
## CIS_REAR — CIS 관측에서 DOMAIN 소비 경계까지

[U2 양단 Flow 상세](flows/10_CIS_REAR_FLOW.md) — 정상·열화·만료·재시작·Port 오류의 문서상 기대동작.

**CIS 센서 Owner → `CisNet_PublishRear` → 공유 CAN `CIS_REAR` → `DomainIf_RxCisRear` → DOMAIN 기능 Owner.**

| 찾을 지점 | 원 Source / 새 탐색 입구 |
|---|---|
| 원 typed 관측·제출 | [NetCisRear](../interface/02_NODES/CIS/10_DATA.md#type-netcisrear), [CisNet_PublishRear](../interface/02_NODES/CIS/20_API.md#api-cisnet_publishrear), [CIS Module](layers/02_APPLICATION_PORT/CIS/00_MODULE_MAP.md#functions) |
| CAN 계약 후보 | [CIS_REAR](../interface/03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) |
| DOMAIN typed 인계 | [DomainIf_RxCisRear](../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisrear), [DOMAIN Module](layers/02_APPLICATION_PORT/DOMAIN/00_MODULE_MAP.md#functions) |
| 기존 내부 투영·provenance | [수신 투영](../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#rx-projection), [원 CAN와 C2 Snapshot 연결](../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-origin-join) |

`OPEN-UNIT`, `OPEN-ENUM`, `OPEN-WIRE`, `OPEN-C2-SOURCE`와 원 provenance 합의(O24)를 유지한다. 과거의 CIS mm/raw↔DOMAIN 정수 cm 이슈는 해결하지 않는다. 현재 Source의 C2 필드는 `rearDistanceMm`이며 0.1 cm raw와 동일한 수치다. 실제 Header 부재와 구 표현의 채택·enum 변환은 계속 OPEN이고, U2가 `/10` 절삭 변환을 도입하지 않는다. 원 BOOT/UPDATE/AGE·품질과 링크 TX_SEQUENCE는 다른 근거다.

<a id="flow-domain-permission"></a>
## DOMAIN_PERMISSION — 원 허용 근거와 네 수신 경계

[U2 양단 Flow 상세](flows/20_DOMAIN_PERMISSION_FLOW.md) — composer·CIS/BCM/VSS 및 WINDOW DEFERRED 수신 의무.

**DOMAIN 기능 Owner의 기존 허용·Alive 근거 → 기존 Network 내부 제출 후보 → 공유 CAN `DOMAIN_PERMISSION` → CIS/BCM/VSS 및 보류 WINDOW 경계.**

| 찾을 지점 | 원 Source / 새 탐색 입구 |
|---|---|
| 기존 C2 입력 출처 | [DomainIf_TxCisPowerPermission](../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txcispowerpermission), [DomainIf_TxWindowOperationPermission](../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txwindowoperationpermission), [DomainIf_TxWindowDomainAlive](../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txwindowdomainalive) |
| 원 Source에 이미 있는 내부 후보 | [DomainNet_PublishPermission](../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnet_publishpermission), [DOMAIN private Module](layers/01_NODE_COMMUNICATION/DOMAIN_PRIVATE/00_MODULE_MAP.md#functions) |
| typed 의미·합성 미정 | [NetDomainPermission](../interface/04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission), [원 합성 출처](../interface/04_COMMON/10_DATA_AND_ID_RULES.md#section-permission-synthesis), [CAN DOMAIN_PERMISSION](../interface/03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) |
| CIS 기능 입력 | [CisApp_OnDomainPermission](../interface/02_NODES/CIS/20_API.md#api-cisapp_ondomainpermission) |
| BCM/VSS Network-private 감시 | [BcmNetPriv_OnDomainPermission](../interface/02_NODES/BCM/20_API.md#api-bcmnetpriv_ondomainpermission), [VssNetPriv_OnDomainPermission](../interface/02_NODES/VSS/20_API.md#api-vssnetpriv_ondomainpermission) |
| WINDOW 보류 입력 | [WindowApp_OnDomainPermission](../interface/02_NODES/WINDOW/20_API.md#api-windowapp_ondomainpermission) — **DEFERRED** |

`DomainNet_PublishPermission`은 기존 문서에 있던 `PROPOSED_DESIGN / BINDING_OPEN` 내부 경계다. 이번에 새 공개 API를 만들거나 Application에 C2 TX와 이 함수를 중복 호출하도록 요구한 것이 아니다.

`OPEN-PERM`의 실제 Producer·필수 입력·품질/coherence 및 원본 **150 ms `SOURCE_LIMIT_NOT_AGREED`**를 유지한다. `ALLOW`를 합성·승인하거나 전달/반복으로 SOURCE_AGE를 다시 시작하지 않는다. BCM/VSS private 감시와 WINDOW 보류 상태를 기능 사용 허용으로 확대하지 않는다.

함수별 `10_BEHAVIOR.md`·`20_PSEUDOCODE.md`는 [U2 ACTIVE 7개](03_FUNCTION_INDEX.md#u2-active-detail)만 작성했다. [최소 CAN LinkPort 의미](layers/06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)와 [팀 미합의](../interface/06_OPEN_FOR_TEAM.md)를 함께 읽는다. 모든 상세는 G2 독립 검토 전 후보이며 실제 통신 실행 결과가 아니다.


## U4D CAN common detailed design navigation

2026-10-12 사용자 직접지시에 따른 문서 상세설계 후보. G3 독립/로컬인수 PASS 유지; 기준 `9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4`. U4D_PROPOSED_PRIVATE / DESIGN_ONLY, G4D 독립검수 PENDING, U4C NOT_STARTED, target PENDING_USER_PUSH.

[CAN 16개 private 후보 함수](05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN TX/RX Flow와 22개 후속시험 설계](flows/30_CAN_COMMON_TX_RX_FLOW.md). 기존본문·144FN·U2계약은보존한다.
