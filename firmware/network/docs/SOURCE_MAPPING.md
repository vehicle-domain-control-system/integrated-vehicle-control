# 지금 Network 소스와 책임 대응

이 대응표는 지금의 분리된 이식형 제품 소스를 설명한다. 골격만 있던 예전 대응표를 대신한다. 바뀌지 않는 과거 Wire 보고서는 각자 원래 revision의 증거로 남는다. 설계는 **Layer 5개, Module 9개, 책임 ID 25개**다. `src/platform/`은 M08 binding을 구현하는 곳이고, Layer가 하나 더 있는 것이 아니다.

| Layer / Module | 제품 파일 | 책임과 실제 동작 |
|---|---|---|
| L01 / M01 | `net_upper_source.c`, `net_upper_receipt.c`, `net_upper_monitor.c`, `net_source.c` | F01-F03: 바뀌지 않는 source AGE·receipt 출처, 서로 독립된 원 식별 검사, 수용한 Pi/CAN relay와 문맥 소유. `net_upper_monitor.c`는 명시적으로 고른 원 AGE 감시와 수신 끊김 감시(`NetPermissionGuard`, `NetRouteWatch`)다 |
| L01 / M02 | `net_upper_query.c` | F04-F05: 상한이 있는 Query 수명, getter 칸 18개 / 출력 형식 17개, 출처, 최종 TX 집계와 End |
| L02 / M03 | `net_upper_adapter.c`, `net_ecu_adapter.c`, `net_app_port_*.c`, `net_domain_*.c` | F06-F10: 형식별 ECU48 handoff, 노드별 App Port facade(VSS·CIS·BCM), 실제 고정 C2의 TxPort·RX·Query·로컬 실패 변환. 외부 Owner hook은 명시적으로 남는다 |
| L03 / M04 | `net_upper_service.c`, `net_upper_can.c`, `net_upper_uart.c`, `net_upper_control.c`, `net_upper_publish.c`, `net_upper_profile.c` | F11-F13: 바뀌지 않는 큐, dispatch, 경로 48개 / 수신 목적지 51개, 원 식별과 전송 식별, 독립 AGE lane, 문맥, 만료와 복구 제어. Network가 소유하는 메시지별 CAN BOOT_ID·TX_SEQUENCE, 대기 AGE를 더하는 최신값 publish 칸, 주기 반복 구동. `net_upper_profile.c`는 노드 프로필에 따른 링크 선택과 저장소 칸 배치다 |
| L04 / M05 | `net_wire_can*.c` | F14-F15: CAN FD 형식 encode/decode 함수 22개, 경로 envelope, 크기·padding, payload 범위와 오류 |
| L04 / M06 | `net_wire_uart*.c`, `net_wire_stream.c` | F16-F18: UART-M 23개 / UART-C 3개 형식 코덱, CRC, 상한이 있는 stream 재동기화와 원 바이트 시각 |
| L05 / M07 | `net_upper_outcome.c`, `net_upper_detail.c` | F19-F21: 서로 독립된 접수·최종·해제 사실, 바뀌지 않는 빌린 버퍼, 실패 기록·ack와 명시적 retire |
| L05 / M08 | `net_upper_port.c`, `net_hal_adapter.c`, `src/platform/*/net_*_upper_port.c`, `src/platform/rtd/net_hal_rtd*.c`, `src/platform/host_virtual/net_virtual.c`, `src/platform/net_boot_id.c` | F22-F23: 시계·백엔드 주입, capture, 손실·reset epoch, 직렬 수명 주기, RTD 백엔드(FlexCAN·LPUART·시간원, ISR 링), 실행 가능한 가상 버스 백엔드. `net_boot_id.c`는 BOOT_ID 계산(`net_boot_id_next`)이다 |
| L05 / M09 | `net_upper_observation.c`, `net_upper_diagnostics.c` | F24-F25: 포화하는 raw 카운터, snapshot, 링크·큐 관측. 실제 C2 raw Health getter는 위임하고 Owner 심각도를 지어내지 않는다 |

앞 다섯 줄의 소스 파일은 그 Module 폴더 안에 있다. 한 노드만 쓰는 M03 파일(App Port facade `net_app_port_<node>.c`, DOMAIN C2 어댑터 `net_domain_*.c`)은 `nodes/<node>/` 아래 같은 Module 폴더에 있다. 공용 wire LE helper와 바뀌지 않는 registry는 `src/l04_frame_codec/`에 남는다. `include/network/`에는 이식형 타입과 Network 헤더가 있다(M02 내부 경계 헤더 `net_upper_query.h`도 여기 있다). 노드 앱 쪽 선언은 `nodes/<node>/include/network/app/net_if_<node>.h`(VSS·CIS·BCM)에 있다. VSS 앱은 같은 선언의 자기 사본 `vss_net_api.h`를 쓰고 Network와 링크에서 만난다. 원래 Owner 공개 헤더는 명시적인 CMake include 경로로 읽기 전용으로 받는다.

`network_common` 라이브러리에는 남겨 둔 골격 `net_runtime`, `net_buffer`, `net_handoff`, `net_can_codec`, `net_uart_stream`, `net_port`, `net_observation`과 그 원래 raw fixture가 있다. 제품이 쓰는 `net_source.c`(M01)와 BOOT_ID 계산 `net_boot_id.c`도 이 라이브러리로 빌드되고, `network_product`가 이 라이브러리를 링크한다. 골격의 Host 시험은 회귀 증거로 남는다. 이 예전 합성 fixture API는 제품 wire도, 실제 차량 Owner API도 아니다.

Layer 밖에는 다음이 있다.

- `src/node/`: 노드 공통 진입. 공개 진입 `net_entry.c`(`Net_Init`, `Net_MainFunction`, `Net_TxFlush`, `Net_GetBootId`), VSS·CIS·BCM 공통 `net_node.c`. 노드 프로필 표 `net_node_profile_<node>.c`와 DOMAIN 조립 루트 `net_node_domain.c`는 `nodes/<node>/src/node/`에 있다.
- `nodes/<node>/`: 한 노드만 쓰는 파일. 안은 공통 트리와 같은 모양이다(위 M03 파일, 노드 프로필 표·조립 루트, 공개 헤더). 실제 보드 프로젝트에 넣는 보드 hook(`nodes/vss/board/s32k344_vss/`, `nodes/domain/board/s32k344_domain/`), 시험대 코드(`nodes/<node>/bench/`, 제품 코드 아님), 노드 시험, 노드 연동 문서도 여기 있다. 보드 프로젝트 사본과 노드 사본에는 `nodes/<node>/`를 뺀 같은 경로로 들어간다.
- `docs/`: 이 대응표, 보드 시험 결과, 과거 기록.
- `sdk_mock/rtd/`: Host에서 RTD 백엔드를 돌리는 mock SDK다. NXP 파일이 아니다.

파일 단위 표 `IMPLEMENTATION_MATRIX.csv`는 예전 표다. Layer 폴더와 `src/platform/`의 host_mock·host_virtual·s32k144·s32k344 파일만 담았고(경로는 `nodes/`로 나누기 전이다), `src/node/`, `src/platform/rtd/`, `net_boot_id.c`, 보드 hook, 그 뒤에 생긴 App Port 파일과 `net_upper_publish.c`는 없다. 지금 파일 구성은 이 문서를 본다.

## 실행 증거와 상세 설계 추적

제품 시험은 서비스 접수, dispatch, 형식 코덱 48개 전부, 수신 목적지 51개, 원 식별·전송 식별 충돌, relay, Query getter 칸 18개, 실패·취소, 노드 프로필, 주입한 백엔드 capture를 실행한다. ECU 어댑터 시험은 CAN 형식 publisher 22개 / 수신 목적지 25개와 UART publisher·수신 목적지 26개를 모두 실행하며, 보류된 wire 7개 / 보류된 RX 8개를 서로 다른 관찰 sink로 확인한다. 일반 WINDOW 콜백은 건드리지 않는다.

`tests/virtual_ecu/`는 물리 전송과 외부 Owner만 대신하고, 따로 도는 ECU worker가 제품 소스를 실행한다. TX 네 단계를 따로 관찰한다. 같은 폴더의 HAL 없는 노드 worker `virtual_node.c`는 노드 프로필 표와 App Port facade를 돌리고, `virtual_peer.c`는 그 상대다(HOST 프로필 제품 서비스와 시험 Owner: CAN의 DOMAIN, UART-C의 Pi). DOMAIN 노드 worker(`virtual_domain_node.c`)는 DOMAIN 조립 루트, DOMAIN 보드 파일, 읽기 전용 DOMAIN 소스의 실제 런타임으로 돈다. 결과 패키지는 정확한 시험 이름, 종료 코드, trace를 기록한다. 소스 참조만으로는 시험 실행 증거가 되지 않고, wrapper 시험이 통과했다고 해서 연결되지 않은 차량 정책이 증명되는 것도 아니다.

전달본의 `DESIGN_TO_CODE_TRACE.csv`는 최신 설계 문서 전부를 SHA와 함께 목록으로 둔다. 상세 함수·콜백 표와 typedef 표는 후보 이름, 실제 제품 대응, 호출 그래프 후보, 시험, 의미 HOLD를 담는다. 후보 private 함수 이름은 중복 wrapper를 만들라는 요구가 아니고, 공개 `DomainIf_*` 선언을 다시 정의해도 된다는 허락도 아니다.

## 연동 경계

`NETWORK_TARGET=host`는 Host 시험을 돌린다. s32k144/s32k344를 고른 native 빌드는 SDK 없는 주입 Port 어댑터와 함께 이식형 라이브러리를 컴파일하고, Host 실행 파일은 만들지 않는다. RTD 백엔드(`src/platform/rtd/`)는 Host에서 mock SDK로 시험하고, `tools/sdk_swap_check.py`가 S32DS 프로젝트의 실제 RTD 헤더에 대고 ARM GCC `-fsyntax-only`로 검사한다(오브젝트·링크 없음). 보드 빌드·링크, Flash, 실제 버스 시험, 대상 ECU 전체 RAM 판정은 `tools/sync_to_s32ds.py`로 Network 소스를 복사한 S32DS 보드 프로젝트에서 하는 별도 gate이고, 이 저장소에서는 실행하지 않는다(NOT_RUN/UNKNOWN). 노드 프로필 저장소에는 다른 노드의 상태가 없다. Network가 아닌 앱, RTD, 드라이버, 스택, 힙, 예비 RAM은 모르면 UNKNOWN이다.

C2 Owner commit 증거, 실제 Owner getter가 없는 것, 사유·품질 선택, session 발급, 순환(wrap) 정책은 명시적인 연동 hook 또는 HOLD로 남는다. BOOT_ID 계산 함수 `net_boot_id_next`는 있지만, 보드 출처(warm reset에도 남는 기록, 리셋 원인, seed)가 HOLD라서 지금 보드 파일은 고정 시험값을 쓴다. WINDOW는 관찰 전용 전송만 따로 있고 구동기 승인은 없다. G01-G08은 HOLD 그대로다. 과거 기록 문서(`WIRE_CANDIDATE_STATUS.md`, `WIRE_WAVE2A_STATUS.md`, `WIRE_WAVE2B_STATUS.md`, `BOARD_BINDING_STATUS.md`)는 한국어로 옮기기만 했고 내용은 그 wave 당시 그대로다.
