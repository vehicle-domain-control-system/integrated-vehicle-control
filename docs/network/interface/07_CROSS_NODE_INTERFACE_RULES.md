# 노드 간 Interface 규칙

Interface v2 · r1 · [기술 제약과 연동 조건](06_TECHNICAL_CONSTRAINTS.md)

DOMAIN·Network·CIS·Pi의 생산·전달·소비 경계와 시간·품질·기동 규칙을 정의한다. 노드별 Data/API와 CAN/UART Wire를 함께 적용한다.

<a id="status"></a>
## 규격과 연동 조건

정의된 계약과 권장 프로필, 미확정 정책, 구현 검증 전 조건을 구분한다. 권장 프로필의 기능 적용에는 각 절의 전제조건이 필요하며 문서의 선언만으로 구현 동작을 보장하지 않는다.

| 표시 | 적용 의미 |
|---|---|
| DECLARED_SOURCE | 참조 헤더의 선언. 실제 빌드의 동일 헤더·ABI 확인 필요 |
| PROPOSED_DESIGN / WIRE_PROPOSED | 설계 또는 Wire 권장 프로필. 양단 지원 조건 확인 필요 |
| CONDITIONAL | 명시한 전제조건이 충족된 범위에서만 적용 가능 |
| POLICY_OPEN / POLICY_OR_CONTRACT_OPEN | 기능 정책 미확정. 해당 사용 조건 확정 전 정상 동작으로 취급하지 않음 |
| BINDING_OPEN / BINDING_NOT_VERIFIED | 실제 Port·Queue·Manager 연동 검증 전 |
| DEFERRED | 설계 유지·구현 보류 |

<a id="roles"></a>
## 1. 구현과 데이터 소유권

DOMAIN은 공개 DomainIf_* 72개, Lifecycle, Rx/Tx 래퍼, Query Getter18개와 기존 Manager 연결을 구현한다. Network는 TxPort Callback26개, CAN/UART Codec·Queue·Adapter·프레임 검증·송수신 및 통신 원자료를 담당한다. SetTxPort 공개 함수 자체는 DOMAIN이 구현하고 Network가 실제 Callback을 구성해 등록한다. 미지원 Port는 성공으로 처리하지 않는다.

차량 상태·요청·명령·결과의 단일 관리 주체는 DOMAIN이다. Network가 관리하는 원 수신 객체·품질 근거·Queue·통신 Health는 두 번째 차량 상태 Manager가 아니다. Rx는 검증→디코딩→C2 인자 전달→DOMAIN Manager, Tx는 DOMAIN 래퍼→Network TxPort→통신, Query는 Network가 DOMAIN Getter 호출→응답 전송 순서다.

C2 Rx/Query는 ISR이 아닌 직렬 Task 문맥에서 처리한다. Manager→C2 Init→Observer→TxPort→통신 시작은 초기화 순서이며 구체 Worker/DOMAIN Queue·재등록 실패 처리는 연결 검증이 남아 있다. CIS Superloop나 Pi Python에 공통 RTOS를 강제하지 않는다.

CIS·Pi 기능 모듈은 기존 API·UART-C v1·C2 v0.6 후보에 맞춰 연결한다. S32K344/144 Network 코드는 Network, CIS 센서·power_gate·vision_logic는 CIS, Pi 비전과 Python Port 및 실제 비전 Permission 적용은 Pi 기능 모듈의 범위다. ESP32 내부 통신 구현과 MOBILE 직접 연동은 해당 모듈의 범위이며 Network는 ESP32–DOMAIN 외부 API/Wire를 담당한다. VSS 기능과 통신의 내부 연결은 하나의 구현 범위다.


<a id="query"></a>
## 2. 최근 요청 이력과 Query

DOMAIN은 Device별 최근 최대 8건을 보관하고, 최근 결과 Getter는 최신순 최대 5건을 제공한다. Network는 기존 M_RESULT를 기록당 1프레임으로 순차 전송하고 M_QUERY_END로 종료한다. 새 UART Frame은 없다. 5건은 최근 요청 결과 조회 범위이며 모든 상태 Query 응답의 총량 제한이 아니다.

동일 Device/Session/Request ID의 1기록 갱신 방식과 내부 저장 정책은 미확정이며 실제 ResultManager 연결 검증이 필요하다. 같은 요청의 중복 응답으로 새 표시 항목을 만들지 않는다. 진행 중인 요청을 완료 결과만으로 대체하거나 누락하지 않는지 ResultManager 연결에서 확인해야 한다. 이력 8건·조회 5건 규칙만으로 내부 알고리즘 전체가 정의되지는 않는다.

ITEM_COUNT를 실제 로컬 UART TX 완료 수로 세는 것과 종결의 부분 실패·0건 의미는 Network–ESP32 연결의 권장 프로필이다. 이력 8건·조회 5건 규칙은 MOBILE 전달 완료 또는 모든 항목의 최신성을 보장하지 않는다. Getter 실패를 성공적인 빈 목록으로 표현하지 않는다.


<a id="permission"></a>
## 3. Permission

DOMAIN은 후방 활성 Power를 약 200ms마다 실제 재평가하며 원 판단의 사용 기한은 500ms다. 값 변경은 즉시 전달하고 동일 값이라도 실제 새 평가가 성공하면 그 평가 순번·시각을 전달한다. CAN 50ms 송신/150ms 수신 중단과 UART-C 500ms 반복/1500ms 수신 중단을 적용한다. 단순 반복으로 원 AGE를 0으로 만들거나 기한을 연장하지 않는다. 미확인·만료·재기동 이후 이전 ALLOW를 유효한 값으로 복원하지 않는다.

CIS는 같은 검증된 NetDomainPermission과 수신 메타를 기존 App Callback과 private Pi relay에 전달한다. App은 후방 기능을, Network relay는 검증·제한·중계를 담당한다. 별도 App용 허용 생성 API를 만들지 않는다.

P_PERMISSION.USE_LIMIT는 1500ms 후보이며, 같은 후방 Power 원 판단을 Pi까지 전달함이 확인되면 500ms로 싣는 조건부 권장 프로필이다. 실제 송신값은 양단 지원·만료 동작 검증 전에는 확정되지 않는다. 검증된 원 수명과 USE_LIMIT 중 더 짧은 기한을 적용한다. Pi 카메라 중지·비전 게이트의 기능 적용과 통신 수신·만료 검사는 별개다.

정책 미결 : POWER_OPERATION_STATE의 생성원, Power/Operation/Alive의 필수 입력, 단일 UPDATE_SEQUENCE/SOURCE_AGE 합성. 기존 합성 후보를 보존하며 확인되지 않은 품질·원 시간을 생성하지 않는다. WINDOW 미구현 때문에 확인된 Power를 자동 불허로 바꾸지 않는다. 원 품질·공통 AGE를 입증하지 못하면 그 입력으로 정상 ALLOW를 주장하지 않는다. WINDOW 관련 작동을 재개하거나 새로운 DTO·Wire 필드를 자동 추가하지 않는다.


<a id="permission-delta"></a>
### Permission Adapter의 투영 조건

후방 활성 Permission의 원 사용 기한은 500ms이며 구 원 TTL 150ms 후보를 적용하지 않는다. CAN 수신 중단 150ms, 후방 거리 원 150ms 후보, UART 수신 중단 1500ms는 각각 별개다.

**권장안 / DOMAIN 기능 정책 미확정:** 의미 투영은 DOMAIN Permission Adapter가 수행하고 Network는 투영된 C2의 품질 일치·원 시간·문맥을 검증한다. `UNKNOWN+OK`를 `false+OK`로 조용히 바꾸면 확정 DENY와 구분되지 않는다. 기존 bool+quality+meta로 그 세 상태를 무손실 표현할 수 없다는 한계를 유지한다.

| 원 기능 의미 | C2 권장 투영 | CAN 전원 값·품질 | 조건 |
|---|---|---|---|
| 유효한 확정 ALLOW | true + OK | 1 + OK | 원 시간·세대·품질 근거 모두 유효 |
| 정상 평가에서 확정 DENY | false + OK | 0 + OK | 평가 실패와 구분 |
| 미관측·판단 근거 없음 | false + NO_DATA | 255 + NO_DATA | 해당 원인이 확인된 경우 |
| 이전 판단 수명 만료 | false + STALE | 255 + STALE | 과거 ALLOW 사용 불가 |
| 신뢰할 수 없는 평가 실패 | false + INVALID 권장 | 255 + INVALID 권장 | 실패 즉시 무효화와 매핑의 DOMAIN 수용 필요 |
| UNKNOWN+OK 원 품질 보존 필수 | 현재 C2로 무손실 표현 불가 | 임의 DENY/OK 치환 금지 | 의미 축약 정책 수용 또는 최소 타입·사유 계약 변경 별도 검토 |

원인별 매핑표는 권장 프로필이다. 원 Quality가 OK였던 UNKNOWN을 비정상 Quality로 투영하면 원 품질이 축약된다. 원 의미·원 품질·원인 근거는 DOMAIN 기능 원 기록에 보존하는 방향을 권장하며 실제 보존 위치는 미확정이다. C2 `quality`와 `meta.quality` 충돌은 확인 불가로 제한하는 기존 CAN 후보를 적용하고 임의 OK 선택을 금지한다. Network가 원 의미를 추측해 투영하거나 신규 사유 필드를 추가하지 않는다.

**실패 즉시 UNKNOWN은 권장안이며 DOMAIN 기능 정책·연동 검증 전이다.** 성공 평가 #17 ALLOW/OK 뒤 200ms에 평가 실패가 나면 원 성공 Sequence #17과 마지막 성공 시각은 그대로이며, 현재 품질을 INVALID/UNKNOWN으로 제한해 전달 요청하는 안이다. 동일 원 성공 Sequence라는 이유로 실패 보고를 중복 폐기하지 않는다. 실패를 새 성공 평가나 AGE0으로 만들지 않으며 성공 이력이 없을 때 정상 source Sequence·AGE를 만들어내지 않는다. 유효하지 않은 시간 근거의 상세 Wire 표현은 기존 NO_DATA·포화 규칙과 대조할 미결로 남긴다.

‘즉시’는 실패 인지 후 다음 성공 평가나500ms 만료까지 기다리지 않고 무효화·전달을 요청한다는 뜻이다. 원격 CIS/Pi의 동시 수신을 보증하지 않는다. Queue 적체·송신 실패·재시도에서 과거 ALLOW가 무효화 뒤 재전달되지 않는지 확인해야 하며 전송 불능을 성공으로 기록하지 않는다. [순번·원 AGE 분리](04_COMMON/10_DATA_AND_ID_RULES.md#delta-permission-sequences)와 [경계 입력과 처리 조건](#delta-acceptance-cases)을 따른다.

DOMAIN 의미 생성→Network 검증·합성은 권장 책임 배치다. POWER_OPERATION_STATE 생성원과 Power/Operation/Alive 필수 입력, 공통 AGE·Quality·세대 조건이 미결이므로 양단 합성규칙은 완성되지 않았다.

<a id="timing"></a>
## 4. 하나로 보는 시간 기준

아래 경과 시간은 수신 프레임 도착 시각만이 아니라 원 생성부터 실제 소비까지의 검증된 누적 AGE를 뜻한다. 원 기한에 도달하면 사용하지 않는 경계와 수신 중단 감시는 분리한다.

| 대상 | 보고/송신 | 수신 중단 | 원 사용 기한·현재 지위 |
|---|---|---|---|
| 후방 활성 Permission | CAN50ms; 실제 DOMAIN 평가 약200ms | CAN150ms | 원500ms |
| CIS→Pi Permission | 새 평가·제한 시 가능한 즉시, 반복500ms | UART1500ms | 검증된 원 수명 이하. USE_LIMIT1500 후보/500 조건부 |
| 온도·습도 | CAN200ms 후보, 센서 약2000ms 참고 | CAN600ms 후보 | 각 원2500ms |
| 실내 조도 | 환경 CAN200ms 후보 | CAN600ms 후보 |1000ms 후보 미확정; 로컬 코드1000과 구분 |
| 후방 거리 | CAN50ms 후보 | CAN150ms 후보 | Network 원150ms 후보; SysRS 원500ms 잠정과 구분 |
| Pi 판정 | UART100ms 후보, CAN100ms 후보 | 각300ms 후보 | 원 판정1000ms 기존 후보 |
| Pi 상태 | UART1000ms 후보 | UART3000ms 후보 | 상태와 관측의 수명 독립. 관측을 새로 만들지 않음 |

후방 거리 원 150ms는 Network 권장 후보이며 후방 활성 Permission 500ms와 별개다. SysRS 최대 500ms보다 일찍 사용을 제한하는 조건이지만 실제 측정·전송·소비 가용성은 미검증이다. 거리 소비 정책 변경이 필요하면 해당 한도와 센서·전달 주기를 함께 검토해야 한다.

경계값 예: 거리 유효조건이 모두 같을 때 effective_age149ms는 시간조건만 만족,150/180/500ms는 거리 사용 불가다. Permission은 별도로499ms는 시간조건만 만족,500ms부터 사용 불가다. 어느 경우든 품질·기동·프레임 수신 상태가 불량이면 시간조건을 기다리지 않는다. 이 예는 설계의 기대값이며 실제 시험 결과가 아니다.

CIS ENV_STALE_MS5000은 참고 코드의 로컬 값 유효성 검사 후보이다. 진단 전용 정책으로 확정하지 않았고 DOMAIN2500ms를 연장하지 않는다. UART50ms/CAN5ms 홉 여유는 기존 후보이며 실측 상한을 보증하지 않는다.

<a id="identity"></a>
## 5. 기동·전송 순서·원 관측·현재 품질

CAN TX_SEQUENCE는 송신 출처·수용한 BOOT·CAN ID/메시지별로 비교한다. UART LINK_SEQUENCE는 기존 방향별 순서다. 원 센서 UPDATE_SEQUENCE, Pi VISION_SEQUENCE/STATUS_SEQUENCE, DOMAIN 성공 평가 Sequence, CAN 합성 UPDATE_SEQUENCE와 CIS relay UPDATE_SEQUENCE는 생성 주체·갱신 사건이 다른 번호 공간이다. CIS relay 순번은 DOMAIN 원 성공 평가 번호가 아니며 전송 순서로도 바꾸어 쓰지 않는다. 수치가 우연히 같아도 동일성의 근거가 되지 않는다.

올바른 프레임을 수용한 뒤 메시지 수신 감시를 반영하고 원 관측 신규성을 판단한다. 이 연결안은 정상 반복에도 기존 CIS Vision Result/Status Callback을 호출한다. 같은 관측의 반복으로 원 AGE를 초기화하지 않는다. 중복 Callback을 생략하는 최적화는 기존 수신 감시 경로가 같은 사실을 유지한다는 증거가 있을 때만 한다.

obs42/VALID 뒤 새 CAN TX_SEQUENCE의 obs42/INVALID가 오면 원 관측은 그대로지만 현재 품질 저하는 반영한다. 원 관측 순번 중복만으로 폐기하지 않는다. 구 전송 프레임이나 단순 READY로 이전 VALID를 복구하지 않는다. 복원은 새 유효 관측과 기능 복구 규칙으로 한다. 원 AGE를 줄이지 않고 기존 C2 Rx→저장→Getter까지 검증한다.

Pi 원 INVALID 사유는 원 기록에 보존하되, 이후 실제 통신 단절·만료·기동 변경·STATUS 고장은 현재 사용 품질을 제한한다. Presence/Count 상호 모순은 같은 판정에서 두 필드가 각각 사용 가능한 경우에만 검사한다. Count255/INVALID 때문에 정상 Presence를 양수 인원수와 모순으로 판단하지 않는다. 한 필드 실패로 다른 정상 센서·필드를 일괄 무효화하지 않는다.

BOOT0 예약과 C2 sourceGeneration0은 같은 규칙이 아니다. 후보 C2의 유효 관측은 nonzero이며 알려진 원 세대는 무효 품질에서도 보존한다. 세대를 모르는0, 특히 STALE+sourceGeneration0의 허용/거부는 C2 후보에서 OPEN이다. 이를 임의 nonzero나 다른 품질로 치환하지 않는다. BCM 원 생산 세대와 CAN BOOT의 동일 lifecycle은 아직 확인되지 않았다.

같은 CIS BOOT에서 센서만 재초기화해 원 순번을 되감지 않는다. 수용한 새 BOOT의 낮은 순번은 이전 세대의 큰 순번과 단순 비교하지 않는다. 이미 종료한 Pi BOOT로 복귀하는 지연 A→B→A를 차단하는 규칙과 기억 범위는 검증 대상이다. 무작위32비트 BOOT를 보안 인증이나 영구 replay 차단으로 표현하지 않는다.

<a id="environment"></a>
## 6. 환경 C2와 후방 상태

환경은 온도·습도·조도별 품질·순번·AGE를 독립 보존한다. Network가 CIS BOOT 변경 또는 해당 수신 중단을 감지하면 기존 환경 입력 경계로 NO_DATA/STALE을 전달한다. 실제 함수는 DomainIf_RxCisCabinEnvironment다. 기존 VSM과 독립 Meta 저장·Getter에서 과거 정상값이 실제로 무효화되는지는 DOMAIN 연동 검증이 필요하다.

현재 C2 구조를 우선 사용한다. 필요한 무효화·세대 전이를 표현하거나 연결할 수 없는 경우에만 최소 계약 변경을 검토한다. sourceGeneration 필드 추가는 현재 계약에 포함되지 않는다. 실제 연결 근거가 필요하며 표현 한계에 따른 향후 최소 변경 가능성은 남아 있다.

후방 raw0.1cm는 mm와 같다. 65535를 실제 거리로 읽지 않고 측정 종류·품질로 해석한다. INACTIVE=5는 불허/확인 불가/만료 사유와 함께 사용하며 NO_OBJECT로 가짜 안전을 만들지 않는다. 재활성 뒤 새 측정 전에는 이전 거리를 정상으로 복원하지 않는다. R단·위험·CLEAR 판단은 DOMAIN 기능이다.

<a id="environment-delta"></a>
### 환경 BOOT 전환 저장 순서

**조건부 권장 설계 / DOMAIN 저장 경계 미확정**이다. `CisCabinEnvironmentState_t`에는 공개 sourceGeneration/CIS BOOT_ID가 없다. 기존 구조를 유지하려면 Network의 검증된 입력 순서가 DOMAIN의 실제 VSM 저장까지 유지됨을 확인해야 한다.

1. 유효한 새 CIS BOOT 전환을 확인하면 구 BOOT의 신규 전달을 막고 아직 실행하지 않은 패킷·콜백을 폐기한다. 처음 본 임의 BOOT를 무조건 최신으로 인정하지 않는다.
2. 이미 실행 중인 구 콜백과 그 콜백에서 시작된 DOMAIN 후속 비동기 쓰기가 완료되거나 더 이상 commit할 수 없는 상태임을 기존 owner 직렬 처리로 확인한다. 콜백 반환만으로 완료 판정하지 않으며 실행 중 작업을 취소했다고 가정하거나 owner를 동기 대기로 막아 교착시키지 않는다.
3. 구 BOOT 쓰기가 이후 끼어들 수 없어진 뒤 기존 `DomainIf_RxCisCabinEnvironment()`로 NO_DATA 무효화를 전달한다.
4. VSM의 이전 정상값·독립 Meta 무효화가 실제 저장 완료됐음을 확인한다. Queue 접수, 호출 시도 또는 반환값에 임의로 붙인 완료 의미로 대체하지 않는다.
5. 그 뒤 새 BOOT의 검증된 관측을 **원 Sequence 그대로**, 원 생성 기준점을 유지한 AGE로 전달한다. 대기 경과는 한 번 누적하고 온습도 AGE가2500ms에 도달하면 정상값으로 복원하지 않는다.
6. 기억된 종료 BOOT의 지연 프레임·대기 콜백이 다시 저장되지 않게 막는다. BOOT 재사용이나 Network 재시작으로 종료 이력을 잃는 경우까지 영구 차단을 보장하지 않는다.

구 BOOT A의900과 새 BOOT B의0은 Network가 `(원 CIS BOOT, 센서, 원 관측 Sequence)`로 구분한다. 900→901처럼 이어 붙이거나 새 전역번호로 리매핑하지 않는다. DOMAIN도 과거900을 전역 마지막 순번으로 비교하여 새0을 거절하는 대신 검증된 입력 순서를 저장까지 유지하는 연결을 수용해야 한다. **일반 NO_DATA는 재부팅 신호나 Sequence 리셋 명령이 아니다.** 실제 BOOT 전환 근거는 Network 내부에서 관리하며 NO_DATA 수신만으로 순번을 초기화하지 않는다.

무효화 저장 실패·완료 미확인에서는 새 정상 입력을 먼저 넘기지 않는다. 새 입력 차단과 과거 VSM 정상값의 무효화 성공은 별개이며, 이때 과거 OK가 이미 지워졌다고 기록하지 않는다. 기존 C2의 Core commit 이후 optional Observer 실패는 직접 저장 실패와 다르므로 commit을 중복 재실행하지 않는다. 실제 완료 판정과 후속 쓰기 순서는 [환경 Rx 계약](02_NODES/DOMAIN/20_API.md#delta-environment-completion)에서 정의하며 실제 DOMAIN 연동 검증이 필요하다.

DOMAIN이 자체 환경 세대를 알아야 하거나 다른 쓰기 경로·비동기 처리 때문에 위 저장 순서를 기존 연결로 증명할 수 없으면 해당 연결은 보류한다. 필요한 최소 Environment Generation/완료 경계 계약 변경을 검토하는 대안을 남기되 현재 공개 필드·API·Wire·헤더에는 반영되지 않았다. 일반 링크 단절의 STALE/NO_DATA 선택은 별도 미결이며 BOOT 전환의 NO_DATA 우선 제안으로 일괄 확정하지 않는다. 온습도2500ms와 조도1000ms 후보 지위도 유지한다.

<a id="failures"></a>
## 7. 접수 실패와 후발 실패

NET_SUBMIT_OK/DOMAIN_IF_OK는 필요한 데이터를 안정되게 복사·인계한 로컬 성공이지 실제 CAN/UART TX·상대 ECU 수용·차량 DONE이 아니다. 접수 전 INVALID_ARGUMENT/NOT_READY/NO_CAPACITY는 미접수 반환이며 그 항목의 후발 실패 사건을 만들지 않는다.

정상 접수된 필요한 명령·결과·일회성 이벤트는 driver 제출의 확정 최종 실패, 송신 전 만료, 송신 전 문맥 무효화가 기존 세 reason에 정확히 맞을 때만 DOMAIN LocalFailure에 원 식별자를 전달한다. 미분류 in-flight 오류·callback 부재를 세 reason으로 확장하지 않고 원 식별자·phase·진단과 영향 경로 HOLD를 남긴다. [실패 범위 정본](04_COMMON/20_API_USAGE_AND_FAILURE.md#r1-local-failure-reasons)을 따른다. 주기 상태·Query 실패는 각 통신/Query 경로를 사용하고 CAN 성공 Callback·CIS/Pi용 C2 함수를 추가하지 않는다. BCM_RESULT 수신/DONE만으로 CONFIRMED나 차량 FAILED/DONE을 합성하지 않는다. 실제 확인 근거와 binding은 검증 대기다.

<a id="scenarios"></a>
## 8. 노드 간 처리 흐름

다음 흐름은 양측 API와 Data의 연결을 설명한다. WINDOW 관련 흐름과 WINDOW 의존 가지는 구현 보류다.

| 흐름 | 메시지 연결 | 종료·복구 경계 | 연동 조건 |
|---|---|---|---|
| <a id="scenario-sc01"></a>수동 도어 요청·실행 결과·상태·음향 | [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [BCM_DOOR_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [BCM_DOOR_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_BCM_DOOR_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-door-state) · [VSS_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-vss-event) | 거부/실패 결과는 원 결과로 반환. 결과를 못 받으면 UNCONFIRMED이며 임의 FAILED/DONE 금지. 중복 전달은 새 구동/새 음향의 근거가 아님. 원 Result confirmation과 실제 전이 입증은 해당 결과 확인 조건. 유효한 새 세대/현재 상태를 확인하고 필요한 경우 조회. 이전 pending/명령 자동 재실행 없음. | 연동 검증 전 |
| <a id="scenario-sc02"></a>공조 설정·센서 입력·실제 출력 및 보호 | [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [CIS_ENVIRONMENT](03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) · [CIS_OCCUPANT](03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) · [BCM_CLIMATE_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [BCM_CLIMATE_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-state) · [BCM_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) · [M_USER_SETTINGS](03_COMMUNICATION/20_UART_M.md#message-m-user-settings) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_BCM_CLIMATE_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-climate-state) | 센서 일부 실패는 독립 정상값과 분리. 설정 DONE은 실내 목표온도 달성 증거가 아니다. 과열/불일치 등은 BCM 원 관측을 전달; local TX 실패로 ECU 고장을 만들지 않음. RECOVERY_CONFIRMED는 이전 목표의 자동 재구동 지시가 아니다. 새 유효 관측/세대 및 기능 Owner의 동작 판단 필요. | 연동 검증 전 |
| <a id="scenario-sc03"></a>실내 조명 설정·적용·표시 | [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [BCM_LIGHT_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [BCM_LIGHT_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-state) · [M_USER_SETTINGS](03_COMMUNICATION/20_UART_M.md#message-m-user-settings) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_BCM_LIGHT_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-light-state) | 적용 실패/미확인은 그대로 유지. APPLIED를 실제 점등 피드백으로 확대하지 않음. NORMAL 설정과 알림 출력은 구별; 부분 설정 존재 정보는 [표현·소비 조건](06_TECHNICAL_CONSTRAINTS.md#부분-usersettings-존재-정보)을 따른다. 현재 유효 적용 상태를 재확인하며 과거 명령을 자동 재실행하지 않음. | 연동 검증 전 |
| <a id="scenario-sc04"></a>디지털 키 문맥·자동 Unlock 상태/결과 | [M_CONTEXT](03_COMMUNICATION/20_UART_M.md#message-m-context) · [M_REQUEST](03_COMMUNICATION/20_UART_M.md#message-m-request) · [BCM_DOOR_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) · [BCM_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · [M_DIGITAL_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-digital-status) · [M_DIGITAL_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-digital-result) | 단순 연결을 ACTIVE/권한/근접으로 합성하지 않음. 무효 session/문맥/만료는 새 자동 동작을 차단한다는 문서 관찰; 실제 전이를 성공 결과만으로 확정하지 않음. 재접속 시 새 유효 문맥과 원 관측을 확인; 과거 접근/결과를 새 사건으로 재생하지 않음. | 연동 검증 전 |
| <a id="scenario-sc05"></a>환경 센서 원값·부분 품질 수집과 표시 | [CIS_ENVIRONMENT](03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [M_CIS_ENVIRONMENT](03_COMMUNICATION/20_UART_M.md#message-m-cis-environment) | 한 센서 실패로 독립 정상 센서값 전체를 무효화하지 않음. 원 AGE가 지난 값·반복 프레임을 fresh로 만들지 않음. 원 사용 한도는 [센서 시간 규칙](#timing)을 따른다. 실제 새 측정이 들어와야 원 최신성 회복. 새 프레임/READY만으로 이전 원값을 승격하지 않음. | 연동 검증 전 |
| <a id="scenario-sc06"></a>Pi 카메라 판정·CIS 경유 전달과 잔류 탑승자 경고 연결 | [P_VISION_RESULT](03_COMMUNICATION/30_UART_C.md#message-p-vision-result) · [P_VISION_STATUS](03_COMMUNICATION/30_UART_C.md#message-p-vision-status) · [CIS_OCCUPANT](03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [M_CIS_OCCUPANT](03_COMMUNICATION/20_UART_M.md#message-m-cis-occupant) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 카메라 무응답/INVALID를 0명·없음으로 만들지 않음. CIS 일반 센서 정상 경로는 독립 유지. 원 Pi 품질과 CIS 제한의 단일 이유 표현은 [품질 규칙](02_NODES/CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)을 따른다. 잔류 탑승자 위험의 미확인 품질을 CLEAR로 바꾸지 않으며 미표현 의미는 [C2 enum 투영 제약](06_TECHNICAL_CONSTRAINTS.md#c2-enumwire-의미-보존) 확인 필요. Pi/runtime/process 세대 변경 후 실제 새 원 관측 필요. 이전 record를 새 Boot로 다시 stamp하지 않음. | 연동 검증 전 |
| <a id="scenario-sc07"></a>후방 관측→DOMAIN 위험 판단→음향·표시 | [CIS_REAR](03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_CIS_REAR](03_COMMUNICATION/20_UART_M.md#message-m-cis-rear) | NO_OBJECT, UNAVAILABLE, INACTIVE를 구별. 무효/만료를 정상 CLEAR로 바꾸지 않음. 원 활성 정보의 합성은 [합성 조건 미확정](06_TECHNICAL_CONSTRAINTS.md)이며 후방 활성 Permission 원 500ms와 분리한다. 새 유효 관측/활성 의미의 현재 경고로 재중재. 이전 음향 사건 재생 금지의 좁은 정책은 [VSS 경고 처리](#scenario-sc13). | 연동 검증 전 |
| <a id="scenario-sc08"></a>원 Permission fan-out과 Pi 사용 허용 경유 | [DOMAIN_PERMISSION](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [P_PERMISSION](03_COMMUNICATION/30_UART_C.md#message-p-permission) | 독립 C2 입력 합성·누적 AGE·허용 생성원은 미확정이다. 만료/NO_DATA/기동변경은 새 허용으로 합성하지 않음. 전달 후보가 원 기한을 연장하거나 연속 허용을 보장한다는 주장 금지. 새 유효 DOMAIN 판단·수용 세대가 확인된 뒤 사용 여부를 기능 Owner가 판단. 이전 기동의 허용 복원 금지. | 연동 검증 전 |
| <a id="scenario-sc09"></a>BCM 보호 발생·현재 진단·가용성 | [BCM_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) · [BCM_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) · [M_BCM_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-bcm-status) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 통신 단절을 허구의 ECU FAILED/실제 고장으로 바꾸지 않음. current fault와 과거 발생 기록을 구별. bit/helper 대응은 [Fault 투영 제약](06_TECHNICAL_CONSTRAINTS.md#fault-bithelper-projection)에 따라 미확정이다. BCM 실제 복구 근거 전달. 복구 확인이 이전 동작의 자동 재구동 허가를 뜻하지 않음. | 연동 검증 전 |
| <a id="scenario-sc10"></a>CIS·Pi 일부 고장 및 통신 Health 전달 | [P_VISION_STATUS](03_COMMUNICATION/30_UART_C.md#message-p-vision-status) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [M_CIS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-cis-status) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 부분 비전·센서 실패와 건강한 경로를 분리. 새 CIS_FAULT 프레임을 발명하지 않음. 실제 Network health→진단 입력 연결 미확인. 실제 새 원 관측 및 기능 복구로 해당 부분을 재평가. 최근 고장 이력은 현재 정상과 별개. | 연동 검증 전 |
| <a id="scenario-sc11"></a>보류 WINDOW 이동·STOP·실제 결과·상태 | [DOMAIN_PERMISSION](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [WINDOW_COMMAND](03_COMMUNICATION/10_CAN_FD.md#message-window-command) · [WINDOW_STATE](03_COMMUNICATION/10_CAN_FD.md#message-window-state) · [WINDOW_RESULT](03_COMMUNICATION/10_CAN_FD.md#message-window-result) · [M_WINDOW_STATE](03_COMMUNICATION/20_UART_M.md#message-m-window-state) | 허용 상실 후 새 이동 제한과 적법 STOP의 조건을 구분. local AntiPinch/스위치가 상위 목표보다 우선. 수신 성공/timeout을 실제 정지 확인으로 만들지 않음. WINDOW/DOMAIN의 별도 재개·실물 구성·보호/결과/표시 근거 확인 필요. 현재는 DEFERRED, 과거 이동 재생하지 않음. | WINDOW 구현 보류 |
| <a id="scenario-sc12"></a>보류 WINDOW 끝단·끼임·Fault와 경고 | [WINDOW_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-window-event) · [WINDOW_STATE](03_COMMUNICATION/10_CAN_FD.md#message-window-state) · [WINDOW_FAULT](03_COMMUNICATION/10_CAN_FD.md#message-window-fault) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_WINDOW_FAULT](03_COMMUNICATION/20_UART_M.md#message-m-window-fault) | END를 AntiPinch로 바꾸지 않음. enum·수신 여부만으로 movementStopped/outputDisabled를 true로 합성하지 않음. 별도 WINDOW 재개 확인 후 다룰 보류 경로. 현재 구현 활성·통합시험 완료로 승격하지 않음. | WINDOW 구현 보류 |
| <a id="scenario-sc13"></a>VSS 일회 Event·지속 경고·복구 경계 | [VSS_EVENT](03_COMMUNICATION/10_CAN_FD.md#message-vss-event) · [VSS_WARNING_STATE](03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) · [VSS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-vss-status) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | 만료/무효 입력은 새 음향 근거가 아님. 기존 지속 경고도 유효 조건까지만 처리; 오디오 정지를 위험 해제로 바꾸지 않음. WINDOW 유래 경고 입력은 DEFERRED. [상위 설계의 VSS 복구 정책](../network_design_draft_v0.1.md): 이전 pending/interrupted 일회 음향은 자동 재개하지 않고, 복구 후 새 유효 현재 경고로 재중재. 실제 HW/시간은 NOT_VERIFIED. | 연동 검증 전 |
| <a id="scenario-sc14"></a>VSS 현재/최근 고장과 App 가용성 | [VSS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-vss-status) · [M_VSS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-vss-status) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) | current fault·LAST_FAULT·link health·원 품질은 동치가 아님. 미확인 status를 READY/FULL/정상으로 합성하지 않음. 실제 fresh status와 기능 복구 근거를 확인하며 최근 이력은 유지 가능. 원 세대/품질의 축약 동등성은 미결. | 연동 검증 전 |
| <a id="scenario-sc15"></a>기존 상태·결과·경고 Query와 부분 응답·종결 | [M_QUERY](03_COMMUNICATION/20_UART_M.md#message-m-query) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [M_DIGITAL_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-digital-result) · [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_USER_SETTINGS](03_COMMUNICATION/20_UART_M.md#message-m-user-settings) · [M_AVAILABILITY](03_COMMUNICATION/20_UART_M.md#message-m-availability) · [M_DIGITAL_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-digital-status) · [M_BCM_DOOR_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-door-state) · [M_BCM_CLIMATE_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-climate-state) · [M_BCM_LIGHT_STATE](03_COMMUNICATION/20_UART_M.md#message-m-bcm-light-state) · [M_BCM_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-bcm-status) · [M_CIS_ENVIRONMENT](03_COMMUNICATION/20_UART_M.md#message-m-cis-environment) · [M_CIS_OCCUPANT](03_COMMUNICATION/20_UART_M.md#message-m-cis-occupant) · [M_CIS_REAR](03_COMMUNICATION/20_UART_M.md#message-m-cis-rear) · [M_CIS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-cis-status) · [M_VSS_STATUS](03_COMMUNICATION/20_UART_M.md#message-m-vss-status) · [M_WINDOW_STATE](03_COMMUNICATION/20_UART_M.md#message-m-window-state) · [M_WINDOW_FAULT](03_COMMUNICATION/20_UART_M.md#message-m-window-fault) · [M_QUERY_END](03_COMMUNICATION/20_UART_M.md#message-m-query-end) | 원 데이터 부재, 존재하나 INVALID, 송신 실패/부분 누락을 구별. local queue OK는 actual TX count/완료가 아님. 원 CAN↔C2 연결 미증명 항목·부분 설정은 영향 projection HOLD. 재연결은 새 현재 문맥으로 보관 값/결과를 재조회; Query로 차량 동작을 재실행하거나 잃은 ECU 결과를 만들어내지 않음. | 연동 검증 전 |
| <a id="scenario-sc16"></a>경고 읽음 및 사용자 이탈 후 열린 도어 | [M_WARNING](03_COMMUNICATION/20_UART_M.md#message-m-warning) · [M_WARNING_ACK](03_COMMUNICATION/20_UART_M.md#message-m-warning-ack) · [BCM_DOOR_STATE](03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) | App 링크 상실을 사용자 이탈로 해석하지 않음. 읽음은 위험 CLEAR/음향 정지 지시가 아님. 근거 없음/만료를 정상 해제로 만들지 않음. 새 유효 상태/현재 경고를 조회·표시. 기존 occurrence와 새로운 발생을 구별하며 stale ACK로 새 경고를 읽음 처리하지 않음. | 연동 검증 전 |
| <a id="scenario-sc17"></a>부팅·세대 변경·단절/재접속·결과 미확인 | [M_CONTEXT](03_COMMUNICATION/20_UART_M.md#message-m-context) · [M_DOMAIN_ALIVE](03_COMMUNICATION/20_UART_M.md#message-m-domain-alive) · [M_QUERY](03_COMMUNICATION/20_UART_M.md#message-m-query) · [M_QUERY_END](03_COMMUNICATION/20_UART_M.md#message-m-query-end) · [M_RESULT](03_COMMUNICATION/20_UART_M.md#message-m-result) · [DOMAIN_PERMISSION](03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · [BCM_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) · [CIS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · [VSS_STATUS](03_COMMUNICATION/10_CAN_FD.md#message-vss-status) | 이전 세대 pending/cache/결과 상관을 새 정상 근거로 사용하지 않음. BLE/UART loss로 새 요청/자동해제 제한; 없어지는 결과를 FAILED/DONE으로 합성하지 않음. CAN bus-off 후 현재 상태 기반 전달 회복, 과거 명령/일회 이벤트 재생 없음. 모든 노드 일괄 reset이나 손실 결과 복구 보장은 문서 근거 없음. 실제 관측/무효화 binding 미검증. | 연동 검증 전 |
| <a id="scenario-sc18"></a>기존 전송 경계의 거부·부분 수신·용량·후발 실패 | [전체 기존 메시지 경계](05_MESSAGE_INDEX.md) · [접수·실패](04_COMMON/20_API_USAGE_AND_FAILURE.md) · [CAN](03_COMMUNICATION/10_CAN_FD.md) · [UART-M](03_COMMUNICATION/20_UART_M.md) · [UART-C](03_COMMUNICATION/30_UART_C.md) | 부분/CRC·길이 불일치 프레임은 정상 수신/원 최신성을 갱신하지 않음. 용량 부족·순서/중복 오류를 성공으로 꾸미지 않고 discrete 결과를 조용히 덮어쓰지 않음. BLE 병목/partial종결의 실제 바인딩은 미확인. 다음 정상 프레임으로 수신 경계를 회복하되 원 ID/세대/AGE를 재생성하지 않음. 기존 문서가 허용한 원 수명 내 재제출 경계만 기록; 새로운 retry 횟수/주기/메시지 없음. | 연동 검증 전 |

<a id="test-plans"></a>
<a id="delta-acceptance-cases"></a>
## 9. 경계 입력과 처리 조건

아래는 구현 시 확인할 입력·결과 조건이다. 권장 프로필의 적용 조건과 실제 연동 검증은 별도로 충족해야 한다.

| 입력·경계 | 처리 조건 |
|---|---|
| <a id="plan-ncp-t01"></a>UART-C 0x60/61/62 byte·LEN·CRC·TYPE/방향 | 19/16/15+overhead10, reserve/errors drop |
| <a id="plan-ncp-t02"></a>UART stream split/concat/CRC resync | misformed frame does not refresh active sensor |
| <a id="plan-ncp-t03"></a>Pi new vision result vs STATUS only | only original new result changes VisionSeq/AGE |
| <a id="plan-ncp-t04"></a>Pi BOOT A->B->delayed A | A rejected once retired; producer collision acknowledged |
| <a id="plan-ncp-t05"></a>random Boot collision or reboot without durable evidence | never promise absolute stale replay exclusion |
| <a id="plan-ncp-t06"></a>Pi VALID/INVALID per field | separate quality preserved; no UNKNOWN->0 |
| <a id="plan-ncp-t07"></a>Pi invalid followed by link loss | current COMMUNICATION_FAULT/STALE, origin Pi reason private |
| <a id="plan-ncp-t08"></a>Pi STATUS fault affects subset | only indicated affected function invalidated |
| <a id="plan-ncp-t09"></a>Pi Permission source age 499/500/501 | validity at <500 only if same accepted source |
| <a id="plan-ncp-t10"></a>CAN50 repetitive vs new reevaluation200 | no fake seq/age0 on periodic transmit |
| <a id="plan-ncp-t11"></a>UART relay new input vs 500 periodic | immediate on actual new valid eval, repeated retains revision |
| <a id="plan-ncp-t12"></a>Domain/CIS restart Permission | old ALLOW cannot be reanimated by stale Frame |
| <a id="plan-ncp-t13"></a>Power source valid but operation source UNKNOWN | power not fabricated/blanket DENIED; shared AGE conservative |
| <a id="plan-ncp-t14"></a>temp/humidity source 2499/2500/2501 | 2500 and beyond unusable at Domain consumer |
| <a id="plan-ncp-t15"></a>sensor local stale 4999/5000/5001 | local expiry boundary explicit, separate Domain2500 |
| <a id="plan-ncp-t16"></a>CIS Boot environment reboot and old delayed frame | VSM previous good invalidates; accept new Boot seq reset |
| <a id="plan-ncp-t17"></a>Rear ALLOW DENIED UNKNOWN NO_DATA | INACTIVE with distinct reasons, no NO_OBJECT on fault |
| <a id="plan-ncp-t18"></a>CAN Rear raw mm / C2 mm and 65535 cases | distance unchanged; no absent/CLEAR confusion |
| <a id="plan-ncp-t19"></a>CIS_STATUS Health vs fault history | link evidence separated from genuine sensor fault |
| <a id="plan-ncp-t20"></a>Local Publish OK vs tx delivery and queue failures | bounded copy not DONE/peer RX; error preserved |
| <a id="plan-ncp-t21"></a>S32K144+Pi UART concurrent sensor blocking | no loss/permission deadline violations measured |
| <a id="plan-ncp-t22"></a>S32K144↔344 CAN with Gen/Power | real FDF/BRS/DLC/Boot/Codec and C2 observer |
| ALLOW/OK, DENY/OK, UNKNOWN/NO_DATA, UNKNOWN/OK | 확정 DENY와 UNKNOWN 구분. UNKNOWN+OK 무손실 필요 시 기존 C2 한계·DOMAIN 기능 정책 미확정 |
| quality OK / meta.quality INVALID | OK를 골라 허용 생성 금지; 기존 충돌 후보대로 제한 |
| 성공17 ALLOW/OK 뒤200ms 평가 실패 | 실패 즉시 무효 전달 요청 권장. 원 성공17 유지·품질 하향 누락 금지; DOMAIN 기능 정책 미확정 |
| 마지막 성공 반복 뒤 실패 / 성공 이력 없음 | 실패 시각으로 AGE0·성공 순번 생성 금지; 미확인 시간 표현 결정 대기 |
| 평가가 정상 수행되어 DENY 결정 | false/OK는 확정 불허이며 평가 실패가 아님 |
| 동일 원 평가 반복, 품질 변경, 새 성공 평가 | 원 평가/합성/프레임 번호의 생성 의미 분리, 반복 TX로 원 평가 재발급 금지 |
| A900 대기 중 B0 수용 | A대기 폐기→구 쓰기 종료→NO_DATA 저장 확인→B 원0. 리매핑 금지 |
| A 콜백 실행 중 B 수용 | A 쓰기가 무효화 뒤 끼어들지 않음; 임의 취소·owner 교착 없이 직렬 경계 확인 |
| 콜백 반환 후 A 비동기 쓰기 대기 | 반환만으로 완료 판정 금지; 실제 후속 저장 종료 전 새 정상 보류 |
| NO_DATA 저장 거부·완료 미확인 | 새 정상 차단. 과거 OK가 이미 지워졌다고 판정 금지 |
| Core commit 뒤 optional Observer 실패 | 저장 실패와 구분·중복 commit 금지; 실제 완료 근거 확인 |
| B 저장 뒤 A 지연 프레임·대기 콜백 | 기억된 종료 BOOT 범위에서 A 재반영 차단 |
| 같은 BOOT의 일반 NO_DATA | 재부팅 추론·순번 리셋 금지; 단절 STALE/NO_DATA 선택 미결 |
| B 관측이 저장 대기 중 AGE2500 도달 | 온습도 원2500ms 한도 적용, 정상 복원 금지 |
| BOOT 재사용·Network 재시작·기억 상실 | 무조건 최신/영구 차단 보장 금지; 재동기화·안전 사용 근거 확인 대기 |
