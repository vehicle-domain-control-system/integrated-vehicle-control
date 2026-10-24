# DOMAIN 노드 Network 사본 안내

DOMAIN 노드(profile 5, CAN + UART-M) 저장소의 `firmware/DOMAIN/network/`에 두는 Network 사본의 안내다. 원본은 NET_git `firmware/network/DOMAIN_NODE_INTEGRATION.md`이고, 사본에는 `README.md`로 들어간다. 설계 근거는 `docs/network/interface/02_NODES/DOMAIN/{10_DATA,20_API,25_PRIVATE_ROUTING}.md`, `03_COMMUNICATION/10_CAN_FD.md`, `03_COMMUNICATION/20_UART_M.md`이다.

## 이 폴더는 생성 사본이다

- **원본:** NET_git의 `firmware/network`. 이 폴더의 파일은 `firmware/network/tools/sync_to_s32ds.py --node DOMAIN`이 원본과 같은 상대 경로로 복사한 것이다(예: `src/l04_frame_codec/net_wire_common.c`, `include/network/net_types.h`).
- **직접 고치지 않는다.** 고칠 것이 있으면 원본을 고치고 커밋한 뒤 다시 만든다. NET_git 루트에서:
  ```
  python -B firmware/network/tools/sync_to_s32ds.py --node DOMAIN --out <DOMAIN 저장소>/firmware/DOMAIN/network
  python -B firmware/network/tools/sync_to_s32ds.py --node DOMAIN --out <DOMAIN 저장소>/firmware/DOMAIN/network --apply
  python -B firmware/network/tools/sync_to_s32ds.py --node DOMAIN --check <DOMAIN 저장소>/firmware/DOMAIN/network
  ```
  - 첫 줄은 dry run이다. 새로 쓰고, 바꾸고, 지울 파일만 출력하고 아무것도 쓰지 않는다.
  - `--apply`는 다음 경우 아무것도 쓰지 않고 거부한다(종료 코드 1): 원본에 커밋되지 않은 변경이 있다, 폴더 이름이 `network`가 아니다, 이 폴더에 고친 파일이나 기록에 없는 파일이 있다, `NETWORK_BUNDLE.txt`가 없거나 다른 노드의 것이다.
  - 같은 원본으로 다시 실행하면 바뀌는 파일이 없다. 원본에서 빠진 파일은 지워진다.
- **`NETWORK_BUNDLE.txt`:** 생성 기록이다. 노드, 프로필(`NETWORK_UPPER_NODE_PROFILE=5`), 원본 브랜치·커밋(`source_commit`), 원본 상태(`source_state`, 커밋되지 않은 변경이 있었으면 `dirty`), 파일마다 SHA-256이 들어 있다. 해시는 CRLF를 LF로 읽은 내용의 값이라 Windows 체크아웃 설정(`core.autocrlf`)과 상관없이 같다. 시각은 넣지 않는다.
- **`--check` 결과(마지막 줄):**
  - `CHECK DOMAIN OK`(종료 0): 기록과 같고 지금 원본으로 만들어도 같다.
  - `CHECK DOMAIN EDITED`(종료 1): 기록이 없거나 형식이 틀리다, 파일이 고쳐졌거나 빠졌다, 기록에 없는 파일이 있다.
  - `CHECK DOMAIN STALE`(종료 3): 사본은 그대로지만 원본이 바뀌었다. 다시 만들어야 한다(달라진 경로를 출력한다).

## 들어 있는 것

- **공통 코덱·서비스:** `src/l01_node_context`, `src/l02_typed_handoff`, `src/l03_message_service`, `src/l04_frame_codec`, `src/l05_transport_port`의 공통 소스와 `include/network/**`(host 전용 `net_host_mock.h`, `net_virtual.h` 제외). CAN·UART 코덱, 수신 검증과 순번, M04 메시지 서비스(BOOT·TX_SEQUENCE·LINK_SEQUENCE, 반복 송신), 조회(Query), 관측·진단이다.
- **C2 어댑터:** `src/l02_typed_handoff/m03_typed_handoff/net_domain_c2.c`, `net_domain_rx.c`, `net_domain_tx.c`, `net_domain_uart.c`, `net_domain_query.c`, `net_domain_owner_binding.c`, 공개 헤더 `include/network/net_domain_c2.h`, 비공개 헤더 `net_domain_private.h`, `net_domain_enum_internal.h`. DOMAIN C2 경계(`Domain_Interface_v0.5_C2_target.h`)와 Network 사이를 잇는다. 정해지지 않은 의미 규칙은 `NetDomainC2Config`의 콜백으로 받고, 콜백이 없으면 그 경로만 HOLD한다(추측한 OK를 만들지 않는다).
- **RTD 백엔드:** `src/platform/rtd/net_hal_rtd.c`(시간원, 시작 순서, ISR 링), `net_hal_rtd_can.c`(FlexCAN), `net_hal_rtd_uart.c`(LPUART, DOMAIN 프로필에서는 UART-M 링크만), 보드 설정 `net_board_cfg.h`.
- **들어 있지 않은 것:** 다른 노드 전용 파일(VSS·CIS·BCM App facade, 노드 프로필 표, 조립 루트 `src/node/`, `net_node.h`, `net_if_entry.h`), host 전용 파일, mock SDK, 시험, 도구.

## 아직 빠진 것: DOMAIN 조립 루트

- DOMAIN 조립 루트(노드 프로필 표와 `Net_Init`·`Net_MainFunction` 공개 진입)가 아직 없다. 그래서 이 사본만으로는 `Net_Init`과 `Net_MainFunction`을 쓸 수 없다.
- 그래서 이 폴더는 DOMAIN S32DS 빌드에 넣지 않는다. DOMAIN 소스 폴더 `firmware/DOMAIN/src` 밖에 두는 이유다. 안에 두면 공개 진입이 없는 Network 소스가 DOMAIN 빌드에 섞인다.
- 조립 루트가 생기면 원본 목록(`firmware/network/tools/s32ds_sources.txt`)의 태그가 바뀌고, 사본을 다시 만들면 그 파일이 들어온다.

## 나중에 빌드에 넣을 때 필요한 것

- **include 폴더 7개**(사본 기준, `firmware/network/CMakeLists.txt`의 `_node_includes`와 같다): `include`, `src/l04_frame_codec`, `src/l02_typed_handoff/m03_typed_handoff`, `src/l05_transport_port/m07_buffer_outcome`, `src/l05_transport_port/m09_observation`, `src/l03_message_service/m04_message_service`, `src/platform/rtd`
- **C2 헤더 폴더:** `firmware/DOMAIN/src/interface`(`Domain_Interface_v0.5_C2_target.h`, `Vehicle_Types_v0.2.h`)
- **define:** `NETWORK_UPPER_NODE_PROFILE=5`. 빠뜨리면 `include/network/net_upper_profile.h`가 컴파일 오류를 낸다(기본 프로필은 없다).
- **RTD 생성 설정 이름**(`src/platform/rtd/net_board_cfg.h`, 프로젝트 설정의 `-D`로 바꾼다):
  - FlexCAN 설정·상태 심볼과 ConfigTools 콜백 이름(`NET_BOARD_CAN_CONFIG`, `NET_BOARD_CAN_STATE`, `NET_BOARD_CAN_CALLBACK`, `NET_BOARD_CAN_ERROR_CALLBACK`), MB 배치
  - UART-M LPUART 인스턴스와 설정 심볼(`NET_BOARD_UART_M_INSTANCE`, `NET_BOARD_UART_M_CONFIG`, `NET_BOARD_UART_CFG_HEADER`), 콜백 연결 방식(`NET_BOARD_UART_CALLBACK`). **UART-M 기본값 LPUART0(`Lpuart_Uart_Ip_xHwConfigPB_0`)은 자리표시값이다.** 실제 인스턴스로 바꿔야 한다.
  - 시간원(방식 A·B·C 중 하나). 설명은 원본 `firmware/network/VSS_NODE_INTEGRATION.md`의 시간원·보드 설정 절과 같다.
- 사본 안의 모든 `.c`는 위 include 폴더, mock SDK(`firmware/network/sdk_mock/rtd/include`, NET_git 안에서만), C2 헤더 폴더로 `-std=c11 -Wall -Wextra -Wpedantic -Werror` 단독 컴파일이 된다. 실제 RTD 헤더로의 컴파일과 링크는 조립 루트가 생긴 뒤 확인한다.

## DOMAIN 팀 확인이 필요한 것

| 항목 | 지금 Network | 근거 |
|---|---|---|
| 값 품질 변환 규칙 | wire `VALIDITY`·`REASON`을 `DataQuality_t`로 바꾸는 규칙은 `NetDomainC2Config.value_quality` 콜백으로 받는다. 없으면 해당 수신(0x300, 0x310, 0x0A0 등)을 HOLD한다. `net_domain_c2_value_quality_provisional`(STALE 사유 → STALE, NO_DATA 사유 → NO_DATA, 유효 + 사유 없음 → OK, 나머지 → INVALID)은 잠정 규칙이다 | `docs/network/interface/04_COMMON/30_ENUM_WIRE_MAPPING.md`(`DataQuality_t`, `ValueValidity_t`, `QualityReason_t`: 제안 Codec 계약, "의미로 명시 변환"), `docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md`(`NetValueQuality`) |
| 시간 함수 | C2 조회 함수는 owner의 현재 ms(`nowMs`, monotonic `uint32_t`)를 받는다. DOMAIN이 주는 ms 함수의 이름과 출처, 메인 루프 전용인지가 정해지지 않았다 | `docs/network/interface/02_NODES/DOMAIN/20_API.md`(`DomainIf_QueryGet*`의 `nowMs` 인자 설명) |
| CIS 전환 함수 | CIS BOOT가 바뀌면 이전 BOOT의 환경(0x300)·Occupant(0x310) 전달을 막고, DOMAIN 소유 동기 Private 전환이 끝난 것을 확인한 뒤 새 BOOT 입력을 다시 전달해야 한다. 이 사본에는 전환 함수 호출이 없다. 링크 epoch 수용은 통합 담당이 `net_domain_c2_accept_ingress_epoch`로 알린다 | `docs/network/interface/02_NODES/DOMAIN/20_API.md`("환경 BOOT 변경 시 C2 Rx 사용"), `docs/network/interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md`(`delta-environment-routing`), `docs/network/interface/07_CROSS_NODE_INTERFACE_RULES.md`(`environment-delta`) |
| DOMAIN_STATE | `M_DOMAIN_ALIVE`(UART-M 0x02, 250 ms)의 `DOMAIN_STATE`(0=INIT, 1=READY, 2=DEGRADED, 3=FAULT)는 실제 DOMAIN 상태여야 한다. Network는 READY를 스스로 만들지 않는다. 이 값을 주는 DOMAIN 함수가 정해지지 않았다 | `docs/network/interface/03_COMMUNICATION/20_UART_M.md`(`M_DOMAIN_ALIVE` B10), `docs/network/sw_detail/functions/uart_m/M_DOMAIN_ALIVE.md` |
| S32K344 LPUART 인스턴스 | UART-M(ESP32 ↔ DOMAIN, 초기 115200 bps)에 쓸 LPUART 번호와 생성 설정 이름. 지금 기본값 LPUART0은 자리표시값이다 | `docs/network/interface/01_NETWORK_OVERVIEW.md`(UART-M v2 · 초기 115200 bps), `docs/network/interface/06_TECHNICAL_CONSTRAINTS.md`("태스크·드라이버·핀·전압을 임의로 확정하지 않는다. 구현 직전에 보드 자료와 대조한다") |
