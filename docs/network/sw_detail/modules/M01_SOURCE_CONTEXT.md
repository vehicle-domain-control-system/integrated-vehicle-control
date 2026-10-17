# M01 원본·문맥 연결

원 수신과 C2 자료의 동일 관측 관계를 보존한다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 입력 | M04의 유효 원 관측과 M03 투영 결과 |
| 출력 | DOMAIN typed 인계의 원본 연결 근거 |
| 보유 상태와 수명 | 원 Net 자료·BOOT/TX/UPDATE 번호·수신 시각과 C2 반환의 연결 근거(ST01). 이 경로에서는 독립 기능 상태를 새로 만들지 않는다. |

F01의 후방 관측을 대표 경로로 구체화한다. 전체 문맥·Permission·Pi relay의 연결은 아래 정본을 따른다. 수신 수용과 typed 인계에 걸친 연결 책임이며 새 기능 상태 저장소를 추가하지 않는다.

## 정상 처리

원 자료와 메타를 함께 보존하고, 그 사본에서 C2를 만든다. DOMAIN 수용 결과는 같은 입력과 연결하며 값만 같은 Getter 결과를 근거로 확정하지 않는다.

## 실패와 복구

C2 거부·기동 변경·동일 관측 증거 부족 시 새 결합을 확정하지 않는다. 원 AGE나 원 번호를 새 수신시각으로 재발급하지 않는다.

원 관측·메타·인계 결과의 연결은 DOMAIN 수신 소유자가 함께 갱신한다. 다른 Task나 ISR가 연결의 일부만 바꾸지 않으며 실제 직렬화 방식은 노드 실행 바인딩에서 확인한다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetRearResult NetRear_Admit(const NetCisRear *message, const NetCanRxMeta *meta, uint32_t nowMs);` | 새 내부 함수. F01의 원 관측 부분: 동일 프레임의 값·메타를 연결하고 M04의 수용 결과와 함께 보존한다. | [NetRear_Admit](../functions/CIS_REAR_RX_ADMISSION.md#admit) |
| `NetRearResult NetRear_Deliver(uint32_t nowMs);` | 새 내부 함수. F01의 인계 연결 부분: 원 입력과 C2 호출 결과의 대응을 유지한다. Manager 상태는 소유하지 않는다. | [NetRear_Deliver](../functions/DOMAIN_CIS_REAR_INGRESS.md#adapter) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L01_NODE_CONTEXT.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F01/F02/F03

원 출처와 C2 관측을 같은 입력으로 연결하고 Permission 입력·기동/세대 바인딩을 확인한다. CIS Pi relay와 App permission은 동일입력의 별도 분기로 보존한다. 이 CAN 확장 문단은 Query와 Pi UART Codec를 포함하지 않는다. Query의 현재 UART-M 상세는 아래에서 연결한다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#registration) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `NetCanResult NetCan_ClassifyRx(const NetCanRxComparison *c, NetCanRxAction *action);` | 내부 제안; 원 입력/결과 단계와 TX의 수용 분류, 원장 직접변경 없음 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#rx-order) |
| `NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainNet_PublishPermission) |
| `void BcmNetPriv_OnDomainPermission( const NetDomainPermission *permission, const NetCanRxMeta *meta );` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-BcmNetPriv_OnDomainPermission) |
| `void CisApp_OnDomainPermission( const NetDomainPermission *message, const NetCanRxMeta *meta );` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-CisApp_OnDomainPermission) |
| `void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-WindowApp_OnDomainPermission) |
| `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-VssNetPriv_OnDomainPermission) |
| `DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainIf_TxCisPowerPermission) |
| `DomainIf_Status_t DomainIf_TxWindowOperationPermission( const DomainIf_WindowOperationPermission_t *permission);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainIf_TxWindowOperationPermission) |
| `DomainIf_Status_t DomainIf_TxWindowDomainAlive( const DomainIf_WindowDomainAlive_t *alive);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainIf_TxWindowDomainAlive) |
| `NetCanResult NetCan_RxDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile);` | Permission 원 입력·기동/품질·합성 근거 연결. 실제 함수의 소유/인계는 상세 참조 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#rx) |

## UART-M 연결 — F01/F02

원 NetDTO/NetCanRxMeta와 C2 실제 반영·Getter/TxPort 사본을 같은 입력으로 연결한다. 현재 DOWN_CONTEXT20B는 원 CAN/Pi Boot를 대체하지 않는다. association 부재는 해당 투영 미제공/Query 부분으로 보존하며 최신값 동등성으로 결합하지 않는다.

[원 CAN 흐름](../flows/UART_M_CAN_PROJECTION.md) · [Router](../functions/UART_M_ROUTER.md) · [M02](M02_QUERY_CONTROL.md) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)

<a id="uart-c"></a>
## UART-C 추가 연결

F02/F03은 DOMAIN의 후방 활성 Power 실제 재평가 약200ms와 원500ms/도달 시 비사용을 기존 원 association에 연결한다. 전송 반복은 원 시각·원 번호를 만들지 않으며 조건부 USE_LIMIT500과 기존1500 후보를 분리한다. F03은 Pi 두 값의 원 품질과 CIS 현재 제한 사건을 별도 보존한다. 원 INVALID 승격 없이 실제 단절·만료·기동 변화·기능 Fault의 해당 필드만 제한하며 [현재 제한 근거](../data/UART_C_DATA.md#type-NetVisionRestrictionEvidence)와 [Node 검사](../nodes/CIS_PI.md#origin-check)로 CAN publish를 연결한다. DOMAIN 원 permission은 CIS relay와 연결한다. 원 DOMAIN lineage 부재는 새 CIS Boot로 메우지 않는다. [함수/흐름](../flows/UART_C_CIS_TO_PI_PERMISSION.md) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)

## DOMAIN Adapter의 원 관측 연결 — F01/F02

검증된 원 DTO/meta, 그 입력에서 만든 C2 자료, DOMAIN 수용 결과와 이후 Snapshot의 대응을 [수신 Adapter](../functions/DOMAIN_RX_ADAPTER.md)와 [Query·원본 연결](../functions/DOMAIN_QUERY_PROVENANCE.md)에서 확인한다. 값 동등성이나 단순 Rx OK는 같은 관측 증거가 아니다. 원 생산 세대·Boot·AGE를 보존하고 실제 반영 근거가 없는 투영만 제한한다.

[Network→DOMAIN 흐름](../flows/NETWORK_TO_DOMAIN.md) · [DOMAIN 경계 개요](../07_DOMAIN_ADAPTER_SERVICE.md)

<a id="environment-boot-fence"></a>
## PG6-SW02 — 환경 원 Boot와 저장 경계

검증된 B 전이에서 M01/M04는 자기 소유 A 환경 대기·미실행 패킷/callback·미시작 환경 Query 응답의 재게시를 차단하고 실행 중 Network callback을 기존 owner 문맥으로 직렬화한다. M03/F08 Adapter가 고정 요청을 DOMAIN 소유 단일 동기 private 경계에 한 번 제출하고 현재 요청 내용·수용 BOOT·적용 Epoch·명확한 완료를 실제 응답과 대조하는 목표다. DOMAIN은 이미 인계된 구 작업의 후속 commit 봉쇄·VSM/Meta·순서 문맥 무효화·Query 구 GOOD 방지를 책임진다. 구 객체가 남아도 미래 commit이 확실히 봉쇄되고 나머지 완료 조건이 성립하면 가능하며 M01이 DOMAIN 내부 Queue를 drain하거나 VSM을 수정하지 않는다. B 원자료는 원 seq/수신시각/AGE로 보존하고 실제 Binding/완료 근거 전 정상 gate를 열지 않는다. [환경 저장 정본](../functions/can/CIS_ENVIRONMENT.md#environment-storage-order)을 따르며 Queue/반환 OK는 완료가 아니다.

원 key는 `(CIS Boot, 센서, 원 Sequence)`다. A900→B0을901로 재번호하지 않는다. 제안 `transitionEpoch`는 원 BOOT·센서seq·CAN TX·recordRevision/eventRevision과 다르며 폭/새 struct 필드/영속 저장을 추가하지 않는다. [첫1·완료 후 다음+1 운영안](../functions/can/CIS_ENVIRONMENT.md#sw-b-epoch-proposal)은 발송된 Network 제안·DOMAIN 회신 대기이며 현재 다른 Epoch는 EPOCH_ORDER_UNAVAILABLE다. 지연 A는 기존 종료 BOOT 기억/검증 범위에서 막되 재사용·기억 상실에 영구 차단을 약속하지 않는다. 일반 NO_DATA는 BOOT/seq reset/Epoch 사건이 아니다. Network 단독 재시작 자동복구 제외는 제안 범위이며 양단 초기 정합·실제 호출 근거 전 영향 환경 경로 HOLD다. 다른 노드 BOOT에 확대하지 않는다.

## PG6-SW02 — Permission 번호 공간

DOMAIN 원 성공 평가 Sequence·마지막 성공 시각은 실제 성공에서만 갱신한다. 성공17 뒤 평가 실패는 원17·원 시각을 유지한다. Network CAN 합성 UPDATE_SEQUENCE는 검증한 원 tuple 또는 실제 전달 의미·품질 제한 변경 후보이며 DOMAIN 성공 번호와 다르다. CAN TX_SEQUENCE·UART LINK_SEQUENCE는 전송 번호, CIS relay UPDATE_SEQUENCE는 원 평가·수용 세대·실제 제한의 별도 중계 번호다. 같은 원 성공 seq만으로 확인된 제한을 버리지 않는다. FAILED/NO_DATA 시 기존 ALLOW 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고다. UNKNOWN의 원인별 C2 Quality 투영은 DOMAIN Adapter 수용 대기의 POLICY_OPEN이며 Network 합성·CIS relay·Pi 연결은 BINDING_NOT_VERIFIED다. 합성 세부조건은 미결이다. [투영·번호 공간](../flows/CAN_BCM_AND_PERMISSION.md#permission-projection).
