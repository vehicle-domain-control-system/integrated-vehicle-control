# M02 Query 제어: 실행되는 제품 구현

`net_upper_query.c`는 private `NetUpperQuery` API로 F04/F05를 구현한다. `network_product`에 링크되므로 이 Module은 더 이상 DESIGN_ONLY가 아니다.

`net_upper_query_receive` / `net_upper_query_begin`은 실제 UART 요청과 문맥 출처를 검사한다. `net_upper_query_step`은 고른 Owner getter 칸을 부르고, 투영한 응답에 원 CAN receipt를 유지하고, 서로 독립된 로컬 TX 사실을 추적하고, 성공한 최종 TX만 세고, Query End를 내보내며, 문맥·허가·링크가 바뀌면 취소한다. 용량과 콜백 재진입에는 상한이 있다.

`NetUpperQueryOwner`는 명시적인 문맥 검사, plan 선택, wire 형식 17개를 만드는 getter 칸 18개를 준다. `net_domain_c2_query_owner`는 함수 본체가 있는 경우에 실제 고정 C2 signature를 묶는다. DOMAIN 노드 조립 루트(`nodes/domain/src/node/net_node_domain.c`)가 이 Owner로 Query 제어기를 초기화한다(`net_upper_query_init`). 지금 DOMAIN 보드 hook은 `select_query_plan`·`confirm_snapshot`을 비워 둔다. 그래서 보드에서는 plan 단계에서 Query 요청이 HOLD되고, CAN 투영 응답도 HOLD된다. Owner 함수가 없거나 plan/commit 정책이 풀리지 않으면 해당 요청만 HOLD한다. Owner getter가 값을 돌려줬다고 Network의 최신 RX가 Core에 commit됐다는 증거는 아니다. 투영하려면 값/식별 일치와 명시적인 commit 대조가 필요하다.

`product.query_*` 회귀 시험은 모든 getter 칸, 결과 종류, 용량/넘침, 문맥 취소, 최종 TX, End를 다룬다. C2, DOMAIN 노드, 가상 ECU 시험이 제품 binding을 실행한다. 정확한 실행 상태는 결과 패키지에 있다. 일반 WINDOW Query는 HOLD 그대로이고, 지원하지 않는 구동기/상태 API를 지어내지 않는다. 로컬 최종 TX 수는 BLE/App 수신이나 실행을 뜻하지 않는다.
