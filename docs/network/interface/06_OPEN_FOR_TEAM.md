# 팀 결정 및 실제 바인딩 미정

현재 결정 담당과 구현 영향을 정리했다. `TEAM_DECISION_REQUIRED` 항목은 양단·기능 담당의 합의 전 호환 구현에 적용하지 않는다. SDK/Firmware/실 Codec/Board/무선 시험은 **NOT_EXECUTED**다.

<a id="section-active-bindings"></a>
## 활성 경로의 설계 규칙과 미합의 범위

상세 처리 규칙은 아래 정본에서 읽고, 각 O-ID 절에서는 합의 주체와 제한을 확인한다. 구체 권장안은 **PROPOSED**이며 **AGREED/IMPLEMENTED/TESTED**와 구분한다. 선언이 존재한다는 사실은 실제 구현·동작의 증거가 아니다.

| 확인 경로 | O-ID | 상세 처리 정본 |
|---|---|---|
| Gateway subtype·문맥·local 반환 | O08/O14/O23 | [TYPE registry](02_NODES/ESP32/25_PRIVATE_ROUTING.md#relay-schema) · [반환/port](02_NODES/ESP32/20_API.md#ports) |
| Query 송신 완료·수신·BLE 종결 | O12/O25 | [DOMAIN 생산](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer) · [ESP32 소비](02_NODES/ESP32/25_PRIVATE_ROUTING.md#r2-query-consumer) |
| 원 CAN과 C2 Snapshot 연결·부분 표시 | O21/O22/O24 | [원 입력 연결](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-origin-join) · [부분 export](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#r2-partial-export) |
| Permission 품질·공유 AGE·Pi 원 기한 | O05 | [CAN 합성](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [Pi Data](02_NODES/PI/10_DATA.md) |
| Pi/CIS 품질·센서별 실패·BCM Health | O13/O15/O19 | [CIS Data](02_NODES/CIS/10_DATA.md) · [BCM Health](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) |
| VSS current/history·원 보고·세대 | O06/O17 | [VSS 변환 후보](02_NODES/VSS/10_DATA.md#r2-status-proposal) |
| MOBILE 원 시각과 조건부 정책 | N01 | [Gateway 기한 검토](03_COMMUNICATION/20_UART_M.md#r2-request-deadline) |

## O01 · Wire version·양단 Codec

**TEAM_DECISION_REQUIRED** · 담당: Network + DOMAIN/ESP32/VSS

현재 UART-M v2의 `M_CONTEXT`는 payload38/frame48, `VSS_WARNING_STATE`는 used60/DLC64다. 과거 payload31/used51 형식은 현재 layout으로 해석하지 않는다. CAN 원 상태 11종의 UART projection은 원 사용 길이에20B를 더한 현재 offset을 사용한다.

같은 TYPE에 두 layout을 혼용하지 않는다. 양단의 버전·길이·CRC가 일치하는지 확인하기 전에는 해당 형식의 통합을 차단한다.

[관련 계약](03_COMMUNICATION/20_UART_M.md) · [관련 계약](03_COMMUNICATION/10_CAN_FD.md)

## O02 · C2 enum→Wire 의미 보존

**TEAM_DECISION_REQUIRED** · 담당: DOMAIN + 각 소비 기능 + Network

일부 ResultReason, Availability의 UNSUPPORTED, Warning/OccupantHazard의 UNCONFIRMED는 Wire에서 표현하지 못한다. origin 세부 정보를 축약하는 기준에도 근거가 필요하다.

표현하지 못하는 값은 인코딩을 거부하고 local 실패를 기록한다. OK·정상 CLEAR·임의의 기타 값으로 대체하지 않는다.

[관련 계약](04_COMMON/30_ENUM_WIRE_MAPPING.md)

## O03 · 명령 수용 기한

**CONFLICT / TEAM_DECISION_REQUIRED** · 담당: DOMAIN + BCM + Network

현재 ACCEPT_LIMIT 후보 200ms와 구 SysRS의 1s 기준이 다르다. 생성 기준·송신 대기·중복 처리를 포함해 확인해야 한다.

200ms와 1s 어느 쪽도 승인 값으로 보장하지 않는다. 합의 전에는 기능 deadline을 충족한다고 주장하지 않는다.

[관련 계약](03_COMMUNICATION/10_CAN_FD.md) · [관련 계약](04_COMMON/10_DATA_AND_ID_RULES.md)

## O04 · WINDOW VENT 목표

**DEFERRED / TEAM_DECISION_REQUIRED** · 담당: WINDOW + DOMAIN

구 요구와 현재 후보의 목표 위치 70과 80이 충돌한다.

[구현 보류 — 설계 유지]. 목표 위치와 실제 위치의 표현·단위를 합의한 후 구현을 재개한다.

[관련 계약](02_NODES/WINDOW/10_DATA.md)

## O05 · DOMAIN_PERMISSION 합성

**TEAM_DECISION_REQUIRED** · 담당: DOMAIN + CIS/WINDOW + Network

독립된 C2 Meta 3종을 단일 UPDATE_SEQUENCE/AGE로 합성하는 기준과 power operation의 생성원이 미정이다. 입력별 품질과 갱신 시점의 차이를 확인해야 한다.

입력별 원 품질을 보존하고 누적 AGE의 최댓값을 제안한다. 필요한 정보가 미확인 상태이면 허용을 생성하지 않는다.

[관련 계약](04_COMMON/10_DATA_AND_ID_RULES.md)

**PROPOSED:** [기존 필드 합성 권장안](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)를 따른다. 같은 평가시점의 원 입력·개별 품질·보수 AGE를 사용한다. 필수 입력/원 기한/state 생성원은 기능 owner, 공유 AGE 소비는 DOMAIN/CIS 확인이 필요하다. [Pi 소비 제한](02_NODES/PI/10_DATA.md)도 함께 적용한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O06 · VSS R4/R6→DTO→C2

**TEAM_DECISION_REQUIRED / BINDING_OPEN** · 담당: VSS + DOMAIN + Network

R4/R6 논리 참고에 Snapshot·Fault kind·보고 수명 정의가 있다. 실제 Firmware Header/구현 증거는 아니며, 변환 위치와 서비스 상태·fault provenance의 연결은 미합의다.

VSS 상태 입력 API와 출력 DTO는 제안이다. 실제 R4/R6 원형이 확정됐거나 구현이 완료된 것으로 표기하지 않는다.

[관련 계약](02_NODES/VSS/10_DATA.md) · [관련 계약](02_NODES/VSS/20_API.md)

**PROPOSED:** [보호된 원 Snapshot 변환 권장안](02_NODES/VSS/10_DATA.md#r2-status-proposal)를 따른다. 기존 NetVssStatus를 통한 일회 변환·복사 후보이며 실제 원 평가/완전성/세대 연결은 미합의다. VSS/DOMAIN은 current 완전성·최근 주요 기록/품질 축약을 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O07 · WINDOW 결과·END·Fault lineage

**DEFERRED / TEAM_DECISION_REQUIRED** · 담당: WINDOW + DOMAIN + Network

REQUEST_SEQUENCE↔C2 commandId의 대응, END와 AntiPinch의 전달 분기, Fault occurrence의 원천이 미정이다.

로컬 STOP/AntiPinch가 우선이다. END를 AntiPinch로 바꾸지 않는다. 연계가 확인되지 않은 결과로 차량 DONE을 합성하지 않는다.

[관련 계약](02_NODES/WINDOW/20_API.md)

## O08 · BLE profile·session·MTU

**TEAM_DECISION_REQUIRED** · 담당: MOBILE + ESP32

UUID·ATT MTU·분할 방식, App 활성 정보의 신뢰 기준, 등록·현재 session 발급·재접속 정책이 미정이다.

기존 예제 UUID를 승인 계약으로 복구하지 않는다. 현재 인증된 session binding이 없으면 요청을 차단한다.

[관련 계약](03_COMMUNICATION/40_BLE.md) · [관련 계약](02_NODES/ESP32/20_API.md)

## O09 · 원 AGE·boot/generation

**PROPOSED / TEAM_DECISION_REQUIRED** · 담당: 전체 기능 + Network

단조 로컬 시각·u16 포화·각 홉의 여유·재기동·rollover를 처리하는 기준이 미정이다. CAN 5ms와 UART 50ms는 후보 값이다.

원 AGE를 0으로 재생성하지 않는다. 홉 여유와 대기는 각각 1회만 반영한다. 확인되지 않은 freshness를 유효한 것으로 처리하지 않는다.

[관련 계약](04_COMMON/10_DATA_AND_ID_RULES.md)

## O10 · SDK/Port·핀·전압

**BINDING_OPEN** · 담당: 보드·각 노드 + Network

144/344 SDK와 superloop/RTOS의 바인딩, 실제 전원·IO·핀·transceiver의 보드 근거를 확인해야 한다.

태스크·드라이버·핀·전압을 임의로 확정하지 않는다. 구현 직전에 보드 자료와 대조한다. SDK/Board 검증은 NOT_EXECUTED다.

[관련 계약](03_COMMUNICATION/10_CAN_FD.md) · [관련 계약](03_COMMUNICATION/20_UART_M.md)

## O11 · CAN bitrate·주기·CRC

**WIRE_PROPOSED / TEAM_DECISION_REQUIRED** · 담당: Network + 전체 CAN 노드

단일 공유 CAN의 nominal 500kbps/data 2Mbps와 bit timing·ID·DLC·CRC·주기는 후보다.

실측 전에는 지연·응답의 상한을 보장하지 않는다. rate 변경은 필요한 구간만 검토한다.

[관련 계약](03_COMMUNICATION/10_CAN_FD.md)

## O12 · capacity·accepted 이후 실패

**BINDING_OPEN** · 담당: Network + DOMAIN/ESP32 + 각 기능

bounded 큐 용량, driver 실패·TTL 만료의 전달, Query burst와 BLE 병목 처리가 미정이다.

local OK와 차량 DONE을 구분한다. 인계 후 실패를 전달할 sink를 준비하고 identity를 보존한다. high-water 실측은 NOT_EXECUTED다.

[관련 계약](04_COMMON/20_API_USAGE_AND_FAILURE.md) · [관련 계약](03_COMMUNICATION/40_BLE.md)

**PROPOSED:** [수용 후 실패 인계](04_COMMON/20_API_USAGE_AND_FAILURE.md#section-network-health)를 따른다. 기존 identity를 보존하고 Query를 차량 Command/Result 실패로 위장하지 않는다. [Query 완료 count](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer)는 최종 TX 근거가 필요하며 실제 port/관측 연결을 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O13 · Pi/CIS 품질 provenance

**TEAM_DECISION_REQUIRED** · 담당: PI + CIS + DOMAIN

Pi의 원 quality·validity·reason과 CIS 재평가를 CIS_OCCUPANT의 단일 quality에서 어떻게 구분할지 미정이다.

Pi boot·visionseq·원 AGE를 유지하고 원값을 로컬에 보존한다. CIS 재평가를 이유로 Pi가 원 생산자라는 사실을 바꾸지 않는다. 전달 품질의 합성 기준은 양단이 합의한다.

[관련 계약](02_NODES/CIS/10_DATA.md) · [관련 계약](02_NODES/PI/10_DATA.md)

**PROPOSED:** [Pi 원 품질과 CIS 제한 처리](02_NODES/CIS/10_DATA.md)를 따른다. 원 품질 승격 없이 Presence/Count를 독립 소비한다. Pi/CIS/DOMAIN은 한 최종 reason의 대표 선택·추가 로컬 근거 보호를 확인해야 하며 cache/codec 연결은 미검증이다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O14 · Gateway local capacity 상태

**TEAM_DECISION_REQUIRED / BINDING_OPEN** · 담당: ESP32 + Network

Gateway의 기존 반환 enum에는 capacity 전용 값이 없다. NOT_READY/INTERNAL_ERROR로 변환하는 책임이 미정이다.

기존 enum을 변경하지 않는다. local capacity 실패를 UART 실행 결과로 바꾸지 않는다.

[관련 계약](02_NODES/ESP32/20_API.md)

**PROPOSED:** [기존 반환형과 port 규칙](02_NODES/ESP32/20_API.md#ports)를 따른다. 일시 capacity 부족의 NOT_READY와 인자/내부 오류를 구분하는 후보다. caller/adapter가 현재 원 ID·문맥·수명을 재검증하는 재제출과 후발 실패 관측을 함께 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O15 · Fault bit·helper projection

**TEAM_DECISION_REQUIRED** · 담당: DOMAIN + BCM/CIS/VSS/WINDOW

상태 비트와 C2 fault helper 사이의 정확한 범주·bit·provenance 대응이 미정이다. CIS_FAULT/VSS_FAULT를 위한 추가 frame은 없다.

확정된 bit가 없는 상태를 특정 고장으로 추정하지 않는다. current 고장·최근 이력·link health를 구분한다.

[관련 계약](04_COMMON/30_ENUM_WIRE_MAPPING.md) · [관련 계약](02_NODES/DOMAIN/20_API.md)

**PROPOSED:** [BCM Health 경계](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status)를 따른다. 기존 확인된 bit만 변환하고 최초 관측/진단 생성 주체를 분리한다. [VSS current/history 변환](02_NODES/VSS/10_DATA.md#r2-status-proposal)을 함께 확인한다. 기능별 생성·해제 및 실제 진단 입력은 owner 확인 사항이다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O16 · VSS Event 사용 한도·실제 시작

**CONFLICT / TEAM_DECISION_REQUIRED** · 담당: VSS + DOMAIN + Network

Network USE_LIMIT 500ms 후보와 구 SysRS의 원 발생 후 실제 시작 2초 미만 기준은 서로 다른 계층의 후보다. 시간 계약을 합의해야 한다.

500ms 또는 2초를 승인 deadline으로 적용하지 않는다. 양단의 수명과 VSS의 실제 시작 직전 검사 기준을 합의한다.

[관련 계약](02_NODES/VSS/10_DATA.md) · [관련 계약](03_COMMUNICATION/10_CAN_FD.md)

## O17 · VSS 상태 품질·원본 세대

**TEAM_DECISION_REQUIRED** · 담당: VSS + DOMAIN

C2 service.meta.quality·faultQuality·sourceGeneration과 축약된 VSS_STATUS의 대응이 미정이다.

서비스 품질과 고장 품질을 정상값으로 추정하지 않는다. 원본 세대·부분 진단은 필요한 내부 원장에 보존하고 합의 후 투영한다.

[관련 계약](02_NODES/VSS/10_DATA.md)

**PROPOSED:** [VSS 원 보고별 제한](02_NODES/VSS/10_DATA.md#r2-status-proposal)를 따른다. 이력 raw0/3과 C2 전체 품질은 동치가 아니다. VSS/DOMAIN이 current 완전성·원 평가/generation·최근 기록 소비를 확인하기 전 미확인 fresh Status 투영은 제한한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O18 · WINDOW 실제 보호 확인

**DEFERRED / TEAM_DECISION_REQUIRED** · 담당: WINDOW + DOMAIN

기존 C2 outputDisabled/movementStopped를 증명할 실제 확인 근거가 현재 State/Event Wire에 없다.

로컬 STOP과 끼임 보호가 우선이다. 상태 enum·수신 성공·timeout만으로 정지 또는 출력 차단 확인을 true로 만들지 않는다.

[관련 계약](02_NODES/WINDOW/10_DATA.md)

## O19 · 센서 범위·품질·Health 연결

**TEAM_DECISION_REQUIRED / BINDING_OPEN** · 담당: CIS/BCM + DOMAIN + Network

환경·거리 값의 표현 범위와 실제 센서 유효 범위·최신성, 로컬 통신 Health를 기능에 전달할 바인딩이 미정이다.

Wire 표현 가능 범위를 센서 정확도로 해석하지 않는다. 기능 담당자가 Network 근거를 받아 통신 진단을 반영하며 임의 고장을 합성하지 않는다.

[관련 계약](02_NODES/CIS/10_DATA.md) · [관련 계약](02_NODES/BCM/20_API.md)

**PROPOSED:** [센서별 품질과 Health 처리](02_NODES/CIS/10_DATA.md)를 따른다. 정상 센서값을 부분 실패와 독립적으로 보존한다. [BCM 실행·confirmation](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result)은 별도 owner 근거가 필요하다. 실제 진단 입력·피드백 책임을 확인해야 한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O20 · 환경 센서 원본 최신성

**CONFLICT / TEAM_DECISION_REQUIRED** · 담당: CIS 센서 + DOMAIN 기능 + Network

pinned Network §8과 제공 Stage B에는 온도·습도 2500ms, 실내 조도 1000ms라는 원본 AGE 사용 한도 후보가 있다. 구 SysRS §2.5 환경 행에는 2000ms 후보가 있으므로 양단 관계를 확인해야 한다. 더 짧은 조도 한도가 요구를 충족하는지도 함께 확인한다. CAN 보고 주기 200ms 및 수신 중단 600ms와는 별개다. 합의 전 승인 값으로 적용하지 않는다.

[관련 Data](02_NODES/CIS/10_DATA.md) · [통신·시간 후보](03_COMMUNICATION/10_CAN_FD.md)


## O21 · 부분 UserSettings 존재 정보

**TEAM_DECISION_REQUIRED / AFFECTED_PROJECTION_HOLD** · 담당: DOMAIN + ESP32 + MOBILE

C2 NORMAL level/RGB의 독립 has flag를 M_USER_SETTINGS raw에서 완전히 표현하지 못함. 종합 품질로의 축약 동등성 미증명.

없는 설정을 정상값으로 합성하지 않는다. 부분 설정 투영은 소비 의미와 무손실 표현을 합의할 때까지 차단한다.

[관련 계약](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md)

**PROPOSED:** [부분 Settings export 후보](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#r2-partial-export)를 따른다. 기존 sentinel만 사용하고 밝기/RGB presence 부재는 해당 composite 누락+partial Query로 제한한다. Settings/ESP32/MOBILE이 누락 표시를 확인해야 하며 확인된 0/OFF와 독립 정상 설정은 보존한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O22 · DigitalKey 실제 unlock 전이

**TEAM_DECISION_REQUIRED / AFFECTED_RESULT_HOLD** · 담당: DOMAIN + ESP32 + MOBILE

C2 actualUnlockTransition의 직접 raw bool 없음. ALREADY_AT_TARGET은 false이나 다른 조합의 동등성 미증명.

실제 전이를 결과 성공만으로 추정하지 않는다. 표현할 수 없는 조합의 export는 차단한다.

[관련 계약](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md)

**PROPOSED:** [DigitalResult 소비 후보](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#r2-partial-export)를 따른다. 원 DONE/reason을 보존하며 완료와 실제 unlock 전이 표시를 분리한다. MOBILE/기능 owner가 전이 확인의 별도 근거를 확인해야 하며 bool이 필요한 표시만 제한한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O23 · Gateway 다형 relay subtype

**TEAM_DECISION_REQUIRED / POLYMORPHIC_RELAY_HOLD** · 담당: ESP32 + MOBILE + Network

기존 broad category/ByteView relay에 typed subtype 전용 인자가 없음. schema registry의 logical tag·length·binding 미정.

정확한 subtype·payload 길이·소비 schema를 합의하기 전 해당 다형 relay를 차단한다. 기존 Gateway 원형은 변경하지 않는다.

[관련 계약](02_NODES/ESP32/25_PRIVATE_ROUTING.md)

**PROPOSED:** [23종 registry와 내부 convention](02_NODES/ESP32/25_PRIVATE_ROUTING.md#relay-schema)를 따른다. 기존 TYPE+P 논리 버퍼는 PROPOSED이며 Core/양측 adapter 채택이 필요하다. 실제 UART P/offset은 불변이고 RequestID/QueryID·현재 peer/session을 검증한다. ALIVE/QUERY_END는 private다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O24 · C2 snapshot 원 CAN provenance

**TEAM_DECISION_REQUIRED / AFFECTED_PROJECTION_HOLD** · 담당: DOMAIN + Network

QueryGet에 raw boot/TX_SEQUENCE가 없음. receive provenance와 source generation/UPDATE_SEQUENCE의 정확한 연결 미정.

원 수신 기록과 snapshot의 대응이 확인되지 않으면 원 boot/sequence를 새로 합성하지 않는다. 해당 projection을 차단한다.

[관련 계약](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md)

**PROPOSED:** [원 수신과 C2 admitted Snapshot 연결](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-origin-join)를 따른다. 원 NetDTO/Meta를 실제 같은 원 입력의 commit에 연결한 경우만 투영한다. DOMAIN owner가 중간 갱신 배제/원자적 결합 지점을 확인해야 하며 미증명 projection만 제외한다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.

## O25 · Query 실제 commit·종결

**TEAM_DECISION_REQUIRED / BINDING_HOLD** · 담당: DOMAIN + ESP32 + MOBILE + Network

M_QUERY_END의 actual frame count는 local queue admission 및 App receipt와 다름. 송신 complete commit·partial stream·footer ordering과 MOBILE 종료 schema 미정.

private Query 서비스는 유지한다. local OK로 actual count나 완료를 합성하지 않으며, 종결은 실제 commit 근거와 양단 schema를 합의한다.

[관련 계약](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md) · [관련 계약](02_NODES/ESP32/25_PRIVATE_ROUTING.md)

**PROPOSED:** [Query 송신 count·종결](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer)를 따른다. 송신 terminal 근거와 footer 순서를 확인해야 한다. [ESP32 수신 종결](02_NODES/ESP32/25_PRIVATE_ROUTING.md#r2-query-consumer) 및 BLE incomplete 표시·timeout은 별도 양단 합의 대상이다. 완료를 전체 최신성이나 차량 DONE으로 바꾸지 않는다. 상태는 제안이며 실제 채택·바인딩 근거는 없다.


## N01 · 구 SysRS MOBILE 3초 정책의 조건부 검토

**PROPOSED / CROSS_NODE_AGREEMENT / FUNCTION_OWNER / IMPLEMENTATION_BINDING** · 담당: MOBILE + ESP32 + DOMAIN

구 SysRS의 MOBILE SENT 후 3초 잠정 정책을 현재 승인 deadline으로 복구하지 않는다. [현재 Gateway 수명 후보](03_COMMUNICATION/20_UART_M.md#r2-request-deadline)는 수용 시점과 로컬 대기·원 문맥을 보존하고, 확인된 기한을 이미 넘은 요청만 차단하며 reconnect로 새 session/AGE0을 만들지 않는다. 원 SENT 시각·clock 불확실성·기한 합의 없이 end-to-end 3초 충족을 주장하지 않는다. MOBILE/DOMAIN 기능 owner는 현재 채택할 원 기준시각·수용 기한·재시도 정책을 확인해야 한다. Wire timestamp/새 raw 변경과 실제 바인딩은 현재 계약에 포함하지 않는다. 후속 합의 대상으로 유지한다.
