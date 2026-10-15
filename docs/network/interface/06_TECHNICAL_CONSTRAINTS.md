# 기술 제약과 연동 조건

Interface v2 · r1 · [노드 간 공통 규칙](07_CROSS_NODE_INTERFACE_RULES.md#status)

각 절은 미확정 기능 정책, 표현 한계, 실제 연결 전 확인 조건을 정의한다. 조건이 입증되지 않은 경로는 해당 기능·투영 범위에서 제한한다. 권장 프로필은 구현 완료를 뜻하지 않는다. WINDOW의 설계는 유지하며 실제 구현은 보류한다.

<a id="section-active-bindings"></a>
<a id="hold-h01"></a>
<a id="r1-status-scope"></a>
## ESP32 선언 원본 불일치

[Gateway 선언](02_NODES/ESP32/11_GATEWAY_TYPES.md#type-gateway-updatebasis-t)의 `Gateway_UpdateBasis_t`에는 `update_sequence`가 있지만 저장소의 두 헤더 버전에는 없다. **H01 / HOLD_BINDING:** 실제 ESP32 공통 헤더와 생산·변환·소비 경로에서 이 필드의 계약을 확인하기 전에는 ABI·구현 연결을 보류한다. 문서의 선언과 저장소 헤더를 자동으로 동일시하지 않는다. 공통 Header 필드 채택 또는 생산원별 Adapter 독립 순번의 선택·실제 경로/버전과 M_CONTEXT38B 양단 변환, 최초·반복·재기동·wrap 근거를 확인할 때까지 HOLD를 유지한다.

## Wire version·양단 Codec

**CONTRACT_OPEN** · 담당: Network + DOMAIN/ESP32/VSS

현재 UART-M v2의 `M_CONTEXT`는 payload38/frame48, `VSS_WARNING_STATE`는 사용60B/전송64B/인코딩 DLC15다. 과거 payload31/used51 형식은 현재 layout으로 해석하지 않는다. CAN 원 상태 11종의 UART projection은 원 사용 길이에20B를 더한 현재 offset을 사용한다.

같은 TYPE에 두 layout을 혼용하지 않는다. 양단의 버전·길이·CRC가 일치하는지 확인하기 전에는 해당 형식의 통합을 차단한다.

[관련 계약](03_COMMUNICATION/20_UART_M.md) · [관련 계약](03_COMMUNICATION/10_CAN_FD.md)

## C2 enum→Wire 의미 보존

**WIRE_MAPPING_OPEN** · 담당: DOMAIN + 각 소비 기능 + Network

일부 ResultReason, Availability의 UNSUPPORTED, Warning/OccupantHazard의 UNCONFIRMED는 Wire에서 표현하지 못한다. origin 세부 정보를 축약하는 기준에도 근거가 필요하다.

표현하지 못하는 값은 인코딩을 거부하고 local 실패를 기록한다. OK·정상 CLEAR·임의의 기타 값으로 대체하지 않는다.

[관련 계약](04_COMMON/30_ENUM_WIRE_MAPPING.md)

## 명령 수용 기한

**CONFLICT / CONTRACT_OPEN** · 담당: DOMAIN + BCM + Network

현재 ACCEPT_LIMIT 후보 200ms와 구 SysRS의 1s 기준이 다르다. 생성 기준·송신 대기·중복 처리를 포함해 확인해야 한다.

200ms와 1s 어느 쪽도 확정된 값으로 보장하지 않는다. 조건 확정 전에는 기능 deadline을 충족한다고 주장하지 않는다.

[관련 계약](03_COMMUNICATION/10_CAN_FD.md) · [관련 계약](04_COMMON/10_DATA_AND_ID_RULES.md)

## WINDOW VENT 목표

**DEFERRED / FUNCTION_POLICY_OPEN** · 담당: WINDOW + DOMAIN

구 요구와 현재 후보의 목표 위치 70과 80이 충돌한다.

[구현 보류 — 설계 유지]. 목표 위치와 실제 위치의 표현·단위를 확정하기 전에는 구현을 보류한다.

[관련 계약](02_NODES/WINDOW/10_DATA.md)

## DOMAIN_PERMISSION 합성

**POLICY_OPEN / BINDING_OPEN.** 후방 Power 시간 규칙은 아래와 같으며, 공유 합성과 기능 정책·실제 연결은 미확정이다. 책임: DOMAIN 기능, Network 전달, CIS 소비. WINDOW 구현은 보류한다.

적용하는 규격은 후방 활성 Power의 약 200ms 실제 재평가·원 사용 기한 500ms, 새 평가/값 변경 전달, CAN50/150ms 및 UART-C500/1500ms 유지, 반복 송신의 AGE 초기화 금지다.

남은 범위는 `POWER_OPERATION_STATE` 생성원, Power/Operation/Alive의 필수 입력과 공통 `UPDATE_SEQUENCE/SOURCE_AGE` 합성이다. 기존 [CAN 합성안](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)은 후보로 유지한다. 독립 Power를 WINDOW 부재만으로 불허로 바꾸지 않고, 공통 AGE를 입증하지 못한 경우 허용을 꾸미지 않는다. 정상 ALLOW 사용에는 해당 합성 조건의 입증이 필요하다.

`P_PERMISSION.USE_LIMIT`의 1500ms 후보와 조건부 500ms 권장 프로필은 원 Power 사용 기한 500ms와 구분한다. Pi 통신 수신·만료 검사와 카메라 기능 적용은 별도 경계다. [상세](07_CROSS_NODE_INTERFACE_RULES.md#permission)

**원 평가와 UNKNOWN 처리:** 원 평가 Sequence는 성공 평가에서만 증가하고 원 TTL500ms·CAN50/150ms·UART-C500/1500ms는 유지한다. 평가 실패·NO_DATA에서 ALLOW 즉시 철회, 원 성공 Sequence 유지와 내부 stateRevision 증가는 DOMAIN 담당자 구현 완료 보고다. UNKNOWN의 false+비정상 Quality 투영과 Network 전달·합성은 수용/검증 대기이며 UNKNOWN+OK의 무손실 표현 한계는 남아 있다. 같은 Seq 품질 하향 누락·실패 AGE0·구 ALLOW 재등장을 막는 실제 TxPort→Queue→CAN 연결은 **BINDING_NOT_VERIFIED**다. POWER_OPERATION_STATE·필수 입력·공통 AGE/Quality/세대 및 O05 합성은 미결이다. [투영 조건](07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

## VSS R4/R6→DTO→C2

VSS 기능과 통신의 내부 연결은 하나의 구현 범위다. DOMAIN의 원 경고 의미와 VSS의 음향·상태 변환은 각각의 기술 경계에서 확인한다.

**BINDING_OPEN / BINDING_OPEN** · 담당: VSS + DOMAIN + Network

R4/R6 논리 참고에 Snapshot·Fault kind·보고 수명 정의가 있다. 실제 Firmware Header/구현 증거는 아니며, 변환 위치와 서비스 상태·fault provenance의 연결은 미확정이다.

VSS 상태 입력 API와 출력 DTO는 제안이다. 실제 R4/R6 원형이 확정됐거나 구현이 완료된 것으로 표기하지 않는다.

[관련 계약](02_NODES/VSS/10_DATA.md) · [관련 계약](02_NODES/VSS/20_API.md)

**PROPOSED:** [보호된 원 Snapshot 변환 권장안](02_NODES/VSS/10_DATA.md#r2-status-proposal)를 따른다. 기존 NetVssStatus를 통한 일회 변환·복사 후보이며 실제 원 평가/완전성/세대 연결은 미확정이다. VSS/DOMAIN은 current 완전성·최근 주요 기록/품질 축약을 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## WINDOW 결과·END·Fault lineage

**DEFERRED / CONTRACT_OPEN** · 담당: WINDOW + DOMAIN + Network

REQUEST_SEQUENCE↔C2 commandId의 대응, END와 AntiPinch의 전달 분기, Fault occurrence의 원천이 미정이다.

로컬 STOP/AntiPinch가 우선이다. END를 AntiPinch로 바꾸지 않는다. 연계가 확인되지 않은 결과로 차량 DONE을 합성하지 않는다.

[관련 계약](02_NODES/WINDOW/20_API.md)

## BLE profile·session·MTU

**CONTRACT_OPEN** · 담당: MOBILE + ESP32

UUID·Properties·ATT MTU·분할 방식은 ESP32/Android 양단의 P4-A 실제 채택 자료 확인 전까지 OPEN이다. 구현 진행과 P4-B 실기기 검증 완료를 동일시하지 않는다. [채택에 필요한 자료](03_COMMUNICATION/40_BLE.md#p4a-adoption)의 동일 버전 근거가 필요하며 App 활성·등록·Session 정책은 해당 기능 담당 범위다.

예제 UUID를 확정 BLE 계약으로 사용하지 않는다. 현재 인증된 session binding이 없으면 요청을 차단한다.

[관련 계약](03_COMMUNICATION/40_BLE.md) · [관련 계약](02_NODES/ESP32/20_API.md)

## 원 AGE·boot/generation

통신 권장 프로필 / 일부 C2 정책 미결 / 구현 확인 대기 · 담당: Network, 원 생산 기능, DOMAIN C2.

원 AGE는 같은 원 관측의 반복·재수신으로 낮추지 않고 홉과 대기를 각각 한 번 누적한다. CAN 메시지별 TX_SEQUENCE, UART 방향별 LINK_SEQUENCE, 원 관측 순번을 분리한다. 정상 반복 수신 감시와 같은 관측의 품질 저하 전달은 [공통 호출 규칙](04_COMMON/20_API_USAGE_AND_FAILURE.md)을 따른다.

CAN 5ms/UART 50ms 전달 여유, wrap·BOOT 실제 생성/기억 범위는 구현·실측 확인이 남아 있다. 원 생산 `sourceGeneration`과 통신 BOOT를 동일시하지 않는다. 후보 C2의 유효 관측 nonzero, 세대 미확인0 규칙과 `STALE + sourceGeneration==0` 정책 OPEN을 분리한다. 환경 C2에 필드를 자동 추가하는 요구가 아니다. [기동·품질](07_CROSS_NODE_INTERFACE_RULES.md#identity)

**원 식별과 기동 조건:** DOMAIN 원 성공 평가 Sequence, CAN 합성 UPDATE_SEQUENCE, 메시지별 TX_SEQUENCE, UART LINK_SEQUENCE와 CIS relay 번호를 구분한다. 실패 보고는 성공 원 시각·순번을 유지한 현재 품질 제한이며 원 AGE0이 아니다. 환경 A900→B0을 원값 그대로 수용하고 일반 NO_DATA로 재부팅 추론·순번 리셋을 하지 않는다. 종료 BOOT 차단은 기억된 범위에 한정하며 BOOT 재사용·Network 재시작의 기억 상실을 단순 BOOT 값으로 해결했다고 보장하지 않는다. [번호 공간](04_COMMON/10_DATA_AND_ID_RULES.md#delta-permission-sequences)

## SDK/Port·핀·전압

**BINDING_OPEN** · 담당: 보드·각 노드 + Network

144/344 SDK와 superloop/RTOS의 바인딩, 실제 전원·IO·핀·transceiver의 보드 근거를 확인해야 한다.

태스크·드라이버·핀·전압을 임의로 확정하지 않는다. 구현 직전에 보드 자료와 대조한다. SDK/Board 검증은 NOT_EXECUTED다.

[관련 계약](03_COMMUNICATION/10_CAN_FD.md) · [관련 계약](03_COMMUNICATION/20_UART_M.md)

## CAN bitrate·주기·CRC

**WIRE_PROPOSED / CONTRACT_OPEN** · 담당: Network + 전체 CAN 노드

단일 공유 CAN의 nominal 500kbps/data 2Mbps와 bit timing·ID·DLC·CRC·주기는 후보다.

실측 전에는 지연·응답의 상한을 보장하지 않는다. rate 변경은 필요한 구간만 검토한다.

[관련 계약](03_COMMUNICATION/10_CAN_FD.md)

## capacity·accepted 이후 실패

**BINDING_OPEN** · 담당: Network + DOMAIN/ESP32 + 각 기능

bounded 큐 용량, driver 실패·TTL 만료의 전달, Query burst와 BLE 병목 처리가 미정이다.

local OK와 차량 DONE을 구분한다. 인계 후 실패를 전달할 sink를 준비하고 identity를 보존한다. high-water 실측은 NOT_EXECUTED다.

[관련 계약](04_COMMON/20_API_USAGE_AND_FAILURE.md) · [관련 계약](03_COMMUNICATION/40_BLE.md)

**PROPOSED:** [수용 후 실패 인계](04_COMMON/20_API_USAGE_AND_FAILURE.md#section-network-health)를 따른다. 기존 identity를 보존하고 Query를 차량 Command/Result 실패로 위장하지 않는다. [Query 완료 count](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer)는 최종 TX 근거가 필요하며 실제 port/관측 연결을 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## Pi/CIS 품질 provenance

통신 권장 프로필 / 기능·C2 연결 검증 대기 · 담당: Network + CIS·Pi 기능 + DOMAIN 저장 연결.

Pi 원 판정의 값·필드별 품질·사유와 원 Pi BOOT/VISION_SEQUENCE/AGE를 보존한다. CAN에는 기존 최종 validity/reason 한 벌을 사용하고, 원 사유와 이후 CIS 제한 근거는 로컬 원 기록으로 구분한다. Pi 원 INVALID 사유를 영구적으로 최우선 고정하지 않는다.

같은 판정의 두 필드가 각각 사용 가능할 때만 Presence/Count 상호 모순을 검사한다. 정상 반복은 수신 감시에 반영하지만 새 관측이 아니다. 같은 관측의 새 품질 저하는 버리지 않고 기존 C2에 전달하며 단순 READY/지연 VALID로 복원하지 않는다.

[CIS 품질 권장 규칙](02_NODES/CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)을 따른다. 사유 변환·C2 commit/Getter 연결은 실제 연동 검증 전이다. 이 권장 프로필은 DOMAIN 기능 정책 전체의 확정을 뜻하지 않으며 새 Wire 품질 필드를 정의하지 않는다.

## Gateway local capacity 상태

**CONTRACT_OPEN / BINDING_OPEN** · 담당: ESP32 + Network

Gateway의 기존 반환 enum에는 capacity 전용 값이 없다. NOT_READY/INTERNAL_ERROR로 변환하는 책임이 미정이다.

기존 enum을 변경하지 않는다. local capacity 실패를 UART 실행 결과로 바꾸지 않는다.

[관련 계약](02_NODES/ESP32/20_API.md)

**PROPOSED:** [기존 반환형과 port 규칙](02_NODES/ESP32/20_API.md#ports)를 따른다. 일시 capacity 부족의 NOT_READY와 인자/내부 오류를 구분하는 후보다. caller/adapter가 현재 원 ID·문맥·수명을 재검증하는 재제출과 후발 실패 관측을 함께 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## Fault bit·helper projection

**CONTRACT_OPEN** · 담당: DOMAIN + BCM/CIS/VSS/WINDOW

상태 비트와 C2 fault helper 사이의 정확한 범주·bit·provenance 대응이 미정이다. CIS_FAULT/VSS_FAULT를 위한 추가 frame은 없다.

확정된 bit가 없는 상태를 특정 고장으로 추정하지 않는다. current 고장·최근 이력·link health를 구분한다.

[관련 계약](04_COMMON/30_ENUM_WIRE_MAPPING.md) · [관련 계약](02_NODES/DOMAIN/20_API.md)

**PROPOSED:** [BCM Health 경계](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status)를 따른다. 기존 확인된 bit만 변환하고 최초 관측/진단 생성 주체를 분리한다. [VSS current/history 변환](02_NODES/VSS/10_DATA.md#r2-status-proposal)을 함께 확인한다. 기능별 생성·해제 및 실제 진단 입력은 owner 확인 사항이다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## VSS Event 사용 한도·실제 시작

VSS 기능과 통신의 내부 연결은 하나의 구현 범위다. DOMAIN의 원 경고 의미와 VSS의 음향·상태 변환은 각각의 기술 경계에서 확인한다.

**CONFLICT / FUNCTION_POLICY_OPEN** · 담당: VSS + DOMAIN + Network

Network USE_LIMIT 500ms 후보와 구 SysRS의 원 발생 후 실제 시작 2초 미만 기준은 서로 다른 계층의 후보다. 시간 계약을 확정해야 한다.

500ms 또는 2초를 확정 deadline으로 적용하지 않는다. 양단의 수명과 VSS의 실제 시작 직전 검사 기준을 확정해야 한다.

[관련 계약](02_NODES/VSS/10_DATA.md) · [관련 계약](03_COMMUNICATION/10_CAN_FD.md)

## VSS 상태 품질·원본 세대

VSS 기능과 통신의 내부 연결은 하나의 구현 범위다. DOMAIN의 원 경고 의미와 VSS의 음향·상태 변환은 각각의 기술 경계에서 확인한다.

**BINDING_OPEN** · 담당: VSS + DOMAIN

C2 service.meta.quality·faultQuality·sourceGeneration과 축약된 VSS_STATUS의 대응이 미정이다.

서비스 품질과 고장 품질을 정상값으로 추정하지 않는다. 원본 세대·부분 진단은 필요한 내부 원장에 보존하고 조건 확정 후 투영한다.

[관련 계약](02_NODES/VSS/10_DATA.md)

**PROPOSED:** [VSS 원 보고별 제한](02_NODES/VSS/10_DATA.md#r2-status-proposal)를 따른다. 이력 raw0/3과 C2 전체 품질은 동치가 아니다. VSS/DOMAIN이 current 완전성·원 평가/generation·최근 기록 소비를 확인하기 전 미확인 fresh Status 투영은 제한한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## WINDOW 실제 보호 확인

**DEFERRED / FUNCTION_POLICY_OPEN** · 담당: WINDOW + DOMAIN

기존 C2 outputDisabled/movementStopped를 증명할 실제 확인 근거가 현재 State/Event Wire에 없다.

로컬 STOP과 끼임 보호가 우선이다. 상태 enum·수신 성공·timeout만으로 정지 또는 출력 차단 확인을 true로 만들지 않는다.

[관련 계약](02_NODES/WINDOW/10_DATA.md)

## 센서 범위·품질·Health 연결

통신 권장 프로필 / 기능 정책·구현 확인 대기 · 담당: Network + CIS/BCM 기능 + DOMAIN 소비.

Wire 표현 범위는 센서 정확도나 실측 유효 범위가 아니다. Network는 정상 수신 시각·CRC/overflow/링크 상실 같은 통신 원자료를 제공하고, 기능 모듈이 실제 영향·Fault를 판단한다. 원 센서 값과 품질은 독립 관리하며 통신 오류를 센서 자체 고장으로 자동 변환하지 않는다.

후방 거리 권장 프로필은 기존 Network 원 관측150ms 후보를 유지한다. SysRS 원 거리500ms 잠정, Permission500ms와 별개다. 실제 후방 소비 가용성은 시험 대기이고 변경 필요 시 해당 거리 조건만 확인한다. [시간표](07_CROSS_NODE_INTERFACE_RULES.md#timing)

CIS `INACTIVE=5`, 만료/불허 사유, 새 측정 후 재활성, Network Health→CIS 진단은 기존 API 연결로 검증한다. BCM의 실제 실행·confirmation 기준은 별도 미결을 유지한다. [CIS Data](02_NODES/CIS/10_DATA.md) · [BCM](02_NODES/BCM/20_API.md)

## 환경 센서 원본 최신성

온습도 원 사용 기한은 각 2500ms다. 조도 정책과 환경 C2 구현 연결은 미확정이다. 책임: CIS 센서, DOMAIN 기능, Network 전달.

온도·습도 각각의 원 사용 기한은 2500ms다. 사용 시점의 누적 AGE가 2500ms에 도달하면 해당 값을 사용하지 않는다. 새 CAN 프레임이나 다른 센서 갱신으로 오래된 관측의 AGE를 초기화하지 않는다.

실내 조도1000ms는 미확정 후보이고 CIS 로컬1000 코드가 DOMAIN의 수용을 대신하지 않는다. CIS 환경 CAN200ms 보고/600ms 수신 감시와 로컬 온습도5000ms 후보는 소비2500ms와 다른 기준이다. 온습도 원2500ms와 조도 후보를 구분한다.

환경 C2는 기존 구조를 우선 유지한다. 새 CIS BOOT는 DOMAIN 소유 private 전환 통지 후보로 구분하고, 기존 Rx NO_DATA와의 무효화 분담은 확인 대기다. 수신 중단의 STALE/NO_DATA 선택 및 Rx→VSM→Getter의 실제 저장 완료는 별도 연동 조건이다. 기존 구조로 해결할 수 없다는 근거가 나오면 최소 계약 변경을 검토한다. [상세](07_CROSS_NODE_INTERFACE_RULES.md#environment)

**환경 저장 순서:** 유효한 새 BOOT 판별 → 구 콜백·후속 쓰기 격리 → DOMAIN 소유 private 전환 통지 후보 → VSM/Meta·필요 순서 문맥 무효화 완료 확인 → 새 BOOT 원 Sequence/AGE 전달은 **PROPOSED / CROSS_OWNER_ACCEPTANCE_PENDING**이다. private 통지와 기존 Rx NO_DATA의 분담, 동기/비동기 완료 방식·실패 처리는 미확정이다. 일반 NO_DATA는 BOOT 신호가 아니며 일반 링크 단절의 STALE/NO_DATA 선택은 별도 미결이다. 무효화 실패·미확인에서는 새 정상 입력을 보류하지만 과거 OK가 지워졌다고 판단하지 않는다. 대기 AGE와 온습도2500ms 경계를 유지한다. [저장 순서·한계](07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)를 따른다.

## 부분 UserSettings 존재 정보

**WIRE_MAPPING_OPEN / AFFECTED_PROJECTION_HOLD** · 담당: DOMAIN + ESP32 + MOBILE

C2 NORMAL level/RGB의 독립 has flag를 M_USER_SETTINGS raw에서 완전히 표현하지 못함. 종합 품질로의 축약 동등성 미증명.

없는 설정을 정상값으로 합성하지 않는다. 부분 설정 투영은 소비 의미와 무손실 표현을 확정하기 전까지 차단한다.

[관련 계약](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md)

**PROPOSED:** [부분 Settings export 후보](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#r2-partial-export)를 따른다. 기존 sentinel만 사용하고 밝기/RGB presence 부재는 해당 composite 누락+partial Query로 제한한다. Settings/ESP32/MOBILE이 누락 표시를 확인해야 하며 확인된 0/OFF와 독립 정상 설정은 보존한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## DigitalKey 실제 unlock 전이

**WIRE_MAPPING_OPEN / AFFECTED_RESULT_HOLD** · 담당: DOMAIN + ESP32 + MOBILE

C2 actualUnlockTransition의 직접 raw bool 없음. ALREADY_AT_TARGET은 false이나 다른 조합의 동등성 미증명.

실제 전이를 결과 성공만으로 추정하지 않는다. 표현할 수 없는 조합의 export는 차단한다.

[관련 계약](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md)

**PROPOSED:** [DigitalResult 소비 후보](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#r2-partial-export)를 따른다. 원 DONE/reason을 보존하며 완료와 실제 unlock 전이 표시를 분리한다. MOBILE/기능 owner가 전이 확인의 별도 근거를 확인해야 하며 bool이 필요한 표시만 제한한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## Gateway 다형 relay subtype

**CONTRACT_OPEN / POLYMORPHIC_RELAY_HOLD** · 담당: ESP32 + MOBILE + Network

기존 broad category/ByteView relay에 typed subtype 전용 인자가 없음. schema registry의 logical tag·length·binding 미정.

정확한 subtype·payload 길이·소비 schema를 확정하기 전 해당 다형 relay를 차단한다. 기존 Gateway 원형은 변경하지 않는다.

[관련 계약](02_NODES/ESP32/25_PRIVATE_ROUTING.md)

**PROPOSED:** [23종 registry와 내부 convention](02_NODES/ESP32/25_PRIVATE_ROUTING.md#relay-schema)를 따른다. 기존 TYPE+P 논리 버퍼는 PROPOSED이며 Core/양측 adapter 채택이 필요하다. 실제 UART P/offset은 불변이고 RequestID/QueryID·현재 peer/session을 검증한다. ALIVE/QUERY_END는 private다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## C2 snapshot 원 CAN provenance

DOMAIN은 공개 C2 구현과 Manager를, Network는 수신 원 기록과 Adapter를 담당한다. 원 입력과 C2 Snapshot의 원자적 연결은 실제 검증이 필요하다. 같은 관측의 새 무효 품질을 버리지 않고 저장·Getter에 반영해야 하며, Pi 원 BOOT와 CIS 송신 BOOT를 바꾸어 붙이지 않는다.

**BINDING_OPEN / AFFECTED_PROJECTION_HOLD** · 담당: DOMAIN + Network

QueryGet에 raw boot/TX_SEQUENCE가 없음. receive provenance와 source generation/UPDATE_SEQUENCE의 정확한 연결 미정.

원 수신 기록과 snapshot의 대응이 확인되지 않으면 원 boot/sequence를 새로 합성하지 않는다. 해당 projection을 차단한다.

[관련 계약](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md)

**PROPOSED:** [원 수신과 C2 admitted Snapshot 연결](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-origin-join)를 따른다. 원 NetDTO/Meta를 실제 같은 원 입력의 commit에 연결한 경우만 투영한다. DOMAIN owner가 중간 갱신 배제/원자적 결합 지점을 확인해야 하며 미증명 projection만 제외한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

**환경 commit 경계:** DOMAIN Rx→VSM→Getter의 commit 위치, 구 콜백 이후 비동기 쓰기 배제와 기존 완료 관측 근거가 필요하다. Queue 접수/콜백 반환만으로 무효화 저장 완료를 판정하지 않는다. Core commit 뒤 optional Observer 실패는 직접 저장 실패와 구별하고 중복 commit하지 않는다. 저장 순서를 유지할 수 없거나 자체 세대 판별·다른 쓰기 경로 때문에 기존 C2로 입증할 수 없으면 해당 연결을 보류한다. 최소 Environment Generation/완료 경계의 계약 변경은 대안이며 현재 공개 필드/API에는 포함되지 않는다. [Rx 완료 조건](02_NODES/DOMAIN/20_API.md#delta-environment-completion)

## Query 실제 commit·종결

최근 요청 결과의 Device별8건 보관/최대5건 최신순 조회/건별 M_RESULT 후 M_QUERY_END 종료는 규칙을 적용한다. 아래 미결은 전송완료 count·부분실패·ESP32 수신 종결의 세부 구현이며, 이력8/5와 구분한다. 전체 상태 Query 응답 수를5건으로 제한하지 않는다.

**BINDING_OPEN / BINDING_HOLD** · 담당: DOMAIN + ESP32 + MOBILE + Network

M_QUERY_END의 actual frame count는 local queue admission 및 App receipt와 다름. 송신 complete commit·partial stream·footer ordering과 MOBILE 종료 schema 미정.

private Query 서비스는 유지한다. local OK로 actual count나 완료를 합성하지 않으며, 종결은 실제 commit 근거와 양단 schema를 확정해야 한다.

[관련 계약](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md) · [관련 계약](02_NODES/ESP32/25_PRIVATE_ROUTING.md)

**PROPOSED:** [Query 송신 count·종결](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer)를 따른다. 송신 terminal 근거와 footer 순서를 확인해야 한다. [ESP32 수신 종결](02_NODES/ESP32/25_PRIVATE_ROUTING.md#r2-query-consumer) 및 BLE incomplete 표시·timeout은 별도 양단 계약 확정 대상이다. 완료를 전체 최신성이나 차량 DONE으로 바꾸지 않는다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.


## 구 SysRS MOBILE 3초 정책의 조건부 검토

**PROPOSED / CROSS_NODE_AGREEMENT / FUNCTION_OWNER / IMPLEMENTATION_BINDING** · 담당: MOBILE + ESP32 + DOMAIN

구 SysRS의 MOBILE SENT 후 3초 잠정 정책은 현재 확정 deadline이 아니다. [Gateway 수명 후보](03_COMMUNICATION/20_UART_M.md#r2-request-deadline)는 수용 시점과 로컬 대기·원 문맥을 보존하고, 확인된 기한을 이미 넘은 요청만 차단하며 reconnect로 새 session/AGE0을 만들지 않는다. 원 SENT 시각·clock 불확실성·기한이 확정되지 않으면 end-to-end 3초를 보장할 수 없다. MOBILE/DOMAIN 기능 경계의 원 기준시각·수용 기한·재시도 정책은 미확정이다. Wire timestamp/새 raw 표현은 현재 계약에 포함되지 않으며 실제 바인딩도 검증 전이다.
