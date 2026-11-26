# VSS — 확정된 차량 의미의 음향 중재·서비스 보고

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

별도의 S32K344가 DOMAIN의 확정 Event·현재 Warning을 받아 음향을 중재한다. VSS는 후방 거리로 차량 위험을 다시 판정하지 않으며 자신의 실제 서비스·고장 상태를 DOMAIN에 보고한다. 현재 문서는 **DRAFT_SHAREABLE / IMPLEMENTATION_BINDING_HOLD**다.

[Data 전체 타입](10_DATA.md) · [송수신 API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공유 CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

| 방향 | 메시지·필요 데이터 | 노드 로컬 API |
|---|---|---|
| DOMAIN→VSS | `VSS_EVENT` · `NetVssEvent` | `void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta)` |
| DOMAIN→VSS | `VSS_WARNING_STATE` · `NetVssWarningState` | `void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta)` |
| DOMAIN→VSS | `DOMAIN_PERMISSION` · `NetDomainPermission` | private `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)` |
| VSS→DOMAIN | `VSS_STATUS` · `NetVssStatus` | `NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message)` |

App은 Event·Warning의 의미를 해석하고 VSS 서비스·고장 원 보고를 제공한다. Network는 검증·한정된 인계 저장소·링크 BOOT/TX_SEQUENCE·송신 대기 AGE·프레이밍을 관리한다. Network의 로컬 접수 성공, CAN ACK, VSS의 의미 데이터 수용, 실제 음향 출력·완료를 각각 구분한다.

API는 모두 **PROPOSED_DESIGN / BINDING_OPEN**이다. [경고 수신](20_API.md#api-vssapp_ondomainwarningstate)·[일회성 Event 수신](20_API.md#api-vssapp_ondomainevent)은 Application 입력 연결, [Status 송신](20_API.md#api-vssnet_publishstatus)은 Application의 실제 보고를 Network에 제출하는 경계다. [Permission 수신](20_API.md#api-vssnetpriv_ondomainpermission)은 Network-private 감시이며 App 필수 API가 아니다.

**A2 내부 선택 / BINDING_OPEN:** HEALTH의 완전 보고→FLOW→기존 INPUT 한 곳에서 보호된 Snapshot을 기존 `NetVssStatus` 11필드로 변환하고 Network가 전체 사본을 확보한다. [Status 생산·수용 계약](10_DATA.md#r2-status-proposal)의 원 평가·현재 Fault·최근 이력을 보존한다. 실제 C Header/Firmware, 원 세대·clock·terminal sink·DOMAIN 설치 profile 연결은 E-S01~05 확인 대기다. 누락 근거를 READY/mask0/정상 품질로 채우지 않는다. VSS App의 permission 소비 항목·연결도 미확인이다. [기술 제약 목록](../../06_TECHNICAL_CONSTRAINTS.md).

**A1 내부 선택 / 실제 시간·양단 지원 미검증:** Event의 최초 실제 출력 시작에는 `L=min(검증된 양의 USE_LIMIT, 500ms 설계 프로필, 2000ms 상한)`의 같은 원 기한을 적용하며 도달 시 만료다. WARNING60(used60/frame64/DLC15)을 단일 설계로 선택했고 WARNING51은 이력이다. 실제 producer·Codec·시각·음향 시작 증거는 별도로 필요하다. [입력·시간 계약](10_DATA.md#a1-binding).

경고 세 근거·후방 활성 근거는 독립이다. INVALID/STALE은 CLEAR가 아니며 출력 중단은 위험 해제가 아니다. 복구 전 일회 Pending 폐기·중단 일회 자동 재개 금지, 복구 후 새 유효 Warning 재중재를 유지한다. WINDOW 끼임 제품 연결은 **[구현 보류 — 설계 유지]**다.

실제 VSS R4/R6 binding, Firmware/Codec/Board/음향·CAN 시험은 **NOT_EXECUTED**다. [DOMAIN측 실제 선언과 Adapter](../DOMAIN/20_API.md) · [메시지 48종 색인](../../05_MESSAGE_INDEX.md).
