# 최소 CAN LinkPort — 작동 의미 계약 후보

[↑ Driver/Port Layer](00_LAYER_OVERVIEW.md) · [Layer 지도](../../01_LAYER_MAP.md) · [Network](../../00_START_HERE.md) · [CIS_REAR](../../flows/10_CIS_REAR_FLOW.md) · [DOMAIN_PERMISSION](../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**PROPOSED_PRIVATE / BINDING_OPEN / TARGET_NOT_VERIFIED / G2_INDEPENDENT=PENDING.** 출처 `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 범위는 두 CAN 흐름이 하위 경계에 요구하는 의미와 실패 구분이다. 함수 이름/서명·enum·public header/ABI·SDK/HAL 구현을 여기서 정의하지 않는다. 새 Layer·전역 Router·범용 Framework가 아니다.

<a id="outcome-boundaries"></a>
## 결과의 다섯 경계

| 관측 | 증명하는 사실 | 아직 증명하지 않는 다음 결과 |
|---|---|---|
| typed `NET_SUBMIT_OK` / C2 local OK | caller 저장소와 무관한 bounded 안정 인계 | CAN Port admission |
| CAN submit accepted | 하위 경계가 이 frame을 안정 소유·상관하여 처리하기로 수용 | controller TX 완료 |
| controller terminal complete | 실제 local controller의 해당 송신 종료를 관측 | 지정 ECU 수신·App 수용 |
| 실제 원격 ECU RX/typed 전달 | 해당 ECU의 구조/기동/순서 검증·경계 호출을 관측 | 기능 owner의 사용·차량 실행 |
| 실제 App 결과 | 기존 기능 owner가 근거로 보고한 실행 결과 | 다른 ECU나 미시험 시나리오 전체 성공 |

CAN ACK는 특정 소비자 App의 ACCEPTED/DONE이 아니다. void callback은 전달 notification이며 보관·기능 수락·음향 시작·완료를 반환하지 않는다. accepted 이후 실패는 과거 stable admission을 취소하는 동기 반환이 아니며 상관된 실제 terminal/health 관측으로 남긴다.

<a id="capability"></a>
## Capability — REQUIRED_BUT_UNVERIFIED

| 필요한 능력 | 두 흐름의 검토 기준 | 실제 확인 방법 / owner |
|---|---|---|
| CAN FD metadata | 후보 standard 11-bit ID·FDF/BRS·RTR 미사용, 원 raw DLC와 실제 length 접근 | 각 노드 SDK/Driver/B0~B3/U10 |
| length/보관 | 두 후보 19 B 사용/20 B 전송/DLC 11, payload 전체 독립 인계·길이/용량 검사 | target sample·buffer contract·Host 후속 시험 |
| accepted/terminal | stable accepted와 busy/no space/not ready/invalid를 구별하고 실제 complete/fail/unknown 관측 | Driver 반환·callback·수명·reset 증거 |
| RX metadata | ID/flags/DLC/length·bytes·로컬 시각·controller error를 손실 없이 인계 | ISR/RX buffer lifetime 및 U10 측정 |
| health | bus-off/reset/recovery/overflow/drop 관측, 미관측 상태를 정상으로 보고하지 않음 | 실제 Driver 사건과 상위 진단 연결 |
| clock/progress | 로컬 단조시각·wrap과 owner progress/종료를 설명 가능한 방식으로 제공 | timer 읽기 원자성·Task/superloop 진척·자원 측정 |

S32K144와 S32K344, 노드별 OS/SDK/backend가 같은 수치·API·동시성/버퍼를 제공한다고 가정하지 않는다. 물리 bitrate 후보 500 kbps/2 Mbps·핀/PHY/전압·종단과 실제 RAM/ISR/latency/stack은 미검증이다. B-track 근거+G9+타깃 SDK 검토 없이 U10 binding을 완료로 표시하지 않는다.

<a id="submit-and-ownership"></a>
## Submit·accepted와 buffer ownership

상위가 ID·flags·사용/전송 길이·payload·그 frame의 로컬 correlation 근거를 준비한다. 실제 schema/ABI는 U4D 이후다. 검사는 `used_length <= tx_length <= owned_buffer_capacity`, 후보 DLC와 tx_length 일치, header/field offset의 `offset + width <= used_length`, payload의 완전한 생존을 요구한다. 두 후보의 padding 검토 범위는 **19 ≤ offset < 20**의 한 바이트이며 0이다. 실제 Codec/byte loop 구현은 이 문서에서 만들지 않는다. `sizeof(struct)`가 검사를 대체하지 않는다.

| 조건 | 의미 후보 | 소유·commit |
|---|---|---|
| 불법 metadata/길이/구조 | invalid | 새로운 하위 admission 없음 |
| backend·현재 기동/clock 미준비 또는 능력 부족 | not ready / unsupported 관측 | 정상 능력·accepted로 보정하지 않음 |
| 일시적 busy | 아직 수용하지 않음 | 상위가 현재 owner 소유권을 유지, 무제한 retry 없음 |
| 유한 공간 부족 | no space | 이전 accepted frame/필요한 discrete 자료를 silent overwrite하지 않음 |
| 전체 독립 복사 또는 명시 exclusive transfer 완료 | accepted | callback 반환 이후에도 유효한 Port-owned bytes와 correlation의 commit이 있어야 함 |

typed OK 시 caller 포인터를 보관하지 않는 계약과 Port submit buffer 수명은 별개 경계다. 동기 encode를 해도 송신 전 원 AGE와 필요한 payload 표현을 갱신하는 책임이 남는다. zero-copy는 독점 소유권·수명 증거가 없는 한 성립한 것으로 취급하지 않는다. 호출 문맥/락/큐 수치와 대기·교체 알고리즘은 `BOUNDARY_PENDING_U4D`다.

<a id="terminal-and-correlation"></a>
## Terminal complete·fail·unknown과 늦은 사건

accepted frame은 해당 로컬 제출과 backend 세대에 연결된 correlation을 유지해야 한다. Driver callback 내부 buffer는 callback 유효 기간만 빌릴 수 있으며 필요한 사건/자료는 owner 저장소로 독립 인계해야 한다. exact token 구조·callback ABI·임의 token API를 U2에서 신설하지 않는다.

| 관측 | 상위 처리 요구 |
|---|---|
| complete | 해당 local 송신 종료만 기록. 네 ECU 수신·ALLOW/DONE 생성 없음 |
| 확인된 fail | 원 상관 근거와 실제 오류를 보존. Vehicle FAILED로 합성 없음 |
| completion 신뢰 근거 없음/timeout·reset 중 불명확 | unknown을 fail 또는 complete로 단정하지 않음 |
| 중복 또는 이미 terminal인 항목의 late callback | 종료 상태를 되돌리거나 다시 commit하지 않음 |
| 이전 backend/BOOT·재사용된 저장소에 대한 late 사건 | 현재 frame/새 기동 자료에 연결하지 않음 |
| Port recovery | 과거 accepted 자료의 결과가 자동 성공하거나 원 평가가 새로 발생하지 않음 |

event 누락·correlation 부재·재진입/동시성 한계도 실제 진단으로 노출해야 한다. 이를 해결하는 slot manager/terminal state machine과 retry 정책은 U4D, 실 SDK callback 검증은 U10이다. Current State의 손실을 Command/Result identity용 공개 failure API에 억지로 연결하지 않는다. 기존 owner별 health 경계의 적용을 확인한다.

<a id="rx-delivery"></a>
## RX injection·owner 처리

RX 사건은 실제 ID·FDF/BRS/RTR·DLC·length·bytes·controller 오류·로컬 수신 시각을 제공해야 한다. ISR은 필요 최소 복사/관측 인계만 수행한다. 무거운 Codec·generation admission·기능 의미 판단과 typed callback은 해당 owner Task/superloop 경계로 넘긴다. C2는 Task-only이며 다른 노드의 superloop 가능성을 C2 ISR 허용으로 확대하지 않는다.

구조·필수 code·수용 generation/순서 오류는 raw 진단으로 격리한다. 같은 CAN bus에 다른 ECU가 보내는 frame을 DOMAIN/CIS 생존으로 세지 않는다. 정상 형식의 원 INVALID/STALE/NO_DATA와 구조 오류를 분리한다. 새 TX_SEQUENCE·최근 RX 시각은 원 SOURCE_AGE/UPDATE의 freshness를 증명하지 않는다. RX 사본 공간이 없으면 overflow/drop을 실제 관측하며 가짜 callback이나 정상 자료를 만들지 않는다.

<a id="health-clock-progress"></a>
## Link health·time·progress

bus-off·overflow·reset·recovery·drop과 최초 관측/정상 RX는 Network 사실이다. 기능의 고장 선언·clear·보호·복구·음향 동작은 기존 owner가 판단한다. MCU reset과 source producer generation을 구별하며 정상 자료 없이 peer-ready를 만들지 않는다. recovery의 재제출은 원 UPDATE/AGE를 보존하고 무조건 과거 ALLOW를 복원하지 않는다.

단조 로컬 시간은 원 AGE에 아직 반영하지 않은 로컬 경과를 한 번 더하는 데 사용한다. 노드 간 timestamp를 직접 빼지 않는다. u16 AGE 포화는 하한이며 wrap·clock read 원자성·순서 비교·실제 지연의 수치 보장은 OPEN이다. monotonic clock 장애/미관측이면 deadline 충족이나 fresh를 주장하지 않는다.

worker/superloop가 진행할 필요와 shutdown/reset 후 더 이상 처리할 수 없다는 사실을 owner에 알려야 한다. wake/notification count를 message count로 취급하지 않는다. Task priority·pump 주기·queue/stack 크기·무한 retry를 U2에서 확정하지 않는다.

<a id="blocked-by"></a>
## BLOCKED_BY 및 교체 검토

`OPEN-PORT-CAPABILITY`와 `OPEN-SDK/HW/TIME`의 owner는 타깃 Driver/보드/RTOS 담당이다. 후속 Host Mock는 위 의미를 가상 사건으로 시험할 후보이며 현재 `HOST_EXECUTION=NOT_EXECUTED`다. 실제 backend는 B0~B3/U10에서 같은 accepted/terminal/RX/health/time 의미와 buffer lifetime을 입증해야 한다.

`OPEN-WIRE`, `OPEN-PERM`, `OPEN-PROVENANCE`, `OPEN-UNIT/ENUM`, `OPEN-C2-SOURCE`는 이 Port가 해결하지 않는다. 최소 내부 backend 능력과 상위 의미가 양립하지 않으면 `CR_PORT_COMPAT`에 원인·영향·대안을 남기며 상위 API/Wire를 무단 바꾸지 않는다. UART-C/M 및 Pi/ESP32 구현·WINDOW 활성화·EXTERIOR 복구는 이 U2 범위 밖이다.
