# UART-M Query·원본 투영·Private 감독 흐름

[M02](../modules/M02_QUERY_CONTROL.md) · [조회 함수 전문](../functions/UART_M_QUERY_CONTROL.md) · [상향](UART_M_UPLINK.md) · [하향과 원 CAN 투영](UART_M_DOWNLINK.md)

Query와 원본 투영은 새 차량 상태를 생산하지 않는다. 앞선 M_CONTEXT의 현재 문맥, DOMAIN의 기존 Getter 사본, 같은 원 CAN 입력을 보존한 Network 근거를 연결하며 실제 연결이 없으면 그 항목을 미제공/부분 상태로 남긴다. CF01·IF-O25·IF-O24의 조건부 상세이고 현재 정상 실행 채택은 확인되지 않았다.

| 단계 | 소유자·입출력 | 실패 분기 |
|---|---|---|
| M_QUERY 수신 | ESP32→DOMAIN, 현재 Device/Session·QueryID/scope·별도 과거 Request 참조 | nonzero QueryID·RESERVED0·CURSOR0·scope0 참조0·profile 검증 전 생산 없음 |
| 현재 문맥 검증 | M02→DomainIf_ValidatePassiveQueryContext, 선행 M_CONTEXT와 같은 직렬 처리 문맥 | SESSION_READY0 자체로 거부하지 않음. 인증·peer·현재 Session 불명은 HOLD; Session 발급 안 함 |
| 범위/권한 확인 | DOMAIN Owner의 실제 목록·접근권한·필터, M02의 호출 계획 | scope1/2/3 참조 조합·목록 미정이면 자동 단건/전체조회로 바꾸지 않음 |
| Getter18 | DOMAIN 본체가 호출자 OUT 사본 제공. Recent만 capacity=min(실제 배열 원소 수,5), Device별 보관 최대8에서 최신순 선정 | 실패 OUT/HasRecord/Count 미소비. count>capacity 거부·절삭 금지. 상태/경고/단건 Query는5 제한 대상 아님 |
| 원본·표현 확인 | Network의 11 CAN projection association·17 schema 변환 | 값 동등성만으로 원 CAN 연결 금지; O21/O22 표현 손실·WINDOW2 HOLD 유지 |
| 응답 사본 제출 | 기존 DomainNetPriv_SubmitMobile*; recipient 현재 Session, 원 Request Session 별개 | local OK 전에 전체 사본. 실패는 새 항목 부분 접수 없음, 기존 discrete 덮기 없음 |
| 응답 Driver 관측 | M07/M08/M02가 각 기존 응답의 최종 UART 결과/참조 해제·수명을 연결 | enqueue/accepted/release만으로 count 증가 안 함. 구 callback·미확정 결과는 종료 보류 |
| M_QUERY_END | 모든 결과/송신 순서 입증 뒤 기존 status0..3·확인된 final TX N | End·자발QUERY_ID0·중복 제외. 결과 불명·기록 접근 실패를 빈 성공으로 만들지 않음 |
| ESP32 종료 관측 | EspNetPriv_OnQueryEnd, 같은 활성 문맥·scope·Boot·실제 유효 수신과 대조 | count 불일치·손실·늦은 End·Boot/Session 교체 시 App 완료·Query 재개 없음 |

CAN 투영은 DOWN_CONTEXT20B+기존 원 CAN used layout을 사용하며 padding을 복사하지 않는다. 원 관측 값·식별과 검증된 현재 제한의 [Core/Getter 연결](../functions/DOMAIN_QUERY_PROVENANCE.md#association)을 함께 보존하고 원 raw GOOD로 현재 품질을 되돌리지 않는다. 현재 DOMAIN Boot는 원 CAN Boot/TX·Pi Boot/VisionSequence를 대체하지 않는다. 동일 원 NetDTO/NetCanRxMeta→C2의 실제 반영→Getter/TxPort 연결 근거가 없으면 해당 투영만 보류하며 unrelated 보고를 일괄 차단하지 않는다. `M_CIS_REAR`의 1 raw=1mm를 정수 cm로 절삭하지 않는다. 새 AGE0·새 원 세대·새 순번을 만들어 Query 데이터를 최신화하지 않는다.

최근 결과로 선정한5개를 전부 실제 final TX한 경우에만 현재 문맥·순서·다른 누락을 확인해 DONE/count5 후보를 만든다. 내부8개 중 미선정3개는 실패가 아니다. 반면 생산 범위가8개인 Query에서5 final TX+3 미확정이면 recent 규칙으로 분모를 줄일 수 없고 End를 보류한다. recent5 중4 final TX+1 미확정도 보류하며, 나머지1이 확정 실패가 되면 기존 PARTIAL_UNCONFIRMED/count4 조건을 확인한다.

M_QUERY_END 자체는 count에 포함하지 않는다. 응답10개가 local enqueue되었어도8 final TX+2 미확정이면 종료를 기다린다.8 final TX+2 확정 실패가 동일 Query/순서로 입증되면 status2/count8 후보다. scope의 실제 빈 집합은 조건부 status0/count0이나 Getter 실패0건은 해당되지 않는다. 이전 Session의 End를 받으면 현재 Query로 귀속하거나 QUERY_ID0 자발 갱신으로 바꾸지 않는다.

M_DOMAIN_ALIVE는 DOMAIN Network→ESP32 private 생존 평가이며 Query 응답 목록·ITEM_COUNT·Gateway SendToMobile에 포함되지 않는다. source Boot/update/AGE/quality를 보존하고 정상 보고가 CAN ECU/DOMAIN Manager/BLE/App/차량 기능 정상이라는 증거가 되지 않는다.250/750ms는 후보이며 Session TTL·Query deadline이 아니다.

| 전형적 반례 | 보존/제한 |
|---|---|
| snapshot 값은 같지만 다른 원 CAN Sequence | 해당 projection association HOLD, 최신 raw와 임의 결합 없음 |
| Getter가 UNKNOWN 품질을 성공 반환 | 해당 Wire 표현 가능성 확인, 없으면 부분; 정상0으로 채우지 않음 |
| Query 진행 중 Session/DOMAIN Boot 변경 | 구 Query 완료 자격 분리. 진행 Driver 참조는 release 증거 전 유지 |
| Window state/fault 요청 | 2 Wire/2 Rx 문서 추적만; 표시/timeout/liveness 미활성 |
| final TX 후 ESP32 유효 수신 손실·BLE 실패 | local TX와 App 목록 완료를 분리, 원 부분 자료·AGE 보존 |

[Query 입력](../functions/uart_m/M_QUERY.md) · [Getter18 정본](../functions/UART_M_QUERY_CONTROL.md#getter18) · [종료 상세](../functions/uart_m/M_QUERY_END.md) · [Private ALIVE](../functions/uart_m/M_DOMAIN_ALIVE.md) · [공통 TX/RX](../functions/UART_M_TX_RX_CONTROL.md) · [Data/API](../data/UART_M_DATA.md)

역사적 설계 근거: S4C `TIME_IDENTITY_CONTRACT.md#query/#projection`, `FAILURE_RECOVERY_CONTRACT.md#query-recovery`, B08 `UART_M_ROUTING.md#query/#provenance`, 원 S4A Getter/submit/private 선언. G6 당시 Interface39·B12 기술 정본은 역사적 봉인 자료로 보존하고, 현재 별도 후보의 문서 참조는 고정 r1 Interface40개로 구분한다. 기존 S6 후 협의 반영 큐의 원 ID·이력과 남은 미결은 유지하며 이번 문구 보완으로 종결하지 않는다.
