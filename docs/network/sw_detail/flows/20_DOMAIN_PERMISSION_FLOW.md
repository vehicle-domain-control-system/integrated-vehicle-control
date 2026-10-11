# DOMAIN_PERMISSION — 원 판단 입력·composer·네 수신 경계

[↑ Flow 색인](../04_FLOW_INDEX.md#flow-domain-permission) · [Network](../00_START_HERE.md) · [Function](../03_FUNCTION_INDEX.md) · [최소 CAN Port](../layers/06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / WIRE_PROPOSED / G2_INDEPENDENT=PENDING.** 기준 `ad934964618c7f11260ed58dcbd86f881cb99086`. `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`. 원 producer·coherence·원 기한과 **150 ms SOURCE_LIMIT_NOT_AGREED**를 보존한다. 아래 합성·소비는 조건부 의미 후보이며 ALLOW 정책 채택이 아니다.

<a id="flow-domain-permission-detail"></a>
## 입력과 실제 함수 범위

| 역할 | 이번 범위와 함수 | 동작 설명 / 의사코드 / 원 Source |
|---|---|---|
| 전원 원 판단 입력 | ACTIVE · FN-DOMAIN-DomainIf_TxCisPowerPermission | [설명](../layers/02_APPLICATION_PORT/DOMAIN/10_BEHAVIOR.md#fn-domain-domainif_txcispowerpermission) · [의사코드](../layers/02_APPLICATION_PORT/DOMAIN/20_PSEUDOCODE.md#fn-domain-domainif_txcispowerpermission) · [선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txcispowerpermission) |
| private composer 결과 제출 | ACTIVE · FN-DOMAIN-DomainNet_PublishPermission | [설명](../layers/01_NODE_COMMUNICATION/DOMAIN_PRIVATE/10_BEHAVIOR.md#fn-domain-domainnet_publishpermission) · [의사코드](../layers/01_NODE_COMMUNICATION/DOMAIN_PRIVATE/20_PSEUDOCODE.md#fn-domain-domainnet_publishpermission) · [기존 내부 경계](../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnet_publishpermission) |
| CIS 전원 소비 입력 | ACTIVE · FN-CIS-CisApp_OnDomainPermission | [설명](../layers/02_APPLICATION_PORT/CIS/10_BEHAVIOR.md#fn-cis-cisapp_ondomainpermission) · [의사코드](../layers/02_APPLICATION_PORT/CIS/20_PSEUDOCODE.md#fn-cis-cisapp_ondomainpermission) · [선언](../../interface/02_NODES/CIS/20_API.md#api-cisapp_ondomainpermission) |
| BCM 내부 감시 | ACTIVE · FN-BCM-BcmNetPriv_OnDomainPermission | [설명](../layers/02_APPLICATION_PORT/BCM/10_BEHAVIOR.md#fn-bcm-bcmnetpriv_ondomainpermission) · [의사코드](../layers/02_APPLICATION_PORT/BCM/20_PSEUDOCODE.md#fn-bcm-bcmnetpriv_ondomainpermission) · [선언](../../interface/02_NODES/BCM/20_API.md#api-bcmnetpriv_ondomainpermission) |
| VSS 내부 감시 | ACTIVE · FN-VSS-VssNetPriv_OnDomainPermission | [설명](../layers/02_APPLICATION_PORT/VSS/10_BEHAVIOR.md#fn-vss-vssnetpriv_ondomainpermission) · [의사코드](../layers/02_APPLICATION_PORT/VSS/20_PSEUDOCODE.md#fn-vss-vssnetpriv_ondomainpermission) · [선언](../../interface/02_NODES/VSS/20_API.md#api-vssnetpriv_ondomainpermission) |
| operation 입력 | DEFERRED_REFERENCE · FN-DOMAIN-DomainIf_TxWindowOperationPermission | [기존 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txwindowoperationpermission) · 함수 본문 상세화/실행 없음 |
| Domain alive 입력 | DEFERRED_REFERENCE · FN-DOMAIN-DomainIf_TxWindowDomainAlive | [기존 선언](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txwindowdomainalive) · 함수 본문 상세화/실행 없음 |
| 네 번째 수신 의무 | DEFERRED_REFERENCE · FN-WINDOW-WindowApp_OnDomainPermission | [기존 선언](../../interface/02_NODES/WINDOW/20_API.md#api-windowapp_ondomainpermission) · **[구현 보류 — 설계 유지]** |
| 동일 입력의 Pi relay | PEER_REFERENCE · FN-CIS-CisNetPriv_RelayPiPermission | [기존 경계](../../interface/02_NODES/CIS/20_API.md#api-cisnetpriv_relaypipermission) · UART-C/Pi 내부 구현은 U7/외부 owner |

한 `DOMAIN_PERMISSION`의 CIS/BCM/VSS/WINDOW **네 수신 의무**는 유지한다. WINDOW 미연결 상태를 실제 활성 consumer·timeout 고장으로 바꾸지 않는다. BCM/VSS는 Network-private 감독이며 새 App permission callback이나 복구 owner가 아니다.

<a id="composer-boundary"></a>
## 원 입력 → composer → 제출의 순서

1. 기능 owner가 [DomainIf_CisPowerPermission_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-cispowerpermission-t)의 bool·quality·SignalMeta를 제공한다. C2의 등록된 기존 typed TxPort가 원 필드와 수용시각을 독립 저장한다. C2 OK는 이 입력의 stable local admission이다. 아직 operation/alive를 합성했거나 Wire를 보냈다는 뜻이 아니다.
2. private composer는 기존 전원/operation/alive 입력을 각각 보관한 상태에서 하나의 로컬 `t_eval`에 원 tuple과 값/품질·메타를 고정해 읽는 후보다. 생성시각이 서로 같다고 가정하지 않는다. 읽는 중 원 tuple이 바뀌면 잠정 결과를 버리고 다음 owner 진행에서 다시 평가한다. 무한 retry loop·고정 반복 횟수·전역 관리자를 새로 만들지 않는다.
3. 합성은 [기존 정본 후보](../../interface/03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) 및 [공통 합성 미정](../../interface/04_COMMON/10_DATA_AND_ID_RULES.md#section-permission-synthesis)을 인용한다. quality 숫자의 max()가 품질 결합 규칙이 아니며, 각 입력의 OK/STALE/INVALID/NO_DATA 의미를 검사한다.
4. 전원은 원 quality와 meta.quality가 일치하고 원 사용 기한·producer가 확인됐을 때 기존 bool의 명시 표현을 사용할 수 있는 후보다. 충돌 시 power=255/INVALID, 미입력 시 255/NO_DATA라는 기존 제한 후보를 참고하며 없는 AGE를 만들어 채우지 않는다. **원 producer/기한/coherence 미합의 상태에서 새 power=1/OK를 생성하거나 사용 승인하지 않는다.**
5. operation/alive 필수 입력과 POWER_OPERATION_STATE 생성원은 기능 owner가 확인해야 한다. 원 Source의 미입력/충돌시 operation_allowed=0 + NO_DATA/INVALID와 state=255 후보는 확인된 정책 DENY가 아니다. 그 분기에는 `BLOCKED_BY: OPEN-PERM`을 유지한다. operation 부재만으로 독립 전원 판단을 새 불허 정책으로 바꾸지 않는다.
6. 확인 가능한 보존 원 근거의 AGE를 같은 `t_eval`까지 한 번씩 누적한 최댓값을 u16 포화하는 후보다. 근거 없는 입력에 0/65535를 넣어 최대값을 완성하지 않는다. 공통 AGE가 특정 필드의 원 기한을 입증하지 못하면 그 필드의 사용은 차단하며 AGE를 낮추지 않는다. 공통 UPDATE는 원 tuple 또는 실제 의미/품질 제한이 바뀌는 평가에서만 바뀐다.
7. 실제 원 근거와 상응하는 typed Snapshot만 내부 `DomainNet_PublishPermission` 경계로 제출한다. Application은 이 함수와 C2 전원 TX를 이중 호출하지 않는다. 아직 존재하지 않는 composer/락/공간·원 계보 binding은 `BOUNDARY_PENDING_U4D / BINDING_OPEN`이다.
8. 전체 bounded 복사·시각 기준점 commit의 NetSubmitResult는 기존 [C2 TxPort 명시 mapping](../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-return-status)과 구별해 관측한다. 이미 안정된 전원 입력의 수용과 이후 완성 Snapshot의 실패를 하나의 모호한 OK로 합치지 않는다.

현재의 합의 부족 때문에 3~7의 허용에 의존하는 분기는 실행 가능하거나 채택 완료로 판정하지 않는다. 정상 형식의 불허/열화 진단 record도 실제 확인 가능한 AGE·provenance가 없으면 NOT_READY로 제한한다. 로컬 기록을 채우기 위해 source_age_ms=0을 지어내지 않는다.

<a id="can-and-receivers"></a>
## CAN 전달과 수신 소비 경계

Codec/Transport는 [DOMAIN_PERMISSION Wire 후보](../../interface/03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)의 19 B 사용/20 B 전송/DLC 11, 명시 필드 배치·zero padding을 따른다는 요구만 인수한다. BOOT_ID/TX_SEQUENCE는 Network 소유, 합성 UPDATE_SEQUENCE/SOURCE_AGE는 원 입력 근거를 보존하는 별도 정보다. 새 응용 CRC/ACK/Heartbeat나 Wire/API/DTO를 추가하지 않는다.

Port accepted·controller terminal·각 ECU 실제 RX·typed callback·App 수용/차량 실행은 각각 독립이다. controller TX complete 또는 CAN ACK는 네 소비자 모두의 수신·사용 완료가 아니다. Driver busy/fail·bus-off·overflow·reset은 원 관측과 local transport 진단이며 차량 FAILED/DONE을 합성하지 않는다.

RX Dispatcher는 CAN ID/flag/length/DLC/padding/code·DOMAIN generation/순서를 검증한 owner 처리 문맥에서 전달한다. 구조 오류는 격리하고 정상 수신 시각을 갱신하지 않는다. 정상 형식의 원 열화는 전달할 수 있으며 새 TX와 원 평가 freshness를 구별한다.

| 수신 경계 | 기대 처리 | 하지 않는 판단 |
|---|---|---|
| CIS App | power 값/품질·공통 AGE와 DOMAIN meta를 함께 보존, 원 기한을 입증한 기존 정책 연결에서만 사용 후보 | operation 정보로 새 전원 ALLOW·Pi 허용 재발급 없음 |
| CIS private Pi relay 참고 | 같은 검증 원 입력에서 원 DOMAIN 근거/누적 AGE를 보존하는 제한 전달 경계 | UART-C frame/Session·Pi 내부 구현·1500 ms 원 기한 연장 없음 |
| BCM private supervision | 정상 DOMAIN peer 관측과 원 permission 최신성 분리, 기존 보호/진단 owner 연결 필요 | App permission consumer·보호 해제·차량 결과 합성 없음 |
| VSS private supervision | DOMAIN 원 record·감독 근거 보존, 음향 owner 경계 유지 | VssRecoveryPermission 변환·READY/CLEAR/DONE·AUDIO_OUTPUT_FAILURE 생성 없음 |
| WINDOW 보류 | 네 번째 수신 의무·원 Source 링크만 유지 | 실제 callback/동작/timeout 활성화 없음 |

<a id="negative-cases"></a>
## 문서상 오류·경계 시나리오

| 상황 | 검출/owner | 기대 처리·반환·commit |
|---|---|---|
| C2 초기화 전/NULL/불법 품질 code | 기존 C2 입력 | NOT_INITIALIZED/INVALID_ARGUMENT, 새 input commit 없음 |
| 원 quality와 meta.quality 충돌 | typed 입력 보관→composer | 충돌 근거 보존, 원 quality 중 임의 OK 선택 없음; 제한 후보는 미합의 유지 |
| 전원 근거 있음, operation/alive 근거 없음 | composer·기능 owner | 두 품질을 분리, 없는 operation 근거로 ALLOW 합성 없음. 독립 power를 임의 DENY하지 않음 |
| producer·원 기한·필수 입력/coherence 미확인 | composer/typed Snapshot 제출/기능 소비 | `BLOCKED_BY: OPEN-PERM`, 허용 경로와 근거 없는 Snapshot 제출 중지 |
| 공통 AGE 산출을 위한 실제 원 AGE 없음 | composer/Publish | 임의 0/65535 생성 없음, 준비된 snapshot인 것처럼 OK를 만들지 않음 |
| composer 읽기 중 tuple 변경 | private adapter | 잠정 결과 폐기, 다음 owner 진행에 재평가; 무한 spin 없음 |
| local 공간 없음/경로 미준비 | 기존 TxPort 또는 Publish | QUEUE_FULL/NO_CAPACITY·NOT_AVAILABLE/NOT_READY를 원 반환체계로 구별, partial admission 없음 |
| 로컬 OK 후 Port fail/unknown/bus-off | Transport/terminal·health | 과거 OK는 stable admission으로 유지, 실제 late 실패 근거 보존; 원 허용 policy를 바꾸지 않음 |
| RX length/flag/code·controller 오류 | 각 Network RX | 격리/진단, callback·정상 peer 관측 없음 |
| duplicate/역전/이전 DOMAIN Boot | RX generation/순서 owner | 과거 허용 새 기동 승계 없음, 원 identity·시간근거로 제한; 상세 wrap/admission U4D |
| 같은 원 UPDATE, 새 TX 수신 | 각 Network/기능 소비 | 링크 새 RX와 원 평가를 분리, 원 AGE를 낮추거나 사용 기한을 다시 시작하지 않음 |
| 오래된/포화 AGE·수신 중단 | 기존 aging/supervision 및 기능 owner | 실제 원 사용 한도 입증 실패 시 사용 제한, 정상/ALLOW/새 fault/복구 owner 합성 없음 |
| callback 저장 용량·consumer binding 미준비 | CIS App 또는 BCM/VSS private 감독 | 전체 저장 안 되면 commit 없음, void를 기능 성공으로 표시하지 않음 |
| reset 이후 늦은 terminal/RX | generation/correlation owner | 과거 token/Boot를 현재 상태에 연결하지 않음. 이전 ALLOW 복원·새 판단 생성 없음 |
| 보류 WINDOW 또는 미합의 VSS App ingress | 사용자/해당 기능 owner | 역할 참고만 유지, 새 실제 소비 경계 신설 없음 |

<a id="source-age"></a>
## 시간·소유권의 불변 조건

TX 인계시각과 RX received_at_ms는 각 노드의 단조시각이다. 원 AGE는 원 producer부터 증가하며 composer/encode/retry/receive 자체가 이를 새로 시작하지 않는다. 원 u32 AGE는 u16으로 포화하고 modulo로 fresh 상태를 만들지 않는다. 이미 포함한 대기·홉 여유를 재합산하지 않으며 노드 간 로컬 시각을 빼지 않는다.

원 tuple의 각 generation/sequence와 합성 UPDATE, frame TX 순번은 별도다. BOOT admission·update/tx wrap·동일 identity의 내용 충돌·late callback correlation은 필요한 관찰 사실로 명시했으며 실제 큐/캐시 알고리즘은 U4D다. TX OK 전에 caller와 무관한 복사/인계가 완료돼야 하고 RX borrowed 객체는 반환 뒤 참조하지 않는다.

<a id="blocked-by"></a>
## BLOCKED_BY / 후속 증거

`OPEN-PERM`(DOMAIN/CIS/보류 WINDOW 기능 owner)의 원 producer·필수 입력/coherence·150 ms 원 기한·state 생성원, `OPEN-PROVENANCE`(composer/Codec/C2 owner)의 원 tuple와 공통 AGE/UPDATE 연결, `OPEN-C2-SOURCE`의 target Header, `OPEN-WIRE`의 양단 채택을 유지한다. `OPEN-PORT-CAPABILITY / OPEN-SDK/HW/TIME`은 B-track/U10 실증으로 확인한다. Pi/ESP32는 `OPEN-PEER-OWNER`, WINDOW는 `OPEN-WINDOW-DEFERRED`다.

Gateway Types 헤더 별도 부재·Architecture README 링크 부재·과거 원본/HOST I2B 증거 한계도 이 U2로 해결되지 않는다. [기존 팀 OPEN](../../interface/06_OPEN_FOR_TEAM.md)은 read-only로 보존한다.
