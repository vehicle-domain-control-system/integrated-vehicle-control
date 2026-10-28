# DOMAIN 노드 Network 사본 안내

DOMAIN 노드(profile 5, CAN + UART-M)용 Network 사본의 안내다. 사본은 `develop`의 `firmware/network_nodes/DOMAIN/network/`에 둔다. Network가 관리하는 폴더이고 DOMAIN 폴더(`firmware/DOMAIN/`)는 건드리지 않는다. 원본은 NET_git `firmware/network/nodes/domain/DOMAIN_NODE_INTEGRATION.md`이고, 사본에는 `README.md`로 들어간다. 설계 근거는 `docs/network/interface/02_NODES/DOMAIN/{10_DATA,20_API,25_PRIVATE_ROUTING}.md`, `03_COMMUNICATION/10_CAN_FD.md`, `03_COMMUNICATION/20_UART_M.md`이다.

## 이 폴더는 생성 사본이다

- **원본:** NET_git의 `firmware/network`. 이 폴더의 파일은 `firmware/network/tools/sync_to_s32ds.py --node DOMAIN`이 원본의 상대 경로로 복사한 것이다(예: `src/l04_frame_codec/net_wire_common.c`, `include/network/net_types.h`). DOMAIN만 쓰는 파일은 원본에서 `nodes/domain/` 아래에 있고, 사본에는 `nodes/domain/`을 뺀 경로로 들어간다(예: 원본 `nodes/domain/src/node/net_node_domain.c` → 사본 `src/node/net_node_domain.c`). 이 문서의 파일 경로는 사본 기준이다.
- **직접 고치지 않는다.** 고칠 것이 있으면 원본을 고치고 커밋한 뒤 다시 만든다. NET_git 루트에서:
  ```
  python -B firmware/network/tools/sync_to_s32ds.py --node DOMAIN --out <develop 체크아웃>/firmware/network_nodes/DOMAIN/network
  python -B firmware/network/tools/sync_to_s32ds.py --node DOMAIN --out <develop 체크아웃>/firmware/network_nodes/DOMAIN/network --apply
  python -B firmware/network/tools/sync_to_s32ds.py --node DOMAIN --check <develop 체크아웃>/firmware/network_nodes/DOMAIN/network
  ```
  - 첫 줄은 dry run이다. 새로 쓰고, 바꾸고, 지울 파일만 출력하고 아무것도 쓰지 않는다.
  - `--apply`는 다음 경우 아무것도 쓰지 않고 거부한다(종료 코드 1): 원본에 커밋되지 않은 변경이 있다, 폴더 이름이 `network`가 아니다, 이 폴더에 고친 파일이나 기록에 없는 파일이 있다, `NETWORK_BUNDLE.txt`가 없거나 다른 노드의 것이다.
  - 같은 원본으로 다시 실행하면 바뀌는 파일이 없다. 원본에서 빠진 파일은 지워진다.
- **위치:** `firmware/network_nodes/DOMAIN/network/`는 원본 `firmware/network`와 나란한 폴더라 원본이 `develop`에 합쳐져도 경로가 겹치지 않는다. 합친 뒤 `--check`가 `STALE`이면 같은 명령으로 다시 만든다.
- **`NETWORK_BUNDLE.txt`:** 생성 기록이다. 노드, 프로필(`NETWORK_UPPER_NODE_PROFILE=5`), 원본 브랜치·커밋(`source_commit`), 원본 상태(`source_state`, 커밋되지 않은 변경이 있었으면 `dirty`), 파일마다 SHA-256이 들어 있다. 해시는 CRLF를 LF로 읽은 내용의 값이라 Windows 체크아웃 설정(`core.autocrlf`)과 상관없이 같다. 시각은 넣지 않는다.
- **`--check` 결과(마지막 줄):**
  - `CHECK DOMAIN OK`(종료 0): 기록과 같고 지금 원본으로 만들어도 같다.
  - `CHECK DOMAIN EDITED`(종료 1): 기록이 없거나 형식이 틀리다, 파일이 고쳐졌거나 빠졌다, 기록에 없는 파일이 있다.
  - `CHECK DOMAIN STALE`(종료 3): 사본은 그대로지만 원본이 바뀌었다. 다시 만들어야 한다(달라진 경로를 출력한다).

## 들어 있는 것

- **공통 코덱·서비스:** `src/l01_node_context`, `src/l02_typed_handoff`, `src/l03_message_service`, `src/l04_frame_codec`, `src/l05_transport_port`의 공통 소스와 `include/network/**`(host 전용 `net_host_mock.h`, `net_virtual.h` 제외). CAN·UART 코덱, 수신 검증과 순번, M04 메시지 서비스(BOOT·TX_SEQUENCE·LINK_SEQUENCE, 반복 송신), 조회(Query), 관측·진단이다.
- **C2 어댑터:** `src/l02_typed_handoff/m03_typed_handoff/net_domain_c2.c`, `net_domain_rx.c`, `net_domain_tx.c`, `net_domain_uart.c`, `net_domain_query.c`, `net_domain_owner_binding.c`, 공개 헤더 `include/network/net_domain_c2.h`, 비공개 헤더 `net_domain_private.h`, `net_domain_enum_internal.h`. DOMAIN C2 경계(`Domain_Interface_v0.5_C2_target.h`)와 Network 사이를 잇는다. 정해지지 않은 의미 규칙은 `NetDomainC2Config`의 콜백으로 받고, 콜백이 없으면 그 경로만 HOLD한다(추측한 OK를 만들지 않는다).
- **RTD 백엔드:** `src/platform/rtd/net_hal_rtd.c`(시간원, 시작 순서, ISR 링), `net_hal_rtd_can.c`(FlexCAN), `net_hal_rtd_uart.c`(LPUART, DOMAIN 프로필에서는 UART-M 링크만), 보드 설정 `net_board_cfg.h`.
- **조립 루트와 공개 진입:** `src/node/net_node_domain.c`(DOMAIN 조립 루트. C2 어댑터가 Owner이고, M04 주기 송신·Query·RTD 백엔드를 묶는다), `src/node/net_entry.c`와 `include/network/app/net_if_entry.h`(`Net_Init`, `Net_MainFunction`, `Net_TxFlush`, `Net_GetBootId`), `include/network/net_node.h`(`NetNodeDiag`).
- **보드 파일:** `board/s32k344_domain/net_domain_board.c`, `net_domain_board.h`. `Net_Init`의 노드 설정 hook, C2 hook, 시간 변환 hook, DOMAIN이 넘겨주는 CIS 전환 binding을 받는 `NetDomainBoard_SetTransitionBinding`이다. DOMAIN C2 공개 헤더만 쓴다.
- **들어 있지 않은 것:** 다른 노드 전용 파일(VSS·CIS·BCM App facade, 노드 프로필 표, 공통 조립 루트 `src/node/net_node.c`), host 전용 파일, mock SDK, 시험, 도구.

## DOMAIN 조립 루트 (host 시험까지)

- **순서:** DOMAIN이 `DomainIf_Init`을 먼저 하고 `Net_Init()`을 부른다. `Net_Init`이 C2 TxPort를 등록한다(`DomainIf_SetTxPort`). DomainIf가 초기화되지 않았으면 등록하지 못하고 `NetNodeDiag.stage`가 `NET_NODE_STAGE_APP_PORT`(`NET_NOT_CONFIGURED`)에서 멈춘다. 이때 아무것도 보내지 않는다.
- **한 바퀴:** `main`이 DOMAIN의 메인 루프 전용 ms 함수(`uint32_t`)를 한 번 읽어 `Net_MainFunction(ms)`에 넘긴다. 그 안에서 수신 프레임이 C2 Rx 함수로 전달된다. DOMAIN 앱이 TxPort로 제출한 뒤 같은 ms로 `Net_TxFlush(ms)`를 부르면 그 바퀴에 나간다(선택). Network와 보드 파일은 DOMAIN 시간 함수를 직접 부르지 않는다.
- **보드 hook(프로젝트 define):** `NET_BOARD_NODE_CONFIG_HOOK=NetDomainBoard_NodeConfig`, `NET_BOARD_DOMAIN_C2_HOOK=NetDomainBoard_C2Config`, `NET_BOARD_ISR_TICK_HOOK=net_hal_rtd_last_now_ms32`, `NET_BOARD_TICK_TO_MS_HOOK=NetDomainBoard_TickToMs`. ISR은 캡처에 직전 바퀴의 ms를 붙인다. 그래서 대기 AGE는 실제보다 최대 한 바퀴 크게 잡힌다(오래된 쪽).
- **C2 hook:** `net_domain_c2_pinned_owner_api`(고정 DOMAIN 함수)와 잠정 규칙 세 개(`net_domain_c2_value_quality_provisional`, `net_domain_c2_permission_bundle_provisional`, `net_domain_c2_status_quality_provisional`)를 쓴다. 나머지 의미 콜백 6개는 NULL이라 그 경로는 HOLD다. BOOT_ID, TX_SEQUENCE 시작, M04 송신 칸, 명령 대상 BOOT, CIS 전환 장벽은 조립 루트가 정한다.
- **송신:** DOMAIN_PERMISSION(0x080)과 VSS_WARNING_STATE(0x0C0)는 앱이 새 평가나 의미·품질 변경 때 TxPort로 제출하면 Network가 50 ms마다 반복한다(같은 원 관측, TX_SEQUENCE +1, AGE 증가). 0x080은 원 Power 수명 500 ms에서 반복이 멈춘다. M_DOMAIN_ALIVE(UART-M 0x02)는 250 ms다. 값은 C2 설정의 `alive_state`에서 받고, 없으면 `INIT / NO_DATA / 0 / 0xFFFF`다. 명령의 TARGET_BOOT_ID는 마지막으로 수신한 BCM 프레임의 BOOT(BCM_STATUS 우선)이고, 아직 모르면 명령을 보내지 않는다(`DOMAIN_IF_NOT_AVAILABLE`). CAN TX_SEQUENCE는 메시지별로 1부터다.
- **수신:** 복합 관측·상태(0x300, 0x230, 0x320, 0x340)와 BCM 도어·공조·조명 상태(0x200, 0x210, 0x220)는 필드별 AGE로 받는다. 신선도는 DOMAIN이 판단한다. BCM 상태는 바뀌지 않은 평가를 원 순번·원 AGE로 반복하므로 AGE가 2,000 ms를 넘어도 프레임은 계속 온다. 다른 경로는 전달 AGE 2,000 ms 상한이다. UART-M 조립 기한은 50 ms다.
- **CAN bus-off:** Network는 bus-off를 받은 바퀴에 CAN 링크를 닫고 TX MB에 남은 프레임을 실제 결과대로 정산한 뒤, 100 ms(다섯 번 넘게 이어지면 1 s) 지나 새 포트 epoch로 다시 연다(`VSS_NODE_INTEGRATION.md` "제한"). 닫힌 동안 CAN 송신 요청은 `DOMAIN_IF_NOT_AVAILABLE`이고, 받은 CAN 프레임은 C2로 가지 않고 버려진다. 다시 열린 뒤 M04 주기 송신(0x080, 0x0C0)은 같은 평가를 다음 TX_SEQUENCE로 이어 보낸다. bus-off 전에 보낸 명령은 다시 보내지 않는다. bus-off나 TX timeout으로 MB에서 abort된 명령은 장치가 받은 뒤의 실패("제출 뒤 미분류 in-flight 오류")라 로컬 실패 사유에 들지 않는다(`04_COMMON/20_API_USAGE_AND_FAILURE.md` "접수 후 실패의 닫힌 범위"). 그 규칙대로 DOMAIN에 알리지 않고, 그 C2 송신 기록은 식별을 가진 채 이 실행 동안 HOLD로 남는다(`NetDomainC2TxRecord.held`, 누적 수 `NetDomainC2.tx_held`). 그 기록의 M04 송신 칸만 회수하므로 DOMAIN_PERMISSION 등 주기 송신은 계속 나간다. 남은 기록은 C2 송신 기록 16칸을 차지해, 이런 실패가 16번 쌓이면 C2 송신(명령, M_RESULT 등, 상태 중계와 WINDOW 명령도 같은 16칸을 쓴다)은 리셋 전까지 `DOMAIN_IF_QUEUE_FULL`이다(`domain_node.n23_busoff_command_held`, 아래 HOLD 표 "의미 콜백 6개"). bus-off 때 MB에 들어가지 못하고 서비스 큐에서 기다리던 C2 기록(같은 CAN ID의 다음 명령 등)은 장치에 닿기 전 실패다. 이 기록은 로컬 실패 사유 콜백이 없어 C2 기록과 M04 송신 칸을 모두 차지한다(아래 HOLD 표). 그런 명령이 기다리는 중에 bus-off가 나면 그때마다 M04 칸 하나가 찬다. UART-M 링크는 닫히지 않는다. 다만 CAN 수신에서 나온 상태 중계는 CAN이 닫힌 동안 나가지 않는다(기다리던 중계도 HOLD로 끝난다).
- **C2 Rx가 OK가 아닐 때:** 그 경로(수신 기록)를 이 실행 동안 막는다. 예외는 BCM 명령 결과(0x140)와 보호 이벤트(0x141)에 DOMAIN이 `DOMAIN_IF_REJECTED`를 돌려준 경우다. 이 둘은 명령·이벤트마다 한 번 오는 메시지이고, DOMAIN은 기록이 없는 명령의 결과(ResultManager NOT_FOUND, 예: DOMAIN 리셋 전에 보낸 명령의 늦은 결과)를 REJECTED로 돌려준다. 그 프레임만 버리고 `NetDomainC2.rejected_frames`를 센다. 같은 프레임은 다시 넘기지 않고, 그 경로의 다음 결과·이벤트는 전달한다.
- **CIS 환경·Occupant(0x300, 0x310):** 장벽별로 막는다. 환경은 0x300의 CIS BOOT_ID, Occupant는 0x310의 `PI_BOOT_ID`(0이면 항상 막음)를 본다. 새 세대의 프레임은 C2로 보내지 않고 버린 뒤(카운트) 장벽을 PENDING으로 둔다. 다음 `net_domain_c2_step`(한 바퀴 안, 수신 전달 뒤, `Net_TxFlush`에서는 부르지 않음)이 전환 binding을 (세대, epoch) 한 번으로 부른다. epoch는 장벽별로 실행마다 1부터이고, 같은 세대·epoch에 DOMAIN barrier revision이 앞선 receipt의 APPLIED(또는 그 receipt의 DUPLICATE_APPLIED)일 때만 열리고 +1이다. NOT_INITIALIZED는 같은 epoch로 다음 바퀴에 다시 부른다. UNAVAILABLE(DOMAIN private context 없음, Occupant 비활성 등)은 DISABLED, 그 밖의 결과·receipt 불일치·epoch가 UINT64_MAX에 이를 때는 BLOCKED이고, 둘 다 이 실행 동안 다시 부르지 않고 그 경로만 막는다(자동 재시도·복구 없음). 떠난 세대 4개는 기억해 늦은 프레임을 계속 막는다. 상태는 `NetNodeDiag.barrier`(상태, 세대, 다음 epoch, 마지막 결과·receipt, 사유, 카운터)에 있다.
- **CIS 전환 binding:** `NetDomainC2TransitionBinding`(`include/network/net_domain_c2.h`: context, 환경 함수, Occupant 함수, 결과 enum과 receipt). 이 함수들은 DOMAIN private 전환(`DomainCisEnvironmentPriv_ApplyBootTransition`, `DomainCisOccupantPriv_ApplyBootTransition`)을 불러 결과를 1:1로 옮기고, private context가 없으면 DOMAIN을 부르지 않고 UNAVAILABLE을 돌려준다. DOMAIN 응용 헤더가 필요하므로 DOMAIN 쪽 통합 코드가 구현하고 `Net_Init()` 전에 `NetDomainBoard_SetTransitionBinding`으로 넘긴다. 넘기지 않으면 0x300·0x310은 계속 막힌다. 시험용 구현 예는 원본 `firmware/network/nodes/domain/tests/product/domain_transition_binding.c`(사본에는 없다)다.
- **아직 하지 않은 것:** 실제 RTD 헤더로의 컴파일과 링크, BOOT_ID 출처(지금은 보드 상수, HOLD), UART-M 인스턴스(자리표시값). host mock 시험(`domain_node.*`)까지 확인했다. 그래서 이 폴더는 아직 DOMAIN S32DS 빌드에 넣지 않는다. 사본은 DOMAIN 폴더 밖(`firmware/network_nodes/`)에 있으므로 보드에 적용하지 않은 Network 소스가 DOMAIN 빌드에 섞이지 않는다.

## 용량

값은 `include/network/net_capacity.h`의 DOMAIN 블록이 정한다(`NETWORK_UPPER_NODE_PROFILE=5`). 모두 `#ifndef`라 프로젝트 `-D`로 바꿀 수 있지만, 여러 파일이 함께 쓰는 구조체의 칸 수이므로 한 빌드의 모든 파일에 같은 값을 준다. 통신량 근거는 `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md`와 `20_UART_M.md`의 메시지 목록이다(WINDOW 제외). DOMAIN 보드에서 Network + RTD 백엔드에 주는 RAM 예산은 44 KB다. host ARM 측정(`-Os -fno-short-enums`, 정적 RAM)은 43,383 B다(보드 map은 아직 없다).

| 상수 | 값 | 기준 |
|---|---|---|
| `NET_UPPER_TX_CAPACITY` | 16 | M04 칸 3개 + C2 송신(명령, UART-M 상태 중계 200 ms 묶음 6개와 1 s 묶음 7개가 한 시점에 13개, 조회 응답). 장치 칸이 없는 프레임은 여기서 기다린다 |
| `NET_UPPER_PUBLISH_CAPACITY` | 3 | M04 표: DOMAIN_PERMISSION, VSS_WARNING_STATE, M_DOMAIN_ALIVE |
| `NET_UPPER_RX_CAPACITY` / `NET_UPPER_POLL_BUDGET` | 24 / 24 | UART-M 수신 프레임 최대 48 B(M_CONTEXT): step 2회 × 24로 한 바퀴에 한 프레임(CIS와 같은 논리). CAN 동시 수신 9 |
| `NET_HAL_RX_CAPACITY` / `NET_BOARD_ISR_DRAIN_BUDGET` | 16 / 16 | 공통 값 그대로 |
| `NET_HAL_TX_CAPACITY` | 4 | CAN TX MB 4개(MB 8..11), UART-M 송신 칸. UART 송신은 이 중 2칸까지만 쓴다 |
| `NET_BOARD_CAN_ISR_RING` | 24 | 받는 주기 프레임 67개/s(15개 경로), 1초마다 9개가 함께 시작. 동시 9개와 약 200 ms의 주기 통신량 |
| `NET_BOARD_UART_ISR_RING` | 256 | UART-M 바이트 링. 115200 bit/s 가득 찬 선로에서 약 22 ms |

- **송신 묶음:** 장치 칸(HAL TX 4)보다 많은 프레임이 한 바퀴에 몰리면 나머지는 서비스 큐에서 다음 바퀴를 기다린다(`upper.tx_deferred`). 같은 번호 공간의 뒤 프레임(UART는 같은 링크 전체, CAN은 같은 ID)은 앞 프레임을 앞지르지 않는다. CAN 컨트롤러에는 한 ID의 프레임을 한 번에 하나만 둔다(FlexCAN은 같은 ID를 MB 번호 순으로 보내므로, 같은 ID의 C2 명령 둘은 앞 것이 끝난 뒤 다음 것이 나간다. `domain_node.n21_same_id_order`, `n22_same_id_order_deferred`). 그래서 LINK_SEQUENCE와 TX_SEQUENCE는 뒤바뀌어 나가지 않는다. 예외는 abort가 끝나지 않아 격리된 MB다. 그 프레임은 이미 실패로 보고했고 MB가 계속 묶일 수 있어 같은 ID를 막지 않는다. 그 프레임이 나중에 나가면 앞 번호라 수신 쪽이 오래된 프레임으로 버린다. 기다리는 동안에도 AGE는 늘고, 사용 기한이 지나거나 링크가 닫히면 보내지 않는다. 이렇게 기다리던 프레임이 취소·만료되면 그 번호는 건너뛸 수 있다(건너뜀은 허용, 재사용은 없음). UART-M은 한 바퀴에 많아야 한 프레임 정도 나간다(`VSS_NODE_INTEGRATION.md` 보드 설정 절, UART 송신 속도와 AGE). 그래서 1초 묶음 13개가 다음 200 ms 묶음 전에 나가려면 main 주기가 약 15 ms(200 ms / 13)보다 짧아야 한다. 장치에 넘긴 뒤 앞 전송을 기다린 시간은 AGE에 들어가지 않는다(보통 앞 전송 하나(100 ms 이내)와 한 바퀴, abort를 기다리는 동안은 더 길다).
- **문서 통신량 부하(host):** `domain_node.load_doc_traffic`. 10 s 동안 CIS·BCM·VSS 주기 프레임(67개/s), ESP32의 M_CONTEXT(48 B, 100 ms), 앱 역할로 DOMAIN_PERMISSION 새 평가 200 ms마다, UART-M 상태 중계 수만큼의 프레임(200 ms마다 6개, 1 s마다 7개 더, 같은 시점)을 넣는다. 상태 중계는 CAN 원본 수신과 DOMAIN snapshot 확인(이 보드에서 HOLD)이 있어야 해서 같은 수·비슷한 길이(46 B)의 M_USER_SETTINGS로 대신한다. main은 10 ms 주기이고 ESP32 쪽은 바퀴마다 UART-M 프레임 하나를 받는다(115200 bit/s 선로보다 느리다). 손실 0, DOMAIN_PERMISSION 간격 ≤ 50 ms + 10 ms, LINK_SEQUENCE 건너뜀 0, 최대 깊이 ≤ 용량을 확인하고 `LOAD DOMAIN ...` 한 줄로 남긴다. host 결과: CAN 링 최대 10/24, 어댑터 수신 16/16, 서비스 수신 24/24, 서비스 송신 13/16, UART 링 48/256, DOMAIN_PERMISSION 간격 최대 50 ms. M_USER_SETTINGS가 제출부터 선로까지 기다린 시간은 최대 120 ms(13개 묶음의 마지막, 10 ms 주기에 한 바퀴 한 프레임)이고, wire AGE가 실제 나이보다 작은 정도는 최대 10 ms(한 바퀴)다(`load uart` 줄).
- **보드 확인 항목:** 보드 빌드 map의 RAM, `main_max_us`, 링 최대 깊이(용량의 75 % 이하), `rtd.rx_overruns` 0, `rtd.tx_abort_timeouts`·`rtd.tx_quarantined`(지금 격리된 TX MB 수, 4이면 그 MB의 플래그가 서거나 리셋될 때까지 CAN 송신이 멈춘다)·`rtd.rx_rearm_errors`·`rtd.rx_unarmed` 0, `rtd.uart[0].uart_tx_abort_errors`·`uart_tx_abort_waiting` 0(1이면 UART-M 송신이 멈춰 있다, `VSS_NODE_INTEGRATION.md` 보드 설정 절). main 주기가 약 15 ms보다 짧은지 본다(위 송신 묶음). DOMAIN 루프 주기에 맞춰 CAN 링과 UART 링 시간을 다시 본다.

## 나중에 빌드에 넣을 때 필요한 것

- **include 폴더 7개**(사본 기준. 원본 트리를 빌드하는 `firmware/network/CMakeLists.txt`의 `_node_includes`에는 노드 헤더 폴더 `nodes/<node>/include`와 `src/node`가 더 있다): `include`, `src/l04_frame_codec`, `src/l02_typed_handoff/m03_typed_handoff`, `src/l05_transport_port/m07_buffer_outcome`, `src/l05_transport_port/m09_observation`, `src/l03_message_service/m04_message_service`, `src/platform/rtd`. 보드 파일 `board/s32k344_domain/*.c`는 자기 폴더의 헤더를 같이 쓴다.
- **C2 헤더 폴더:** `firmware/DOMAIN/src/interface`(`Domain_Interface_v0.5_C2_target.h`, `Vehicle_Types_v0.2.h`)
- **define:** `NETWORK_UPPER_NODE_PROFILE=5`. 빠뜨리면 `include/network/net_upper_profile.h`가 컴파일 오류를 낸다(기본 프로필은 없다). MCU는 S32DS CPU define(`CPU_S32K344`)이나 `NETWORK_NODE_PLATFORM=344`로 정한다. 없으면 `src/node/net_node_domain.c`가 컴파일 오류를 낸다. 위 보드 hook 4개도 define으로 준다.
- **RTD 생성 설정 이름**(`src/platform/rtd/net_board_cfg.h`, 프로젝트 설정의 `-D`로 바꾼다):
  - FlexCAN 설정·상태 심볼과 ConfigTools 콜백 이름(`NET_BOARD_CAN_CONFIG`, `NET_BOARD_CAN_STATE`, `NET_BOARD_CAN_CALLBACK`, `NET_BOARD_CAN_ERROR_CALLBACK`), MB 배치
  - UART-M LPUART 인스턴스와 설정 심볼(`NET_BOARD_UART_M_INSTANCE`, `NET_BOARD_UART_M_CONFIG`, `NET_BOARD_UART_CFG_HEADER`), 콜백 연결 방식(`NET_BOARD_UART_CALLBACK`). **UART-M 기본값 LPUART0(`Lpuart_Uart_Ip_xHwConfigPB_0`)은 자리표시값이다.** 실제 인스턴스로 바꿔야 한다.
  - 시간원(방식 A·B·C 중 하나). 설명은 원본 `firmware/network/nodes/vss/VSS_NODE_INTEGRATION.md`의 시간원·보드 설정 절과 같다.
- 사본 안의 모든 `.c`는 위 include 폴더, mock SDK(`firmware/network/sdk_mock/rtd/include`, NET_git 안에서만), C2 헤더 폴더로 `-std=c11 -Wall -Wextra -Wpedantic -Werror` 단독 컴파일이 된다. 실제 RTD 헤더로의 컴파일과 링크는 보드 적용 때 확인한다.
- **enum 크기:** Network 소스는 `-fshort-enums`(`arm-none-eabi-gcc` 기본)와 `-fno-short-enums`(S32DS 프로젝트 설정) 모두에서 위 옵션으로 컴파일된다. 한 빌드 안의 DOMAIN·Network·RTD 파일은 같은 설정을 쓴다. C2 구조체와 Network 구조체의 배치가 enum 크기에 따라 달라지기 때문이다.

## DOMAIN 팀 확인이 필요한 것

| 항목 | 지금 Network | 근거 |
|---|---|---|
| 값 품질 변환 규칙 | wire `VALIDITY`·`REASON`을 `DataQuality_t`로 바꾸는 규칙은 `NetDomainC2Config.value_quality` 콜백으로 받는다. 없으면 해당 수신(0x300, 0x310, 0x0A0 등)을 HOLD한다. `net_domain_c2_value_quality_provisional`(STALE 사유 → STALE, NO_DATA 사유 → NO_DATA, 유효 + 사유 없음 → OK, 나머지 → INVALID)은 잠정 규칙이다 | `docs/network/interface/04_COMMON/30_ENUM_WIRE_MAPPING.md`(`DataQuality_t`, `ValueValidity_t`, `QualityReason_t`: 제안 Codec 계약, "의미로 명시 변환"), `docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md`(`NetValueQuality`) |
| 시간 함수 | Network는 DOMAIN의 메인 루프 전용 ms 함수(`uint32_t`) 하나를 전제한다. `main`이 바퀴마다 한 번 읽어 `Net_MainFunction`·`Net_TxFlush`에 넘긴다. 함수 이름과 출처는 DOMAIN이 정한다. C2 조회 함수도 owner의 현재 ms(`nowMs`)를 받는다 | `docs/network/interface/02_NODES/DOMAIN/20_API.md`(`DomainIf_QueryGet*`의 `nowMs` 인자 설명) |
| DOMAIN_PERMISSION 송신 | DOMAIN develop은 `DomainIf_TxCisPowerPermission`을 아직 부르지 않는다. 시험대에서는 `NET_DOMAIN_BENCH_PERMISSION`을 켠 `NetDomainBench_PermissionPoll`(시험 전용)이 대신 부른다 | `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md`(DOMAIN_PERMISSION) |
| 상태 보고 품질 | BCM_STATUS(0x230)·CIS_STATUS(0x320)·VSS_STATUS(0x340) wire에는 품질 필드가 없다. 보드 hook은 잠정 규칙 `net_domain_c2_status_quality_provisional`(수신·디코드가 정상이면 `meta.quality = OK`, 신선도는 DOMAIN이 `meta.ageMs`와 TTL로 판단)을 쓴다. 이견이 있으면 이 콜백을 바꾼다 | `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md`(BCM_STATUS, CIS_STATUS, VSS_STATUS) |
| 의미 콜백 6개 | WINDOW 3개, 로컬 실패 사유, Query 계획, 스냅숏 확인은 보드 hook에서 NULL이다. 그 경로는 HOLD다(M_QUERY 응답 등). 스냅숏 확인을 연결하면 UART-M 상태 중계(받은 CAN 상태의 UART-M 투영, `txMobileBcmDoorState` 등)가 나간다. 중계와 WINDOW 명령(점검 경로)도 명령처럼 C2 송신 기록 하나를 쓰고, 끝나면(보냄, 또는 실패 뒤 장치가 놓음) 그 기록과 M04 송신 칸을 회수한다. 식별이 없어 실패 알림은 없다(`domain_node.n24_state_relay_retired`: 중계 32번 뒤 남은 기록 0, 선로가 받지 않은 중계는 TX timeout abort 뒤 회수. WINDOW 명령은 `domain_c2_window_command_retired`). C2 송신 기록(명령, VSS 이벤트, M_RESULT)이 장치에 닿기 전에 사용 기한 만료가 아닌 사유(HOLD·STALE 등, bus-off 때 서비스 큐에서 기다리던 명령 포함)로 끝나면, 로컬 실패 사유 콜백이 없어 DOMAIN에 알리지 못하고 회수하지 않는다. 이 기록은 M04 송신 칸(16)을 이 실행 동안 차지하고, 칸은 DOMAIN_PERMISSION·VSS_WARNING_STATE 주기 송신과 함께 쓰므로 쌓이면 이 송신도 막힌다. 장치가 받은 뒤 실패한 기록(bus-off·TX timeout abort)은 콜백과 관계없이 HOLD다(`04_COMMON/20_API_USAGE_AND_FAILURE.md` "접수 후 실패의 닫힌 범위"의 "제출 뒤 미분류 in-flight 오류": 식별·송신 단계를 남기고 그 경로를 HOLD, 의미 연결은 미결). 이 기록은 C2 송신 기록만 차지하고 M04 칸은 회수한다(위 "CAN bus-off"). 지금은 리셋 말고 이 기록을 푸는 경로가 없다. DOMAIN이 이 경우를 어떻게 받을지 정하면 Network 쪽에 푸는 경로를 넣는다 | `include/network/net_domain_c2.h`(`NetDomainC2Config`) |
| 명령 한도 | ACCEPT_LIMIT는 200 ms 후보(1 s 충돌 OPEN), VSS_EVENT USE_LIMIT는 500 ms 초안을 보드 hook에 둔다 | `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md`(명령 공통부, VSS_EVENT) |
| CIS 전환 함수 | Network는 새 CIS BOOT(환경)·Pi 세대(Occupant)를 막고 전환 binding을 한 번 불러 완료 receipt를 확인한 뒤 다시 전달한다. binding 구현(DOMAIN private 전환 호출, `DomainRuntime_GetCisEnvironmentPrivate`·`GetCisOccupantPrivate`)과 `NetDomainBoard_SetTransitionBinding` 호출은 DOMAIN 쪽 몫이고 아직 없다. Occupant를 켤지(`cisOccupantEnabled`, TTL 두 값)도 DOMAIN이 정한다. 링크 epoch 수용은 통합 담당이 `net_domain_c2_accept_ingress_epoch`로 알린다 | `docs/network/interface/02_NODES/DOMAIN/20_API.md`("환경 BOOT 변경 시 C2 Rx 사용"), `docs/network/interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md`(`delta-environment-routing`), `docs/network/interface/07_CROSS_NODE_INTERFACE_RULES.md`(`environment-delta`) |
| DOMAIN_STATE | `M_DOMAIN_ALIVE`(UART-M 0x02, 250 ms)의 `DOMAIN_STATE`(0=INIT, 1=READY, 2=DEGRADED, 3=FAULT)는 실제 DOMAIN 상태여야 한다. Network는 READY를 스스로 만들지 않는다. 이 값을 주는 DOMAIN 함수가 정해지지 않았다. Network는 C2 설정의 `alive_state`로 받을 자리만 두었고, 보드 hook은 비워 둔다(`INIT / NO_DATA / 0 / 0xFFFF`) | `docs/network/interface/03_COMMUNICATION/20_UART_M.md`(`M_DOMAIN_ALIVE` B10), `docs/network/sw_detail/functions/uart_m/M_DOMAIN_ALIVE.md` |
| S32K344 LPUART 인스턴스 | UART-M(ESP32 ↔ DOMAIN, 초기 115200 bps)에 쓸 LPUART 번호와 생성 설정 이름. 지금 기본값 LPUART0은 자리표시값이다 | `docs/network/interface/01_NETWORK_OVERVIEW.md`(UART-M v2 · 초기 115200 bps), `docs/network/interface/06_TECHNICAL_CONSTRAINTS.md`("태스크·드라이버·핀·전압을 임의로 확정하지 않는다. 구현 직전에 보드 자료와 대조한다") |
