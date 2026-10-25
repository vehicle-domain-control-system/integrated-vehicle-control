# 이식형 Network 제품 스택

지금 제품 진입점은 `include/network/net_upper.h`의 `NetUpper`다. CAN FD·UART-M·UART-C 형식 코덱 48개를 상한이 있는 제품 큐, 지연 수신 처리, 형식별 ECU 어댑터, 조회 제어기와 주입된 Port를 거쳐 실행한다. 남겨 둔 `network_common` / `NetRuntime` 골격은 따로 있는 과거 시험 대상이고 제품 서비스가 아니다.

소유 구조는 5개 Layer와 9개 Module 그대로다. 설계 책임은 25개이지만, 이것이 wrapper 함수 25개를 만들라는 뜻은 아니다. 지금 소스는 `SOURCE_MAPPING.md`가 설명한다(`IMPLEMENTATION_MATRIX.csv`는 예전 파일 단위 표다). 실제로 돌린 시험 결과, 소스 SHA, 상세 설계 선언과 HOLD 상태는 결과 패키지에 둔다. 이 파일은 빌드가 됐다는 이유로 PASS를 추정하지 않는다.

## Host에서 이식형 제품 빌드

```sh
cmake -S firmware/network -B build-network -DNETWORK_TARGET=host -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON
cmake --build build-network --parallel
ctest --test-dir build-network --output-on-failure
```

Host에 맞는 C11 컴파일러와 generator를 고른다. wire 선택 `NET_WIRE_CANDIDATE_ONLY`는 명시적인 로컬 프로필이다. Host 시험 통과가 외부 계약의 승인은 아니다. `NETWORK_NODE_PROFILE`은 HOST, CIS, BCM, WINDOW, DOMAIN, VSS를 받고 저장소 크기는 프로필이 정한다. 차량 앱·RTD·스택·힙 RAM은 모르면 UNKNOWN이다. Network 크기만 보고 들어간다고 추정하지 않는다.

`NETWORK_TARGET=s32k144` 또는 `s32k344`에 `BUILD_TESTING=OFF`를 주면 같은 이식형 C 라이브러리와 SDK 없는 어댑터 브리지를 빌드한다. RTD, 핀, 클럭, IRQ, DMA, 링커 스크립트, ARM 컴파일러는 설정하지 않는다. `NETWORK_BUILD_BOARD_HAL=ON`을 명시하면 실패한다. 여기에는 보드 백엔드가 없기 때문이다. 예전 `net_s32k144_port.c` / `net_s32k344_port.c` 골격 함수는 계속 실패 쪽으로 닫혀 있다. 실제 제품 주입은 `net_*_upper_port.c`와 `NetHalAdapter`를 쓴다.

## RTD 노드의 MCU 선택

`NETWORK_NODE_PROFILE`은 ECU 역할을 고른다. MCU는 고르지 않는다. 공통 `NetNode_Init`은 `NETWORK_NODE_PLATFORM=144` 또는 `344`(`src/platform/rtd/net_board_cfg.h`)로 같은 `NetHalAdapter`를 초기화한다. S32DS에서는 명시한 값이 없으면 프로젝트에 이미 있는 컴파일러 define `CPU_S32K144HFT0VLLT` 또는 `CPU_S32K344`가 그 선택이 된다. 선택이 없거나, 모르는 값이거나, 서로 다르면 컴파일 오류다. 다른 CPU 부품 define을 쓰는 프로젝트는 `NETWORK_NODE_PLATFORM`을 명시해야 한다.

Host RTD mock 빌드는 CMake 옵션을 명시해야 한다. 예:

```sh
cmake -S firmware/network -B build-network-cis -DNETWORK_TARGET=host -DBUILD_TESTING=ON -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON -DNETWORK_NODE_PROFILE=CIS -DNETWORK_BUILD_RTD_MOCK=ON -DNETWORK_NODE_PLATFORM=144
```

기존 VSS 보드는 344, CIS S32K144 RTD 보드 연동은 144를 쓴다. BCM은 실제 보드 프로젝트에 맞춰 고른다(지금 BCM 설계는 144를 지정하지만 그 보드 프로젝트는 여기에 없다). mock SDK는 자체 합성 주변장치 설정을 그대로 쓴다. 그래서 144를 고르면 어댑터 식별과 제품 경로를 확인하는 것이지, 실제 S32K144 핀, RTD 헤더, 자원을 확인하는 것은 아니다. `NETWORK_TARGET`만 주면 지금도 위에서 말한 이식형 라이브러리만 고른다.

`tools/s32ds_sources.txt`는 두 MCU 선택과 mock 빌드가 함께 쓴다. 노드가 고른 식별값으로 `net_hal_adapter_init`을 직접 부르므로, 이 목록에는 더 이상 S32K344 고정 wrapper가 필요 없다. 144/344 upper-port wrapper API는 이식형 어댑터 라이브러리에 따로 남아 있다. 복사 도구는 계속 예전 S32K144 골격 소스를 뺀다.

## 제품 연동

- `net_upper_init`, `net_upper_capture`, `net_upper_step`: 호출자가 소유한 새 저장소, 안정된 이벤트 복사본, 직렬화된 superloop 하나.
- `NetEcuAdapter`: 형식별 송신 함수 48개와 형식별 수신 콜백. 콜백이 없으면 실패 쪽으로 닫힌다. 차량 판단 알고리즘을 만들어 넣지 않는다.
- `net_upper_cis_publish_rear`, `net_upper_cis_publish_occupant`, `net_upper_cis_publish_pi_permission`: 제품 relay는 수용한 receipt, 원 식별, 원 AGE를 유지한다.
- `NetUpperQuery`: 문맥 검증, 상한이 있는 getter 진행, 원 출처, 실제 로컬 TX 성공 집계, Query End와 취소.
- `NetPermissionGuard`와 `NetRouteWatch`: 명시적으로 고른 원 AGE 감시와 수신 끊김 감시. view는 원 DTO와 사유를 유지한다. timeout이 DENY/ALLOW DTO를 만들어 내지 않는다. 두 객체의 step·read 함수는 같은 직렬 superloop에서 부른다. Event/Result가 끊긴 것은 주기 생존 신호가 아니다.
- `NetDomainC2`: 선택 기능. 실제 고정 C2 헤더 연결, TxPort 등록, 형식 변환, Owner RX와 getter, 로컬 실패, commit 대조 검사. `NETWORK_BUILD_DOMAIN_C2=ON`을 켜고 `NETWORK_DOMAIN_C2_INCLUDE_DIR`을 준다. 실제 Owner 본체나 의미 규칙 hook이 없으면 그 매핑만 HOLD한다. DOMAIN 공개 API를 대신하는 선언은 만들지 않는다.
- `NetVirtualBackend`: Host에서 실행되는 드라이버 대체물. Port 어댑터와 함께 `NETWORK_BUILD_VIRTUAL_ECU=ON`을 켜면 고른 ECU worker를 돌린다. `tests/virtual_ecu/network_virtual_demo.py`는 준비한 프로필 worker들을 실제 CAN envelope와 UART 바이트로 잇는다. Owner 가짜는 제품 서비스 밖에 있고, 제품 큐·코덱·라우터·어댑터가 실행된다. `NETWORK_BUILD_VIRTUAL_NODE=ON`은 대신 HAL 없는 노드 worker를 빌드한다. 이 백엔드 위에서 VSS / CIS / BCM의 노드 프로필 표(`src/node/net_node_profile_<node>.c`)와 App Port facade, 그리고 HOST 상대 노드(CAN의 DOMAIN, UART-C의 Pi)를 돌린다. `tools/virtual_nodes_run.py --build-dir <dir>`는 넷을 모두 빌드하고, include 경로가 mock SDK, `src/platform/rtd`, NXP SDK에 닿지 않는지 확인한 뒤 facade 왕복을 돌린다(`RESULT passed=N failed=M`). `--domain-owner-src <DOMAIN src 폴더>`와 `--domain-c2-include <C2 헤더 폴더>`를 함께 주면 DOMAIN 노드 worker도 빌드한다. 이 worker는 DOMAIN 조립 루트(`src/node/net_node_domain.c`), DOMAIN 보드 파일, 읽기 전용 DOMAIN 소스의 실제 런타임으로 이뤄진다. 그다음 CAN의 DOMAIN 쪽을 HOST 상대 대신 이 worker로 바꿔 한 번 더 돈다. 두 옵션을 주지 않으면 이 실행은 빠진다.
- 노드 진입 함수: `include/network/app/net_if_entry.h`의 `Net_Init`, `Net_MainFunction`, `Net_TxFlush`, `Net_GetBootId`. 구현은 모든 노드 프로필이 함께 쓰는 `src/node/net_entry.c`다. `Net_TxFlush`는 선택 함수다. 같은 superloop 바퀴에서 `Net_MainFunction` → 앱 → `Net_TxFlush` 순으로 부르면, 앱이 그 바퀴에 낸 송신을 다음 바퀴를 기다리지 않고 내보낸다. 수신 프레임 전달과 앱 콜백은 하지 않는다. VSS / CIS / BCM은 노드 프로필 표(`src/node/net_node_profile_<node>.c`)를 쓰고, DOMAIN은 조립 루트 `src/node/net_node_domain.c`와 보드 파일 `board/s32k344_domain/`을 쓴다.
- SDK 교체 검사: `tools/sdk_swap_check.py`는 `src/platform/rtd`(와 `net_node.c` / `net_entry.c`)를 S32K344와 S32K144 S32DS 프로젝트의 실제 RTD 헤더에 대고 ARM GCC `-fsyntax-only`로 검사한다(오브젝트·링크 없음, VSS·CIS 프로필). 보드 이름은 생성 설정에서 읽는다(다른 이름만 `net_board_cfg.h`의 -D 덮어쓰기가 된다). 프로젝트는 읽기만 하고, 보고서는 `--out/summary.md`에 쓴다.
- 시험대 도구: `NETWORK_BUILD_BENCH_TOOL=ON`은 Host `vss_bench` CLI를 빌드한다(`tools/vss_bench/README.md`). wire 코덱으로 CAN / UART-C 프레임을 만들고, 그것을 보내는 S32K144 디버그 콘솔 명령을 출력하며, 그 콘솔에서 복사한 프레임을 해독한다.

일반 WINDOW 경로는 HOLD 그대로다. `inspection_transport`를 명시하고 별도 inspection 제출 API를 쓰면, 보류된 wire 데이터를 `held_can` / `held_uart` 관찰 콜백까지만 넘길 수 있다. 이것은 일반 Owner 전달, C2 차량 상태 commit, WINDOW 생존 판단, 구동기 실행을 허락하지 않는다.

## 소유, 시각, 복구

모든 API는 소유자 하나가 직렬로 불러야 한다. `in_call`은 동기 재진입을 거부할 뿐 ISR·스레드 잠금이 아니다. 살아 있는 인스턴스를 복사하거나 다시 초기화하지 않는다. 서비스 내부를 바꾸지 않고, 출력 저장소를 살아 있는 내부 객체와 겹치게 두지 않는다. Port의 제출, 접수, 최종 결과, 참조 해제는 서로 독립된 사실이다. 어느 한 경계의 성공이 상대의 수신이나 차량 실행을 뜻하지 않는다. reset, timeout, 취소 어느 것도 드라이버가 아직 빌려 간 버퍼를 풀지 않는다.

wire 전송 식별과 원 관측 식별은 따로다. 전송 식별이 같은데 원 바이트가 다르면 충돌이다. 원 필드 식별이 같고 전송 순번만 더 새로우면 AGE는 늘 수 있지만 그 필드의 값과 품질은 바꿀 수 없다. 유효한 반복 관측은 receipt 정보만 새로 하고 Owner에 다시 전달하지 않는다. 예외는 두 가지다. 하나는 바뀌지 않은 Vision 평가의 새 UART-C LINK 관측으로, 일부러 설계한 경우다. 다른 하나는 노드 프로필이 고른 주기 경로다(`rx_repeat_delivery`: 수신 감시를 먹이는 VSS/CIS/BCM 수신 콜백). 이때 반복은 원 receipt와 함께 Owner에 간다(`NetUpperDelivery.repeat`). 결과 진행과 독립 lane은 서로 독립이다. 수용한 BOOT가 아닌 송신 BOOT는 CAN ID마다(UART-C는 수신 방향마다) 새 세대를 시작한다. 단, 그 키가 끝낸 BOOT 4개 가운데 하나면 아니다. 끝난 BOOT의 늦은 프레임은 거부한다.

원 AGE는 명시적인 단조 시계와 바뀌지 않는 수신 출처 기록을 쓴다. 시각이 뒤로 가거나 모를 때, 산술 넘침, 풀리지 않은 식별 순환, 정책 없음은 그 동작만 HOLD한다. Permission 원 TTL, RX 끊김, 명령·이벤트 사용 기한은 서로 따로다. Query는 실제 최종 로컬 TX 성공만 셀 수 있다. 이것은 상대 앱의 확인 응답이 아니다.

## 과거 기록과 아직 풀리지 않은 연동

`WIRE_CANDIDATE_STATUS.md`, `WIRE_WAVE2A_STATUS.md`, `WIRE_WAVE2B_STATUS.md`, `BOARD_BINDING_STATUS.md`는 각 이름의 wave에 대한 과거 기록이다. 거기 적힌 코덱 7개 / 27개 / 48개 수, 골격뿐이라는 문장, 예전 시험 총수를 지금 구현 목록으로 읽지 않는다.

G01~G08, WINDOW 실행, 외부 Owner 정책·ABI 채택, BOOT_ID의 보드 출처, CAN TX_SEQUENCE 순환, 실제 드라이버·RTD 시각과 해제 의미, 대상 ECU 전체 RAM·스택·힙, ARM 빌드, Flash, 실제 버스 시험은 해당하는 대로 HOLD/UNKNOWN/NOT_RUN으로 명시해 둔다. HAL과 무관한 코드는 Host 백엔드로 돈다. Host에서의 성공을 실제 보드나 ECU 전체가 맞는다는 주장으로 올리지 않는다.

보드에 올려 실제로 확인한 것(S32K344 VSS 노드, S32K144 BCM 시험대)과 확인하지 못한 것은 `BOARD_TEST_RESULTS.md`에 있다.

BOOT_ID 계산 함수 `net_boot_id_next`(`include/network/net_boot_id.h`, `src/platform/net_boot_id.c`)는 있고 Host 시험도 돈다. HOLD인 것은 보드 쪽이다. warm reset에도 남는 noinit 기록 위치, 리셋 원인 읽기, seed 출처가 아직 없어서, 보드 파일은 지금 고정 시험값을 쓴다. 그래서 보드 시험 중에는 한 노드만 리셋하면 상대가 그 노드의 프레임을 옛 것으로 거부한다. 양쪽을 함께 리셋한다.
