# 공통 Data와 식별·최신성 규칙

Interface v2 · r1 · [규격과 연동 조건](../07_CROSS_NODE_INTERFACE_RULES.md#status)

공통 타입과 원본 시간 정보·통신 처리 규칙을 정의한다. 확정된 시간 규칙, 권장 프로필과 미확정 기능 정책을 구분하며 실제 기능 연결·시험은 검증 전이다. 연결된 Data·API·Wire에 동일 규칙을 적용한다.

시간 기준은 [시간표](#section-timing), 반복 수신과 품질 저하는 [수신 처리](#section-repeat-and-quality), 남은 조건은 [미결과 연결 확인](#section-open-bindings)에서 확인한다.

**PROPOSED_DESIGN / BINDING_OPEN.** 이 문서의 C 표현은 설계용 선언이며 구현 헤더가 아니다. 기존 C2/Gateway 소스 선언은 각 노드 문서의 DECLARED_SOURCE를 따른다. struct의 메모리 배치·C enum 크기·패딩은 Wire layout이 아니다.

[시작 안내](../00_START_HERE.md) · [메시지별 API 색인](../05_MESSAGE_INDEX.md) · [C enum ↔ Wire 변환](30_ENUM_WIRE_MAPPING.md) · [호출·실패](20_API_USAGE_AND_FAILURE.md)

## 사용 순서

노드 Data의 구조체를 먼저 읽고 아래 공통 선언으로 멤버의 정확한 타입을 확인한다. Wire의 바이트 위치·길이는 [CAN](../03_COMMUNICATION/10_CAN_FD.md)·[UART-M](../03_COMMUNICATION/20_UART_M.md)·[UART-C](../03_COMMUNICATION/30_UART_C.md)에서 확인한다.

- 원 ID·기동·전송 순서와 원 평가 번호: [식별자별 생산자와 수명](#section-identity)
- 값의 품질·누적 AGE·수신 중단: [품질·AGE·수신 중단](#section-quality-freshness)
- 허용 메시지의 독립 품질·원 기한: [DOMAIN_PERMISSION 합성](#section-permission-synthesis) · [전체 7필드](#type-netdomainpermission)
- 호출 접수와 실제 실행 결과의 구분: [NetSubmitResult 전체 선언](#type-netsubmitresult) · [호출·비동기 실패](20_API_USAGE_AND_FAILURE.md)

## 타입 색인

| 타입 | 사용하는 위치 |
| --- | --- |
| [`NetSubmitResult`](#type-netsubmitresult) | 로컬 반환 |
| [`NetCanSource`](#type-netcansource) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetCanRxMeta`](#type-netcanrxmeta) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetUartRxMeta`](#type-netuartrxmeta) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetCommandMeta`](#type-netcommandmeta) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetRgb`](#type-netrgb) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetValueQuality`](#type-netvaluequality) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetValidity`](#type-netvalidity) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetQualityReason`](#type-netqualityreason) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetVehicleResult`](#type-netvehicleresult) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetResultConfirmation`](#type-netresultconfirmation) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetAvailability`](#type-netavailability) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetReason`](#type-netreason) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetFlag`](#type-netflag) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetTriState`](#type-nettristate) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetFanLevel`](#type-netfanlevel) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetThermalDirection`](#type-netthermaldirection) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetLightType`](#type-netlighttype) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetOrigin`](#type-netorigin) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetWarningType`](#type-netwarningtype) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetSeverity`](#type-netseverity) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetFunctionId`](#type-netfunctionid) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetFaultCategory`](#type-netfaultcategory) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetControllerState`](#type-netcontrollerstate) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetSensorFunctionState`](#type-netsensorfunctionstate) | 수신 메타 / Codec·노드 의미 자료 |
| [`NetDomainPermission`](#type-netdomainpermission) | DOMAIN → 4수신 허용·링크감시 |


<a id="section-identity"></a>
## 식별자별 생산자와 수명

CAN TX_SEQUENCE는 수용한 송신 노드 BOOT 안에서 메시지별로 관리한다. 수신 비교 범위는 송신 출처·BOOT·CAN ID/메시지 구분을 포함한다. CIS_ENVIRONMENT(TX1), CIS_OCCUPANT(TX1), CIS_ENVIRONMENT(TX2)는 서로 다른 메시지의 순번을 비교 범위에 섞지 않으면 정상 순서로 처리한다. 원 센서 관측 순번 및 UART-C 방향별 LINK_SEQUENCE와 합치지 않는다. 새 필드가 아니라 기존 카운터 비교 범위를 명확히 한 것이다. [노드 간 공통 규칙](#section-generation)

| 정보 | 원 생산자·수명 | 재전송/중계 규칙 |
| --- | --- | --- |
| DeviceContext / Session | ESP32 등록·현재 연결 담당; Session 발급 방식 OPEN | 현재 수신 Session과 과거 REQUEST_SESSION 분리; 재연결/번호 재사용 전 새 문맥 |
| Request ID / Query ID | MOBILE 새 사용자 입력 / 조회 | 원 ID 보존; Query는 실행 명령을 만들지 않음 |
| C2 commandId | DOMAIN 기능 최종 결정; DomainCommandId_t/u32 | Network 재발급 금지; 원 명령 재전송이면 동일 ID |
| decisionSequence | DOMAIN 내부 결정 provenance/u32 | peer Wire 필수 필드 아님; 내부 보존 |
| Occurrence ID | 각 기능의 새 사건 생산자/u32 | Domain 경고, BCM 사건, WINDOW 사건은 별도 공간; 반복 새 발생 금지 |
| BOOT_ID | 송신 노드 runtime/Network 기동 owner | 0 예약, 수용한 기동 변경 시 이전 문맥 무효화; Pi 원 BOOT를 CIS BOOT로 치환 금지 |
| C2 sourceGeneration | 해당 상태 원 생산자의 세대 | 통신 BOOT와 동일 lifecycle임을 확인한 때만 대응. 유효 관측 nonzero 후보, 미확인0과 STALE+0 정책 미결을 구분 |
| CAN TX_SEQUENCE / UART LINK_SEQUENCE | 송신 Network의 메시지/방향별 프레임 순서 | 송신 프레임마다 증가; 재전송도 증가; 새 관측 증거 아님 |
| UPDATE_SEQUENCE / VISION_SEQUENCE | 해당 기능·센서의 실제 새 평가/판정 | 새 원본에서만 증가; 캐시·재전송에서 보존 |
| SOURCE_AGE / EVENT_AGE / COMMAND_AGE | 각 원 관측 / 사건 / 명령 최초 생성 후 경과 | 서로 다른 기준; 각 구간 대기·전달 여유를 정확히 한 번만 누적 |


<a id="section-quality-freshness"></a>
## 품질·AGE·수신 중단

정상 반복의 수신 감시와 원 관측 신규성은 분리한다. 같은 관측의 새로운 품질 저하는 원 관측 순번·AGE를 보존해 기존 C2에 반영한다. 중복 제거로 수신 감시나 무효 품질 전달이 사라지지 않게 하는 구체 호출 조건은 [이 문서의 수신 처리](#section-repeat-and-quality)를 따른다. 사용 기한은 원 누적 AGE가 한도에 도달하면 만료로 판정하며, 시간표의 규격·후보 범위는 [노드 간 공통 규칙](#section-timing)에 구분한다.

원본 AGE는 호출 시점까지의 경과 ms다. TX Network는 인계 시 로컬 시각을 캡처하고 실제 송신 전에 그 뒤 대기만 더한다. RX는 해당 구간의 전달 여유를 한 번 반영하고 수신 시각을 보관한다. 중계 통합부는 RX 이후 대기를 한 번 더하고 다음 TX Network는 새 인계 이후 대기만 더한다. 서로 다른 노드의 클록 값을 직접 빼지 않는다.

`effective_age = min(65535, source_age + local_elapsed + not_yet_accounted_hop_allowance)`는 **포화·중복 방지 설명식**이다. u32 ageMs가 65535를 넘으면 u16 모듈로 연산으로 값을 줄이지 않는다. 원본 데이터의 사용 기한을 넘은 값은 STALE/INVALID로 소비하며 품질 없는 AGE 0·정상값을 합성하지 않는다. CAN 5 ms / UART 50 ms 전달 여유는 PROPOSED이며 실측값·보장 상한이 아니다.

포화값 65535는 실제 AGE가 그 이상일 수 있다는 하한이며 정확한 경과 상한이나 fresh의 증거가 아니다. 포화 또는 미확인 경과 시간 때문에 원 사용 한도 이내임을 확인할 수 없으면 그 정보에 의존하는 제어·허용·새 실행을 허용하지 않는다. 최근 고장 이력의 포화 AGE를 현재 고장이나 새 발생으로 바꾸지 않는다.

예: Pi 원 판정 10 ms + UART 전달 후보 50 ms = CIS 수신 60 ms, CIS 대기 20 ms 후 CAN AGE 80 ms, DOMAIN 수신에서 CAN 후보 5 ms를 한 번 반영하면 85 ms다. 같은 대기 20 ms를 `NetCanRxMeta.received_at_ms`와 DTO에 두 번 합산하지 않는다.

같은 원 세대와 원 관측·명령·사건 식별자의 재수신·순서 역전으로 현재 누적 AGE를 낮추지 않는다. 기존 기록과 새 수신의 AGE를 같은 로컬 소비 시점까지 각각 한 번 누적해 비교하며, 순서·원 시각을 확인할 수 없는 입력으로 최신 상태나 허용을 생성하지 않는다. 실제 sequence wrap·Boot admission·홉 시간 바인딩은 OPEN이다.

Frame receive timeout은 마지막 **검증·수용한 새 전송 프레임** 이후 경과다. 같은 원 관측을 새 전송 순번으로 반복한 정상 프레임은 포함하고, 같은 전송 순번·같은 raw의 정확 중복과 CRC/길이/enum reject는 제외한다. 새 TX_SEQUENCE만으로 원 생성시각·관측번호를 갱신하지 않으며 현재 AGE는 계속 증가한다. Event 무발생에 주기 timeout을 적용하지 않는다. NO_DATA/INVALID/STALE, 확인된 0/false, 후방 NO_OBJECT, 위험 CLEAR는 각각 의미가 다르다. [수용 분기](#section-repeat-and-quality)를 함께 적용한다.

<a id="section-permission-synthesis"></a>
## DOMAIN_PERMISSION 합성과 4수신

후방 활성 Power는 약 200ms 실제 재평가/원 500ms, CAN 50ms 송신/150ms 수신 감시, UART-C 반복 없음/1500ms 수신 감시를 적용한다. 독립 C2 입력의 합성과 POWER_OPERATION_STATE 생성원은 미결이다. 후방 거리 150ms 후보와 조도 1000ms 후보는 별도 정책이다. [Permission 합성](#section-permission-synthesis)

C2의 `DomainIf_CisPowerPermission_t`, `DomainIf_WindowOperationPermission_t`, `DomainIf_WindowDomainAlive_t`는 독립 판단·SignalMeta를 가진다. Wire는 한 메시지에 power quality와 operation quality를 나눠 싣지만 공통 UPDATE_SEQUENCE/AGE만 가진다.

**PROPOSED_DESIGN / FUNCTION_POLICY_OPEN:** private adapter는 독립 입력을 별도로 보관한다. 전원 허용 품질은 전원 원 입력을 보존하고, WINDOW operation quality는 operation과 실제 Domain alive 근거를 보수적으로 결합한다. 합성은 품질 숫자의 `max()`로 처리하지 않는다. 각 입력을 NO_DATA/INVALID/STALE/OK 의미로 검사하고 미확인·오래됨이 하나라도 필요한 판단에 포함되면 허용을 새로 생성하지 않는다. 같은 입력에서 `quality`와 `meta.quality`가 충돌하면 해당 입력은 확인 불가이며 임의 OK 선택을 금지한다.

공통 source_age_ms 후보는 포함 입력들의 **현재 누적 AGE 최대값**을 포화해 사용한다. 합성 update_sequence 후보는 검증된 원 입력 `(generation, sequence)` tuple 또는 실제 전달 의미·품질 제한이 달라질 때만 갱신한다. frame TX_SEQUENCE/50 ms Publish로 합성 원본 번호·AGE 0을 만들지 않는다. 입력별 provenance·정확한 합성 owner·POWER_OPERATION_STATE 생성원은 C2 단일 bool만으로 완전히 증명되지 않아 OPEN이다. 양 끝 모듈 조건 확정 전 허용 확정 구현을 차단한다.

BCM/VSS는 Network-private Domain 감시가 소비하며 새 App permission callback을 요구하지 않는다. CIS는 전원 허용, WINDOW는 동작 허용을 기능에서 소비한다. WINDOW는 **[구현 보류 — 설계 유지]**다. CIS→Pi P_PERMISSION은 원 DOMAIN 허용의 품질·누적 AGE를 보존하며 CIS가 새 허용을 만들지 않는다.


<a id="delta-permission-sequences"></a>
### 원 성공 평가·합성·전송의 번호 공간

실패·NO_DATA에서 ALLOW를 철회하면서 DOMAIN 내부 stateRevision을 증가시키는 동작은 담당자 구현 완료 보고다. 이는 원 성공 평가 Sequence 또는 CAN 합성 UPDATE_SEQUENCE·TX_SEQUENCE·UART LINK_SEQUENCE와 다르며 공개 필드 추가 근거가 아니다. 실패 때 마지막 성공 Sequence·시각을 유지해 AGE를 누적한다. 동일 원 Seq의 품질 변화 전달과 합성 갱신 조건은 실제 Network 연결 검증/계약 확인 대기다.

DOMAIN 원 평가 Sequence는 실제 평가 성공에서만 증가하며 반복 송신으로 변하지 않는다. 마지막 성공 평가 시각에서 원 AGE를 누적한다. 합성 순번 분리는 통신 권장 프로필 방향이지만 갱신 세부조건·공통 AGE/Quality/세대 합성은 양단 미결이다.

| 번호 | 생성 주체·갱신 사건 | 대체할 수 없는 근거 |
|---|---|---|
| DOMAIN 원 평가 Sequence | DOMAIN PermissionManager/Adapter의 실제 성공 평가 | 실패·재전송·링크 순번은 성공 평가 아님 |
| CAN 합성 UPDATE_SEQUENCE | Network 합성 경계, 검증 원 tuple 또는 실제 전달 의미·품질 제한 변경 후보 | DOMAIN 원 성공 평가와 다른 번호 공간 |
| CAN TX_SEQUENCE | Network 송신, 메시지별 실제 프레임 전송 | 합성·원 평가 신규성 아님 |
| UART LINK_SEQUENCE | UART 방향별 프레임 전송 | 원 평가·relay 갱신 아님 |
| P_PERMISSION relay UPDATE_SEQUENCE | CIS private relay 소유. CIS 기동마다 1부터, 새 원 평가·수용한 DOMAIN 세대 변경·실제 품질 제한 변경마다 +1, 같은 relay 기록을 다시 보낼 때는 유지 | DOMAIN 원 번호나 CAN 합성 번호의 무손실 전달 필드 아님 |

각 번호의 수치가 우연히 같아도 생성 의미·동일성은 다르다. FAILED/NO_DATA 시 기존 ALLOW의 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고다. [UNKNOWN의 원인별 C2 Quality 투영](../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)은 수용 대기이고 Network→CIS/Pi 전달은 검증 대기다. 검증된 제한이 전달될 때 성공17 뒤 실패는 원 성공17과 원 시각을 보존한 품질 하향으로 처리한다. 원 성공 Sequence 중복만으로 이 보고를 버리지 않는다. 새 성공/AGE0으로 위조하거나 정상 성공 이력이 없는데 source Sequence·시간을 만들지 않는다. 합성 품질 하향은 다음 성공 평가·500ms 만료까지 미루지 않는 안이고, CIS relay는 받은 품질 하향을 즉시 전달한다. 원격 동시 적용 보장은 아니다. 적체/송신 실패 뒤 구 ALLOW 재전달 방지는 연결 검증 대기다.

<a id="section-timing"></a>
## 원 사용 기한과 통신 시간표

<a id="r1-age-boundaries"></a>
**소비 시점 경계:** 후방 Power 원 평가 AGE 499ms는 다른 사용 조건까지 만족할 때만 사용 가능하며, 500ms와 501ms는 원 유효기한 도달/초과로 사용하지 않는다. 이는 원 평가 500ms의 적용이며 CAN 수신 감시 150ms나 P_PERMISSION.USE_LIMIT 후보의 변경이 아니다. 온도와 습도는 각각 원 AGE 2499ms에서 다른 품질 조건을 확인하고 2500ms·2501ms에서는 사용을 제한한다. 조도 1000ms는 별도의 미확정 권장 후보다. 이 경계는 설계 기대값이며 제품 시험 결과가 아니다.

아래 원 사용 기한은 원본 생성부터 실제 소비까지 누적한 AGE에 적용한다. `effective_age >= limit`이면 만료다. 수신 감시가 정상이어도 오래된 원본을 사용할 수 없으며, 시간 조건을 만족해도 품질·수용한 세대·기능 조건이 나쁘면 사용을 제한한다.

| 대상 | 실제 생성·평가 | 송신·보고 | 정상 프레임 수신 중단 | 원 사용 기한과 지위 |
|---|---|---|---|---|
| 후방 활성 Power Permission | DOMAIN 약 200ms 실제 재평가 | CAN 50ms 유지. 변경 또는 같은 값의 새 평가 완료 시 전달 | CAN 150ms 유지 | 500ms. 다른 Permission 전체에 확대하지 않음 |
| CIS→Pi P_PERMISSION | 검증된 원 DOMAIN 평가·세대 변경·실제 품질 제한을 중계 | 새 평가·세대 변경·제한 시 즉시. 반복 없음 | UART-C 1500ms 유지 | 검증된 원본 사용 기한 이하. USE_LIMIT 1500ms는 기존 후보, 같은 후방 Power 원본일 때 500ms는 조건부 권장 프로필 |
| CIS 온도·습도 | 각 센서의 실제 측정. 약 2000ms는 센서 측정 조건 참고값 | CAN 200ms 후보 | CAN 600ms 후보 | 각 2500ms. 센서별 품질·Sequence·AGE 독립 |
| CIS 실내 조도 | 해당 센서 실제 측정 | 환경 CAN 200ms 후보 | CAN 600ms 후보 | 1000ms 후보 미확정. 온습도 규칙에 포함하지 않음 |
| CIS 후방 거리 | 실제 새 거리 관측 | CAN 50ms 후보 | CAN 150ms 후보 | 원 150ms 기존 후보를 검증용으로 유지. SysRS 최대 500ms 잠정 및 Permission 500ms와 별개 |
| Pi 비전 판정 | 실제 새 판정에서 VISION_SEQUENCE 증가 | UART-C 100ms / CIS CAN 100ms 후보 | 각 300ms 후보 | 원 판정 1000ms 기존 후보 |
| Pi 기능 상태 | 실제 상태 평가에서 STATUS_SEQUENCE 증가 | UART-C 1000ms 후보 | UART-C 3000ms 후보 | 판정과 상태의 수명은 독립. READY로 이전 판정의 AGE를 갱신하지 않음 |

Permission은 값이 같아도 실제 새 평가가 성공하면 원 평가 번호와 시각을 갱신해 전달한다. CAN 50ms 반복은 새 평가가 아니며 UART-C P_PERMISSION은 반복하지 않는다. 원본 미확인·만료·재기동 뒤에는 이전 ALLOW를 유효하게 복원하지 않는다. 연속 유효 허용은 실제 새 평가 중계로만 유지되며 그 지연을 검증해야 한다.

USE_LIMIT 500ms 적용에는 같은 후방 Power 원 판단이라는 근거와 양단 바이트·AGE·만료 해석 검증이 필요하다. 그 전에는 기존 1500ms 후보를 배포 확정값으로 바꾸지 않는다. 어떤 USE_LIMIT도 검증된 원 수명을 연장하지 않는다. Pi에서 이를 카메라 정지나 비전 기능 제한에 사용할지는 CIS·Pi 기능 모듈이 정한다.

경계 예시는 설계상 기대값이다. 다른 유효 조건이 같을 때 거리 AGE 149ms는 시간 조건을 만족하고 150/180/500ms는 거리 사용 불가다. 후방 활성 Permission은 499ms까지 시간 조건을 만족하고 500ms부터 사용할 수 없다. 온습도는 각 2499ms까지 시간 조건을 만족하고 2500ms부터 해당 값을 사용할 수 없다. 실제 Host·보드 시험 결과가 아니다.

CIS 참고 코드의 ENV_STALE_MS 5000ms는 로컬 값 유효성 검사 후보이며 DOMAIN의 2500ms를 연장하지 않는다. 로컬 LUX_STALE_MS 1000ms도 DOMAIN 조도 기한을 확정하는 근거가 아니다. CAN 5ms / UART 50ms 홉 전달 여유는 기존 후보로 남기며 실측 상한으로 표현하지 않는다.

<a id="section-repeat-and-quality"></a>
## 정상 반복과 현재 품질을 전달하는 순서

통신 권장 프로필 VERSION·TYPE/ID·방향·길이·CRC·코드·수용 BOOT·전송 순서를 검사한 뒤 다음 세 경우를 구분한다. 같은 수용 BOOT에서 **같은 TX_SEQUENCE/LINK_SEQUENCE와 같은 raw의 정확한 중복**은 새 수신 생존 또는 원 freshness의 근거로 사용하지 않는다. **새 전송 순번의 정상 반복**은 같은 원 관측이어도 검증·수용 뒤 기능 수신감시를 갱신한다. 이 경로에서는 기존 `CisApp_OnPiVisionResult()` / `CisApp_OnPiVisionStatus()`를 호출하고 CIS가 수용 수신시각과 원 관측 신규성을 분리한다. 콜백 생략에는 동일한 수용 수신 사실이 다른 감시 경로에 전달된다는 실제 연결 증거가 필요하다. CRC·길이·예약 코드 오류와 구세대 프레임은 감시를 갱신하지 않는다. 같은 전송 순번인데 raw가 다른 경우도 정상 반복으로 승격하지 않고 기존 순서/형식 거절 및 진단 경로에서 처리한다.

통신 권장 프로필 원 관측의 동일성은 현재 품질의 불변을 뜻하지 않는다. 같은 Pi BOOT/VISION_SEQUENCE 42의 원 관측값과 원 생성 기준점을 유지한 채 실제 근거가 있는 VALID→INVALID/STALE 품질 저하를 새 CAN TX_SEQUENCE로 수용하면 기존 `DomainIf_RxCisOccupant()` 경로에 전달한다. **높은 TX_SEQUENCE만으로 원값 변경 또는 INVALID→VALID 승격을 허용하지 않는다.**원 관측 번호·생성시각은 고정하고 현재 AGE는 경과에 따라 증가시키며, AGE 동결·0 초기화·동일 구간 중복가산을 금지한다. 원 raw/BOOT/관측번호, 평가 품질·사유, 순서 상태와 C2 Meta의 연결은 하나의 수용 결과로 commit한다. 저장·인계 실패 때 일부 원본/품질/순서만 먼저 commit하여 재수용을 막거나 새 Getter에 구 provenance를 붙이지 않는다. DOMAIN 저장값·Getter까지의 원자적 연결은 실제 구현 검증 대기다. 지연된 구 VALID, 단순 READY 또는 링크 복구만으로 회복시키지 않으며 새 유효 관측과 기능 복구 규칙이 필요하다.

품질 평가에서는 Presence와 Count를 각각 검사한 다음, 같은 판정에서 두 필드가 모두 사용 가능할 때만 상호 모순을 판단한다. Presence=ABSENT/VALID, Count=255/INVALID/NO_DATA는 정상 Presence를 무효화하는 모순이 아니다. 두 필드가 모두 사용 가능한데 ABSENT/count>0 또는 PRESENT/count=0이면 양쪽을 INVALID/DATA_INVALID로 제한하는 권장 프로필을 적용한다. 원 Pi 사유와 이후 통신 단절·만료·기능 고장 근거는 별도로 보존한다. 한 벌의 CAN validity/reason이 그 모든 근거를 동시에 표현하지는 않는다.

통신 권장 프로필 CAN 순서 비교의 키는 송신 출처·수용한 BOOT·CAN ID/메시지다. ENV TX1 → OCC TX1 → ENV TX2는 정상이다. UART-C 방향별 LINK_SEQUENCE, 원 센서 UPDATE_SEQUENCE, Pi VISION_SEQUENCE/STATUS_SEQUENCE 및 CIS relay UPDATE_SEQUENCE와 혼용하지 않는다.

<a id="section-generation"></a>
## 통신 기동과 원 생산 세대

Wire BOOT_ID의 0 예약과 C2 sourceGeneration=0은 다른 규칙이다. 수용된 통신 기동이 바뀌면 그 출처의 이전 문맥을 무효화한다. C2 sourceGeneration은 상태를 생산한 원 세대를 뜻한다. BCM producer와 CAN BOOT가 같은 lifecycle이라는 근거가 있을 때만 대응시킨다. 지금은 그 대응과 실제 양단 연결이 검증되지 않았다.

제공된 Vehicle_Types v0.3 / C2 v0.6 후보는 유효한 실제 관측에 nonzero generation을 요구하고, 세대를 모를 때 0을 사용한다. INVALID/NO_DATA에서도 이미 알고 있는 세대를 0으로 지우지 않는다. STALE + sourceGeneration 0의 허용·거부는 정책 미결이며, 통과시키려고 품질을 NO_DATA로 바꾸거나 nonzero를 만들어 내지 않는다. 이 후보는 ClimateState_t와 InteriorLightState_t의 필드 보완을 포함하지만 공개 API 원형·Wire를 변경하지 않는다. 허용된 두 필드 추가는 DOMAIN 타입 문서에 반영했다.

같은 CIS BOOT에서 센서만 다시 초기화해 관측 순번을 임의로 되감지 않는 것은 통신 처리 규칙이다. 새 BOOT를 수용한 뒤의 낮은 관측 순번은 이전 세대의 높은 순번과 직접 비교하지 않는다. Pi의 A→B→지연 A에서는 종료한 세대 재수용을 막는 규칙을 검증한다. 구세대 기억 범위, 충돌과 runtime 재시작 처리는 확인이 남아 있다. 무작위 32비트 BOOT는 인증 또는 영구 재생 차단의 근거가 아니다.

<a id="section-environment-binding"></a>
## 환경값의 독립 메타와 무효화

Network는 CIS BOOT와 온도·습도·조도별 Sequence·AGE·품질을 보존한다. 기존 `DomainIf_RxCisCabinEnvironment()`는 값 수신 경계이며 새 BOOT 무효화는 DOMAIN 소유 단일 동기 private 경계로 처리한다. 전환용 NO_DATA를 공개 Rx에 중복 전달하지 않는다. 같은 BOOT의 센서 NO_DATA·만료·수신 중단은 기존 Rx·품질 정책을 유지한다. 일반 링크 단절의 STALE/NO_DATA 선택과 VSM·독립 Meta·Getter까지의 실제 연동 검증은 별도 미결이다.

환경 C2 구조는 우선 유지한다. 현재 구조로 필요한 무효화·세대 전이를 표현하거나 연결할 수 없다는 구체적인 근거가 나오면 최소 계약 변경을 검토할 수 있다. 이는 영구 확장 금지가 아니다. Network 원 수신 기록을 임의의 최신 Getter 결과에 붙이지 않고, DOMAIN이 실제 처리한 입력과 같은 관측인지 확인해야 한다.

<a id="delta-environment-order"></a>
**CIS 환경 연속 전환 — DOMAIN 순차 Epoch 구현 확인 / Network 바인딩 대기:** transitionEpoch는 Wire나 센서 관측 번호가 아닌 로컬 전환 순서다. 유일성은 DOMAIN과 Network가 정상 실행을 지속하고 초기 상태가 맞는 현재 구간에 한정한다. 양쪽 적용 상태·진행 요청·구 작업을 맞춘 명시적 초기화 뒤 첫 Epoch1, Network가 직전 적용 완료를 현재 요청과 대조하고 새 CIS BOOT를 검증한 뒤 다음 +1을 한 번 발급한다. 미완료 중 다음 번호 선발급·동시 승인과 실행 중 감소·reset·wrap 재사용은 금지한다. 실제 타입 최대값은 오류·환경 전달 차단이며 타입 폭이나 Epoch0 예약 의미를 새로 정하지 않는다. 같은 BOOT 센서 갱신·일반 NO_DATA·CAN 재전송은 발급 사건이 아니다.

환경 C2에는 BOOT/sourceGeneration이 없다. Network는 자기 소유 구 대기·콜백·미시작 환경 Query 응답을 차단/직렬 처리하고 고정 요청을 동기 private API로 한 번 전달한다. DOMAIN은 이미 인계받은 구 쓰기의 완료 후 재저장을 막고 VSM·독립 Meta·관련 순서 문맥을 무효화한다. Network는 현재 요청 내용·적용 Epoch·명확한 완료를 확인한 뒤 새 원 관측을 기존 Rx로 전달한다. Queue 접수·호출 사실·일반 C2 OK는 완료 증거가 아니다. 현재 마지막 적용 Epoch·동일 내용만 기존 완료 결과를 재사용하며 이후 정상값을 다시 지우지 않는다. 충돌·과거·건너뜀·실패의 Core 판정 구현과 실제 Network 호출·Receipt 바인딩 미검증은 [private 전환 정본](../02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#delta-environment-routing)을 따른다. DOMAIN Production Private Core의 첫 Epoch 1·완료 후 N+1·중복/충돌·STALE/ORDER_ERROR 처리는 소스로 확인됐으며 Host 시험은 담당자 보고다. Network의 실제 Private 호출·Receipt 대조와 환경 Rx/VSM/Query 연결은 미검증이므로 해당 환경 경로만 HOLD한다.

Network는 `(원 CIS BOOT, 센서, 원 관측 Sequence)`로 A900→B0을 구분하고 원 번호를 재발급·리매핑하지 않는다. DOMAIN도 이를 세대 없는 전역 Sequence 비교로 거절하지 않고 검증 입력 순서를 저장까지 수용해야 한다. 일반 NO_DATA는 데이터 부재이며 재부팅/Sequence 리셋 명령이 아니다. 새 입력 대기 AGE는 한 번 누적하고 온습도2500ms 만료를 정상으로 되돌리지 않는다. 종료 BOOT 차단은 기존 기억 범위에 한정하며 Epoch가 CIS BOOT_ID 재사용·구분 근거 부족을 해결하지 않는다. 실패·단독 재시작에서 자동 재시도·복구·영속 완료 승계를 하지 않고 구 작업 정리와 양쪽 초기 정합 전 환경 전달을 보류한다. 실제 초기화 호출·수명 및 CIS BOOT 생성 조건은 확인 대기다. 일반 링크 단절의 STALE/NO_DATA·조도 TTL은 미결이다. [전체 조건과 C2 변경 검토 대안](../07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)을 따른다.

<a id="section-failure-boundary"></a>
## 접수와 실행·실패의 구분

NET_SUBMIT_OK / DOMAIN_IF_OK는 각각 해당 로컬 경계의 성공을 뜻하며 신규 Manager 저장까지 보증하지 않는다. 현재 `DomainIf_RxCisRear()`는 중복 관측도 DOMAIN_IF_OK로 매핑하므로 새 반영은 원 입력 및 Query 결과로 구분한다. CAN/UART 송신 완료, 상대 수용 또는 차량 DONE을 의미하지 않는다. 접수 전 INVALID_ARGUMENT·NOT_READY·NO_CAPACITY는 미접수 반환이며 해당 거절을 접수 후 LocalFailure로 다시 만들지 않는다.

정상 접수된 필요한 명령·결과·일회성 이벤트의 LocalFailure는 **driver 제출의 확정 최종 실패**, **송신 전 만료**, **송신 전 문맥 무효화**의 기존 세 사유와 정확히 맞을 때만 원 ID와 함께 전달한다. 이미 제출한 뒤의 미분류 in-flight 오류나 callback 부재를 이 세 사유로 넓혀 해석하지 않는다. [세 reason과 실패 인계 조건](20_API_USAGE_AND_FAILURE.md#r1-local-failure-reasons)을 따른다. 주기 상태·Query 실패는 각 통신·Query 경로를 사용한다. CIS·Pi에 같은 C2 API를 새로 추가하지 않는다. 정상 형식의 INVALID/STALE 보고는 품질 보고이며 형식 거절과 구분한다.

<a id="section-open-bindings"></a>
## 남은 정책과 연결 확인

| 항목 | 영향·관련 경로 | 담당과 필요한 근거 | 현재 제한 |
|---|---|---|---|
|  Power/Operation/Alive 합성 | NetDomainPermission, DOMAIN_PERMISSION, P_PERMISSION | DOMAIN 기능 생산 근거와 Network 합성·공통 AGE 검증 | 원 품질·AGE를 입증하지 못한 입력으로 정상 ALLOW를 생성하지 않음 |
| 조도 1000ms | CIS_ENVIRONMENT 조도 소비 | DOMAIN 소비 정책, CIS 측정 조건 | 온습도 2500ms로 일괄 확정하지 않음 |
| 환경 VSM 실제 무효화 | DomainIf_RxCisCabinEnvironment → Manager → Getter | DOMAIN의 약속된 코드 확인과 독립 Meta 증거 | Network 전달 Mock만으로 저장 무효화 성공을 선언하지 않음 |
| STALE + generation 0 | sourceGeneration이 있는 C2 타입 | DOMAIN/C2 정책의 허용·거부 결정 | 임의 세대 생성·품질 치환 금지 |
| USE_LIMIT 500ms | CisNetPriv_RelayPiPermission → PiApp_OnCisPermission | Network/CIS/Pi의 원본 동일성·바이트·만료 검증 | 조건부 권장 프로필 유지, 실제 송신값 확정 아님 |
| 후방 거리 150ms | CisNet_PublishRear → DomainIf_RxCisRear | CIS 측정·Network 전달·DOMAIN 소비 가용성 | 기존 검증 후보 유지, Permission 500ms와 별도 |
| BOOT 수용·원본 귀속 | CIS 콜백, CAN 순서, DOMAIN 저장 연결 | 각 모듈의 runtime 연결·지연/복구 시험 | 권장 프로필과 구현 검증을 구분 |

DOMAIN 공개 API·Manager는 DOMAIN, Codec·Adapter·TxPort는 Network가 구현한다. VSS 통신과 기능 연결은 같은 구현 범위이며 ESP 내부 구현·MOBILE 직접 연동은 별도 범위다. CIS·Pi는 통신 규격을 기능에 연결하고 Pi 카메라 사용 정책을 결정한다. WINDOW의 기존 구현 보류 경로와 BCM/VSS의 Network-private 감시는 유지한다.

<a id="type-netsubmitresult"></a>
## `NetSubmitResult`

로컬 제출 결과. 상대 수신·차량 수용·실행 완료를 의미하지 않는다.

```c
typedef enum NetSubmitResult
{
    NET_SUBMIT_OK = 0,             /**< 필요한 필드의 복사·로컬 보관을 완료했다. */
    NET_SUBMIT_INVALID_ARGUMENT = 1,    /**< NULL, 예약 코드, 필드 조합 또는 범위가 유효하지 않다. */
    NET_SUBMIT_NOT_READY = 2,           /**< 링크·현재 문맥이 이 제출을 수용할 준비가 되지 않았다. */
    NET_SUBMIT_NO_CAPACITY = 3      /**< 필요한 로컬 보관 공간이 없다. 기존 개별 항목을 덮어쓰지 않는다. */
} NetSubmitResult;
```

| 상수 | 로컬 C 값 | 사용 조건 |
| --- | --- | --- |
| `NET_SUBMIT_OK` | 0 | SUBMIT OK |
| `NET_SUBMIT_INVALID_ARGUMENT` | 1 | SUBMIT INVALID ARGUMENT |
| `NET_SUBMIT_NOT_READY` | 2 | SUBMIT NOT READY |
| `NET_SUBMIT_NO_CAPACITY` | 3 | SUBMIT NO CAPACITY |

이 enum은 로컬 반환형이다. Wire RESULT에 인코딩하지 않는다.


<a id="type-netcansource"></a>
## `NetCanSource`

CAN 원 송신 헤더의 출처. 중계 표시에서도 원 값을 보존한다.

```c
typedef struct NetCanSource
{
    uint32_t boot_id;       /**< 송신 노드 기동 번호. 0 예약. */
    uint32_t tx_sequence;   /**< 같은 메시지의 전송 순번. 새 관측 번호가 아니다. */
} NetCanSource;
```

| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `boot_id` | `uint32_t` | 원 CAN Network(보존) | 송신 노드 기동 번호. 0 예약. |
| `tx_sequence` | `uint32_t` | 원 CAN Network(보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |


<a id="type-netcanrxmeta"></a>
## `NetCanRxMeta`

CAN 수신 메타. Network가 생성하며 callback 호출 동안 읽기 전용이다.

```c
typedef struct NetCanRxMeta
{
    NetCanSource source;
    uint32_t received_at_ms; /**< 로컬 단조 증가 ms. 노드 간 시계 비교에 사용하지 않는다. */
} NetCanRxMeta;
```

| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `source` | [`NetCanSource`](10_DATA_AND_ID_RULES.md#type-netcansource) | 수신 Network | 동일 이름의 원본 필드 규칙을 따른다. |
| `received_at_ms` | `uint32_t` | 수신 Network | 로컬 단조 증가 ms. 노드 간 시계 비교에 사용하지 않는다. |


<a id="type-netuartrxmeta"></a>
## `NetUartRxMeta`

UART 수신 메타. Session·요청 식별은 메시지의 별도 문맥을 사용한다.

```c
typedef struct NetUartRxMeta
{
    uint16_t link_sequence;  /**< 해당 방향의 전송 순번. 차량 요청 ID가 아니다. */
    uint32_t received_at_ms; /**< 로컬 단조 증가 ms. */
} NetUartRxMeta;
```

| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `link_sequence` | `uint16_t` | 수신 Network | 해당 방향의 전송 순번. 차량 요청 ID가 아니다. |
| `received_at_ms` | `uint32_t` | 수신 Network | 로컬 단조 증가 ms. |


<a id="type-netcommandmeta"></a>
## `NetCommandMeta`

ECU 명령 문맥. `command_id`는 Domain C2 기능 결정 owner가 생성하며 Network는 전송 메타를 보완한다.

```c
typedef struct NetCommandMeta
{
    uint32_t target_boot_id;  /**< 마지막 상태로 확인한 대상 ECU 기동 번호. */
    uint32_t command_id;      /**< Domain C2 기능 결정 owner가 최초 부여하는 명령 ID. Network가 중복 생성하지 않고 같은 명령 재송신 시 유지. */
    uint16_t command_age_ms;  /**< Domain C2 명령 birth AGE에 Network 대기·전송 지연을 누적한 ms; 반복 전송에서 초기화 금지. */
    uint16_t accept_limit_ms; /**< Wire 제안 200ms. SysRS Domain→ECU 1초 잠정안과 미정 차이 확인; 임의 상향 금지. */
} NetCommandMeta;
```

| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `target_boot_id` | `uint32_t` | Domain Network | 마지막 상태로 확인한 대상 ECU 기동 번호. |
| `command_id` | `uint32_t` | Domain C2 기능 결정 | Domain C2 기능 결정 owner가 최초 부여하는 명령 ID. Network가 중복 생성하지 않고 같은 명령 재송신 시 유지. |
| `command_age_ms` | `uint16_t` | Domain Network | Domain C2 명령 birth AGE에 Network 대기·전송 지연을 누적한 ms; 반복 전송에서 초기화 금지. |
| `accept_limit_ms` | `uint16_t` | Domain Network | Wire 제안 200ms. SysRS Domain→ECU 1초 잠정안과 미정 차이 확인; 임의 상향 금지. |


<a id="type-netrgb"></a>
## `NetRgb`

**선언 종류:** 구조체

```c
typedef struct NetRgb
{
    uint8_t r; /**< 빨강. 0~255. */
    uint8_t g; /**< 초록. 0~255. */
    uint8_t b; /**< 파랑. 0~255. */
} NetRgb;
```

| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `r` | `uint8_t` | 해당 정보 생산 기능 | 빨강. 0~255. |
| `g` | `uint8_t` | 해당 정보 생산 기능 | 초록. 0~255. |
| `b` | `uint8_t` | 해당 정보 생산 기능 | 파랑. 0~255. |


<a id="type-netvaluequality"></a>
## `NetValueQuality`

값의 품질. 정상 상태값과 별도로 해석한다.

```c
typedef uint8_t NetValueQuality;
enum NetValueQualityCode
{
    NET_QUALITY_OK = 0,
    NET_QUALITY_STALE = 1,
    NET_QUALITY_INVALID = 2,
    NET_QUALITY_NO_DATA = 3
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_QUALITY_OK` | 0 | QUALITY OK |
| `NET_QUALITY_STALE` | 1 | QUALITY STALE |
| `NET_QUALITY_INVALID` | 2 | QUALITY INVALID |
| `NET_QUALITY_NO_DATA` | 3 | QUALITY NO DATA |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netvalidity"></a>
## `NetValidity`

관측값의 유효 여부.

```c
typedef uint8_t NetValidity;
enum NetValidityCode
{
    NET_VALIDITY_INVALID = 0,
    NET_VALIDITY_VALID = 1
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_VALIDITY_INVALID` | 0 | VALIDITY INVALID |
| `NET_VALIDITY_VALID` | 1 | VALIDITY VALID |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netqualityreason"></a>
## `NetQualityReason`

품질 사유. 원본 사유는 별도 근거로 보존한다. 이후 확인된 장애에 따른 최종 전달 사유의 제한은 [정상 반복과 현재 품질](#section-repeat-and-quality)을 따른다.

```c
typedef uint8_t NetQualityReason;
enum NetQualityReasonCode
{
    NET_QUALITY_REASON_NONE = 0,
    NET_QUALITY_REASON_NOT_READY = 1,
    NET_QUALITY_REASON_OUT_OF_RANGE = 2,
    NET_QUALITY_REASON_SENSOR_FAULT = 3,
    NET_QUALITY_REASON_VISION_FAULT = 4,
    NET_QUALITY_REASON_STALE = 5,
    NET_QUALITY_REASON_NO_DATA = 6,
    NET_QUALITY_REASON_COMMUNICATION_FAULT = 7,
    NET_QUALITY_REASON_RECOVERING = 8,
    NET_QUALITY_REASON_DATA_INVALID = 9
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_QUALITY_REASON_NONE` | 0 | QUALITY REASON NONE |
| `NET_QUALITY_REASON_NOT_READY` | 1 | QUALITY REASON NOT READY |
| `NET_QUALITY_REASON_OUT_OF_RANGE` | 2 | QUALITY REASON OUT OF RANGE |
| `NET_QUALITY_REASON_SENSOR_FAULT` | 3 | QUALITY REASON SENSOR FAULT |
| `NET_QUALITY_REASON_VISION_FAULT` | 4 | QUALITY REASON VISION FAULT |
| `NET_QUALITY_REASON_STALE` | 5 | QUALITY REASON STALE |
| `NET_QUALITY_REASON_NO_DATA` | 6 | QUALITY REASON NO DATA |
| `NET_QUALITY_REASON_COMMUNICATION_FAULT` | 7 | QUALITY REASON COMMUNICATION FAULT |
| `NET_QUALITY_REASON_RECOVERING` | 8 | QUALITY REASON RECOVERING |
| `NET_QUALITY_REASON_DATA_INVALID` | 9 | QUALITY REASON DATA INVALID |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netvehicleresult"></a>
## `NetVehicleResult`

차량 처리 결과. NOT_PRESENT는 미확인 응답의 필드 없음 표시이며 차량 결과 단계가 아니다.

```c
typedef uint8_t NetVehicleResult;
enum NetVehicleResultCode
{
    NET_RESULT_ACCEPTED = 0,
    NET_RESULT_IN_PROGRESS = 1,
    NET_RESULT_DONE = 2,
    NET_RESULT_REJECTED = 3,
    NET_RESULT_CANCELLED = 4,
    NET_RESULT_FAILED = 5,
    NET_RESULT_NOT_PRESENT = 255
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_RESULT_ACCEPTED` | 0 | RESULT ACCEPTED |
| `NET_RESULT_IN_PROGRESS` | 1 | RESULT IN PROGRESS |
| `NET_RESULT_DONE` | 2 | RESULT DONE |
| `NET_RESULT_REJECTED` | 3 | RESULT REJECTED |
| `NET_RESULT_CANCELLED` | 4 | RESULT CANCELLED |
| `NET_RESULT_FAILED` | 5 | RESULT FAILED |
| `NET_RESULT_NOT_PRESENT` | 255 | RESULT NOT PRESENT |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netresultconfirmation"></a>
## `NetResultConfirmation`

결과 확인 여부. 미확인을 실행 실패로 바꾸지 않는다.

```c
typedef uint8_t NetResultConfirmation;
enum NetResultConfirmationCode
{
    NET_CONFIRMATION_UNCONFIRMED = 0,
    NET_CONFIRMATION_CONFIRMED = 1
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_CONFIRMATION_UNCONFIRMED` | 0 | CONFIRMATION UNCONFIRMED |
| `NET_CONFIRMATION_CONFIRMED` | 1 | CONFIRMATION CONFIRMED |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netavailability"></a>
## `NetAvailability`

기능 가용성. 상태 정상 여부와 구분한다.

```c
typedef uint8_t NetAvailability;
enum NetAvailabilityCode
{
    NET_AVAILABILITY_AVAILABLE = 0,
    NET_AVAILABILITY_LIMITED = 1,
    NET_AVAILABILITY_UNAVAILABLE = 2
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_AVAILABILITY_AVAILABLE` | 0 | AVAILABILITY AVAILABLE |
| `NET_AVAILABILITY_LIMITED` | 1 | AVAILABILITY LIMITED |
| `NET_AVAILABILITY_UNAVAILABLE` | 2 | AVAILABILITY UNAVAILABLE |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netreason"></a>
## `NetReason`

결과·가용성 사유. 실제 생성하는 사유는 각 기능의 기존 계약을 따른다.

```c
typedef uint16_t NetReason;
enum NetReasonCode
{
    NET_REASON_NONE = 0,
    NET_REASON_NOT_REGISTERED = 1,
    NET_REASON_LINK_LOST = 2,
    NET_REASON_SESSION_INVALID = 3,
    NET_REASON_DOOR_OPEN = 4,
    NET_REASON_STATE_UNTRUSTED = 5,
    NET_REASON_CMD_INVALID = 6,
    NET_REASON_NO_FEEDBACK = 7,
    NET_REASON_DRIVE_LIMIT_EXCEEDED = 8,
    NET_REASON_ALREADY_AT_TARGET = 9,
    NET_REASON_LOCAL_OVERRIDE = 10,
    NET_REASON_STOP_REQUESTED = 11,
    NET_REASON_SUPERSEDED = 12,
    NET_REASON_ANTIPINCH = 13,
    NET_REASON_FAULT = 14,
    NET_REASON_STALE = 15,
    NET_REASON_NO_DATA = 16,
    NET_REASON_NOT_READY = 17,
    NET_REASON_OUT_OF_RANGE = 18,
    NET_REASON_SOURCE_FAILURE = 19,
    NET_REASON_UNKNOWN_REQUEST = 20,
    NET_REASON_EXPIRED = 21,
    NET_REASON_ID_CONFLICT = 22,
    NET_REASON_POWER_NOT_ALLOWED = 23,
    NET_REASON_OVERHEAT = 24,
    NET_REASON_FAN_MISMATCH = 25
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_REASON_NONE` | 0 | REASON NONE |
| `NET_REASON_NOT_REGISTERED` | 1 | REASON NOT REGISTERED |
| `NET_REASON_LINK_LOST` | 2 | REASON LINK LOST |
| `NET_REASON_SESSION_INVALID` | 3 | REASON SESSION INVALID |
| `NET_REASON_DOOR_OPEN` | 4 | REASON DOOR OPEN |
| `NET_REASON_STATE_UNTRUSTED` | 5 | REASON STATE UNTRUSTED |
| `NET_REASON_CMD_INVALID` | 6 | REASON CMD INVALID |
| `NET_REASON_NO_FEEDBACK` | 7 | REASON NO FEEDBACK |
| `NET_REASON_DRIVE_LIMIT_EXCEEDED` | 8 | REASON DRIVE LIMIT EXCEEDED |
| `NET_REASON_ALREADY_AT_TARGET` | 9 | REASON ALREADY AT TARGET |
| `NET_REASON_LOCAL_OVERRIDE` | 10 | REASON LOCAL OVERRIDE |
| `NET_REASON_STOP_REQUESTED` | 11 | REASON STOP REQUESTED |
| `NET_REASON_SUPERSEDED` | 12 | REASON SUPERSEDED |
| `NET_REASON_ANTIPINCH` | 13 | REASON ANTIPINCH |
| `NET_REASON_FAULT` | 14 | REASON FAULT |
| `NET_REASON_STALE` | 15 | REASON STALE |
| `NET_REASON_NO_DATA` | 16 | REASON NO DATA |
| `NET_REASON_NOT_READY` | 17 | REASON NOT READY |
| `NET_REASON_OUT_OF_RANGE` | 18 | REASON OUT OF RANGE |
| `NET_REASON_SOURCE_FAILURE` | 19 | REASON SOURCE FAILURE |
| `NET_REASON_UNKNOWN_REQUEST` | 20 | REASON UNKNOWN REQUEST |
| `NET_REASON_EXPIRED` | 21 | REASON EXPIRED |
| `NET_REASON_ID_CONFLICT` | 22 | REASON ID CONFLICT |
| `NET_REASON_POWER_NOT_ALLOWED` | 23 | REASON POWER NOT ALLOWED |
| `NET_REASON_OVERHEAT` | 24 | REASON OVERHEAT |
| `NET_REASON_FAN_MISMATCH` | 25 | REASON FAN MISMATCH |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netflag"></a>
## `NetFlag`

0/1 표시. 구체적인 의미는 각 필드의 설명을 따른다.

```c
typedef uint8_t NetFlag;
enum NetFlagCode
{
    NET_FLAG_NO = 0,
    NET_FLAG_YES = 1
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_FLAG_NO` | 0 | FLAG NO |
| `NET_FLAG_YES` | 1 | FLAG YES |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-nettristate"></a>
## `NetTriState`

0/1/확인 불가 표시. 필드마다 0/1의 기능 의미가 다르다.

```c
typedef uint8_t NetTriState;
enum NetTriStateCode
{
    NET_TRISTATE_NO = 0,
    NET_TRISTATE_YES = 1,
    NET_TRISTATE_UNKNOWN = 255
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_TRISTATE_NO` | 0 | TRISTATE NO |
| `NET_TRISTATE_YES` | 1 | TRISTATE YES |
| `NET_TRISTATE_UNKNOWN` | 255 | TRISTATE UNKNOWN |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netfanlevel"></a>
## `NetFanLevel`

Fan 수준. 명령에는 UNKNOWN을 사용하지 않는다.

```c
typedef uint8_t NetFanLevel;
enum NetFanLevelCode
{
    NET_FAN_OFF = 0,
    NET_FAN_LOW = 1,
    NET_FAN_MEDIUM = 2,
    NET_FAN_HIGH = 3,
    NET_FAN_UNKNOWN = 255
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_FAN_OFF` | 0 | FAN OFF |
| `NET_FAN_LOW` | 1 | FAN LOW |
| `NET_FAN_MEDIUM` | 2 | FAN MEDIUM |
| `NET_FAN_HIGH` | 3 | FAN HIGH |
| `NET_FAN_UNKNOWN` | 255 | FAN UNKNOWN |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netthermaldirection"></a>
## `NetThermalDirection`

공조 출력 방향.

```c
typedef uint8_t NetThermalDirection;
enum NetThermalDirectionCode
{
    NET_THERMAL_IDLE = 0,
    NET_THERMAL_COOL = 1,
    NET_THERMAL_HEAT = 2
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_THERMAL_IDLE` | 0 | THERMAL IDLE |
| `NET_THERMAL_COOL` | 1 | THERMAL COOL |
| `NET_THERMAL_HEAT` | 2 | THERMAL HEAT |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netlighttype"></a>
## `NetLightType`

실내 조명 의미. App이 경고·고장 종류를 직접 명령하지 않는다.

```c
typedef uint8_t NetLightType;
enum NetLightTypeCode
{
    NET_LIGHT_NORMAL = 0,
    NET_LIGHT_GOODBYE = 1,
    NET_LIGHT_WARNING = 2,
    NET_LIGHT_FAULT = 3
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_LIGHT_NORMAL` | 0 | LIGHT NORMAL |
| `NET_LIGHT_GOODBYE` | 1 | LIGHT GOODBYE |
| `NET_LIGHT_WARNING` | 2 | LIGHT WARNING |
| `NET_LIGHT_FAULT` | 3 | LIGHT FAULT |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netorigin"></a>
## `NetOrigin`

요청·발생의 출처. 로컬 발생에 MOBILE 원 요청을 합성하지 않는다.

```c
typedef uint8_t NetOrigin;
enum NetOriginCode
{
    NET_ORIGIN_NONE = 0,
    NET_ORIGIN_MOBILE_MANUAL = 1,
    NET_ORIGIN_PROXIMITY_AUTO = 2,
    NET_ORIGIN_LOCAL = 3,
    NET_ORIGIN_TEST = 4,
    NET_ORIGIN_VEHICLE_POLICY = 5
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_ORIGIN_NONE` | 0 | ORIGIN NONE |
| `NET_ORIGIN_MOBILE_MANUAL` | 1 | ORIGIN MOBILE MANUAL |
| `NET_ORIGIN_PROXIMITY_AUTO` | 2 | ORIGIN PROXIMITY AUTO |
| `NET_ORIGIN_LOCAL` | 3 | ORIGIN LOCAL |
| `NET_ORIGIN_TEST` | 4 | ORIGIN TEST |
| `NET_ORIGIN_VEHICLE_POLICY` | 5 | ORIGIN VEHICLE POLICY |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netwarningtype"></a>
## `NetWarningType`

경고 종류. WINDOW 관련 경고는 설계를 유지하고 실물 연동을 보류한다.

```c
typedef uint8_t NetWarningType;
enum NetWarningTypeCode
{
    NET_WARNING_REAR = 1,
    NET_WARNING_OCCUPANT = 2,
    NET_WARNING_ANTIPINCH = 3,
    NET_WARNING_BCM_FAULT = 4,
    NET_WARNING_CIS_FAULT = 5,
    NET_WARNING_WINDOW_FAULT = 6,
    NET_WARNING_VSS_FAULT = 7,
    NET_WARNING_OPEN_DOOR_AFTER_LEAVE = 8
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_WARNING_REAR` | 1 | WARNING REAR |
| `NET_WARNING_OCCUPANT` | 2 | WARNING OCCUPANT |
| `NET_WARNING_ANTIPINCH` | 3 | WARNING ANTIPINCH |
| `NET_WARNING_BCM_FAULT` | 4 | WARNING BCM FAULT |
| `NET_WARNING_CIS_FAULT` | 5 | WARNING CIS FAULT |
| `NET_WARNING_WINDOW_FAULT` | 6 | WARNING WINDOW FAULT |
| `NET_WARNING_VSS_FAULT` | 7 | WARNING VSS FAULT |
| `NET_WARNING_OPEN_DOOR_AFTER_LEAVE` | 8 | WARNING OPEN DOOR AFTER LEAVE |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netseverity"></a>
## `NetSeverity`

경고 심각도. 판단 임계는 기능 담당 범위다.

```c
typedef uint8_t NetSeverity;
enum NetSeverityCode
{
    NET_SEVERITY_INFO = 0,
    NET_SEVERITY_CAUTION = 1,
    NET_SEVERITY_EMERGENCY = 2
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_SEVERITY_INFO` | 0 | SEVERITY INFO |
| `NET_SEVERITY_CAUTION` | 1 | SEVERITY CAUTION |
| `NET_SEVERITY_EMERGENCY` | 2 | SEVERITY EMERGENCY |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netfunctionid"></a>
## `NetFunctionId`

M_AVAILABILITY의 기능 식별자. WINDOW는 App 제어 추가를 뜻하지 않는다.

```c
typedef uint8_t NetFunctionId;
enum NetFunctionIdCode
{
    NET_FUNCTION_DOOR = 1,
    NET_FUNCTION_CLIMATE = 2,
    NET_FUNCTION_INTERIOR_LIGHT = 3,
    NET_FUNCTION_DIGITAL_KEY = 4,
    NET_FUNCTION_OCCUPANT = 5,
    NET_FUNCTION_ENVIRONMENT = 6,
    NET_FUNCTION_REAR_WARNING = 7,
    NET_FUNCTION_WINDOW = 8,
    NET_FUNCTION_VSS = 9
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_FUNCTION_DOOR` | 1 | FUNCTION DOOR |
| `NET_FUNCTION_CLIMATE` | 2 | FUNCTION CLIMATE |
| `NET_FUNCTION_INTERIOR_LIGHT` | 3 | FUNCTION INTERIOR LIGHT |
| `NET_FUNCTION_DIGITAL_KEY` | 4 | FUNCTION DIGITAL KEY |
| `NET_FUNCTION_OCCUPANT` | 5 | FUNCTION OCCUPANT |
| `NET_FUNCTION_ENVIRONMENT` | 6 | FUNCTION ENVIRONMENT |
| `NET_FUNCTION_REAR_WARNING` | 7 | FUNCTION REAR WARNING |
| `NET_FUNCTION_WINDOW` | 8 | FUNCTION WINDOW |
| `NET_FUNCTION_VSS` | 9 | FUNCTION VSS |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netfaultcategory"></a>
## `NetFaultCategory`

BCM/WINDOW 고장 범주.

```c
typedef uint8_t NetFaultCategory;
enum NetFaultCategoryCode
{
    NET_FAULT_CATEGORY_NONE = 0,
    NET_FAULT_CATEGORY_SENSOR = 1,
    NET_FAULT_CATEGORY_FUNCTION = 2,
    NET_FAULT_CATEGORY_COMM = 3
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_FAULT_CATEGORY_NONE` | 0 | FAULT CATEGORY NONE |
| `NET_FAULT_CATEGORY_SENSOR` | 1 | FAULT CATEGORY SENSOR |
| `NET_FAULT_CATEGORY_FUNCTION` | 2 | FAULT CATEGORY FUNCTION |
| `NET_FAULT_CATEGORY_COMM` | 3 | FAULT CATEGORY COMM |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netcontrollerstate"></a>
## `NetControllerState`

BCM/WINDOW/Domain의 현재 제어기 상태 코드. 각 노드의 상태 전이 조건은 별도다.

```c
typedef uint8_t NetControllerState;
enum NetControllerStateCode
{
    NET_CONTROLLER_INIT = 0,
    NET_CONTROLLER_READY = 1,
    NET_CONTROLLER_DEGRADED = 2,
    NET_CONTROLLER_FAULT = 3
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_CONTROLLER_INIT` | 0 | CONTROLLER INIT |
| `NET_CONTROLLER_READY` | 1 | CONTROLLER READY |
| `NET_CONTROLLER_DEGRADED` | 2 | CONTROLLER DEGRADED |
| `NET_CONTROLLER_FAULT` | 3 | CONTROLLER FAULT |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netsensorfunctionstate"></a>
## `NetSensorFunctionState`

CIS/Pi 기능 상태. 준비 상태와 새 관측 여부를 구분한다.

```c
typedef uint8_t NetSensorFunctionState;
enum NetSensorFunctionStateCode
{
    NET_SENSOR_FUNCTION_NOT_READY = 0,
    NET_SENSOR_FUNCTION_READY = 1,
    NET_SENSOR_FUNCTION_ACTIVE = 2,
    NET_SENSOR_FUNCTION_FAULT = 3,
    NET_SENSOR_FUNCTION_RECOVERING = 4
};
```

| 상수 | C 표현 값 / 제안 Wire code | 사용 조건 |
| --- | --- | --- |
| `NET_SENSOR_FUNCTION_NOT_READY` | 0 | SENSOR FUNCTION NOT READY |
| `NET_SENSOR_FUNCTION_READY` | 1 | SENSOR FUNCTION READY |
| `NET_SENSOR_FUNCTION_ACTIVE` | 2 | SENSOR FUNCTION ACTIVE |
| `NET_SENSOR_FUNCTION_FAULT` | 3 | SENSOR FUNCTION FAULT |
| `NET_SENSOR_FUNCTION_RECOVERING` | 4 | SENSOR FUNCTION RECOVERING |

별칭의 폭은 C 표현용이다. 필드별 허용 코드 검사 후 명시 Codec으로 해당 u8/u16에 기록한다. 기존 C2 enum의 숫자와는 별도다.


<a id="type-netdomainpermission"></a>
## `NetDomainPermission`

DOMAIN_PERMISSION. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetDomainPermission
{
    NetTriState vehicle_power_permission; /**< VEHICLE_POWER_PERMISSION: 0=불허; 1=허용; 255=확인 불가 */
    NetValueQuality power_permission_quality; /**< POWER_PERMISSION_QUALITY: VALUE_QUALITY */
    NetTriState power_operation_state; /**< POWER_OPERATION_STATE: 0=이동 불가; 1=이동 가능; 255=확인 불가 */
    NetFlag operation_allowed; /**< OPERATION_ALLOWED: 0=불허; 1=허용 */
    NetValueQuality operation_quality; /**< OPERATION_QUALITY: VALUE_QUALITY */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 허용 상태의 새 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 합성에 포함한 원 판단의 보수적 누적 경과, ms; 단순 새 송신 시 0으로 초기화 금지 */
} NetDomainPermission;
```

#### 필드 구성

| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `vehicle_power_permission` | [`NetTriState`](10_DATA_AND_ID_RULES.md#type-nettristate) | DOMAIN C2 전원 허용 판단 | VEHICLE_POWER_PERMISSION: 0=불허; 1=허용; 255=확인 불가 |
| `power_permission_quality` | [`NetValueQuality`](10_DATA_AND_ID_RULES.md#type-netvaluequality) | 위 전원 허용의 원 SignalMeta·품질 | POWER_PERMISSION_QUALITY: VALUE_QUALITY |
| `power_operation_state` | [`NetTriState`](10_DATA_AND_ID_RULES.md#type-nettristate) | 생성원 FUNCTION_POLICY_OPEN | POWER_OPERATION_STATE: 0=이동 불가; 1=이동 가능; 255=확인 불가 |
| `operation_allowed` | [`NetFlag`](10_DATA_AND_ID_RULES.md#type-netflag) | DOMAIN C2 WINDOW operation 판단 | OPERATION_ALLOWED: 0=불허; 1=허용 |
| `operation_quality` | [`NetValueQuality`](10_DATA_AND_ID_RULES.md#type-netvaluequality) | operation·Domain alive 원 품질의 보수 결합 후보 | OPERATION_QUALITY: VALUE_QUALITY |
| `update_sequence` | `uint32_t` | Network private 합성 adapter 권장 방향, 상세 binding OPEN | UPDATE_SEQUENCE: 검증된 원 입력 tuple 또는 실제 전달 의미·품질 제한이 바뀐 새 합성 평가의 후보. DOMAIN 성공 평가 Sequence·frame 반복과 구분 |
| `source_age_ms` | `uint16_t` | 원 판단 AGE를 보존하는 DOMAIN private 합성 후보 | SOURCE_AGE: 합성에 포함한 원 판단의 보수적 누적 경과, ms; 단순 새 송신 시 0으로 초기화 금지 |


## 고정 폭 무효값

```c
enum { NET_AVAILABILITY_ITEM_COUNT = 9 };
#define NET_AGE_SATURATED_MS UINT16_MAX
#define NET_UNKNOWN_U8 UINT8_MAX
#define NET_INVALID_TEMPERATURE INT16_MIN
#define NET_INVALID_U16 UINT16_MAX
#define NET_INVALID_ILLUMINANCE UINT32_MAX
```

NO_DATA의 65535와 오래된 값의 65535는 quality로 구분한다. RGB 0~255는 모두 정상 표현 가능하므로 255를 RGB 무효값으로 쓰지 않는다. `NetDomainPermission`은 공유 의미 DTO이며 CAN header는 이 타입에 붙이지 않고 Network가 생성한다.
