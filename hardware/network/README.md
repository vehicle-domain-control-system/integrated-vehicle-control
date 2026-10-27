# Network 보드 설정 계획 (초안)

- **상태:** 초안(2026-10-28). 평가 보드 시험에서 쓴 설정을 바탕으로 정한 예정값이다. 회로도와 실제 차량 하드웨어로 확정하지 않았다.
- **범위:** Network가 쓰는 CAN·UART의 핀, 통신 설정, 보드마다 ConfigTools와 프로젝트 설정에 넣을 것.
- **관리:** Network 담당이 고친다. 핀이나 인스턴스를 바꿀 때는 이 문서를 먼저 고치고 알린다.
- **메시지 정의와 주기:** 이 문서 범위 밖이다(`docs/network`).

## 1. 노드와 링크

| 노드 | MCU | CAN | UART | 지금 상태 |
|---|---|---|---|---|
| DOMAIN | S32K344 | FlexCAN0 | UART-M: ESP32와 연결 | 보드 연결 전 |
| VSS | S32K344(VSS 오디오 보드) | FlexCAN0 | 없음 | 보드 시험 통과 |
| CIS | S32K144 | FlexCAN0 | UART-C: Pi와 연결 | 평가 보드 시험 통과 |
| BCM | S32K144 | FlexCAN0 | 없음 | 평가 보드 CAN 시험 |
| WINDOW | 미정 | 예정 | 없음 | 설계만 |

ESP32는 DOMAIN과 UART-M으로만, Pi는 CIS와 UART-C로만 잇는다. 둘 다 CAN 버스에는 붙지 않는다.

## 2. CAN 버스 (모든 노드 공통)

버스에 붙는 모든 노드가 같은 값을 써야 한다. 지금 평가 보드 세 종류가 모두 이 값으로 통신한다.

| 항목 | 값 |
|---|---|
| 방식 | CAN FD(ISO), BRS 켬, 표준 ID(11비트) |
| 공칭 비트레이트 | 500 kbit/s, 샘플점 87.5 % |
| 데이터 비트레이트 | 2 Mbit/s, 샘플점 75 % |
| 페이로드 | 64 B(메시지 버퍼 크기) |
| bus-off | 컨트롤러 자동 회복 켬. Network는 bus-off 뒤 CAN 링크를 100 ms(이어지면 1 s) 닫았다가 다시 연다 |
| 종단 | 버스 양 끝에만 120 Ω(노드를 늘릴 때 보드 종단 위치 확인) |

비트 타이밍은 ConfigTools에 넣는 값이다. CAN 클럭이 다르면 분주만 바꿔 공칭 16 Tq, 데이터 12 Tq를 유지한다.

| CAN 클럭 | 공칭 | 데이터 |
|---|---|---|
| 48 MHz(S32K144) | prescaler 6, prop 7, phase1 6, phase2 2, SJW 2 | prescaler 2, prop 4, phase1 4, phase2 3, SJW 2 |
| 24 MHz(S32K344, FIRC 48 MHz ÷ 2) | prescaler 3, 나머지 같음 | prescaler 1, 나머지 같음 |

생성 파일의 레지스터 값은 이 값과 다르게 보이므로(대부분 1 작음), 생성 파일 값을 ConfigTools에 옮겨 넣지 않는다.

## 3. UART 링크

| 링크 | 양 끝 | 설정 |
|---|---|---|
| UART-C | CIS(S32K144 LPUART1) ↔ Pi | 115200 bit/s 8N1, 흐름 제어 없음 |
| UART-M | DOMAIN(S32K344 LPUART11, 초안) ↔ ESP32 | 115200 bit/s 8N1(시작값), 흐름 제어 없음 |

- **드라이버 방식:** MCU 쪽은 인터럽트 방식이다(DMA 아님).
- **실제 baud:** S32K144(LPUART 클럭 8 MHz)와 S32K344 LPUART11(40 MHz)은 115,942 bit/s(+0.64 %)다. CIS 시험에서 115,200 상대와 수신 오류 0이었다.
- **상대 쪽(Pi, ESP32):** 115200 8N1, 흐름 제어 없음, 3.3 V TTL로 맞춘다.

## 4. 보드마다 설정할 것

1. **Port:** 5절의 CAN·UART 핀과 트랜시버 켜기 GPIO.
2. **Clock:** FlexCAN0·LPUART 클럭 게이트를 켠다. CAN 클럭은 2절 표 중 하나와 맞춘다.
3. **FlexCAN0:**
   - 2절 값, 페이로드 64 B, 5절의 `max_num_mb`
   - 인터럽트 방식, legacy RX FIFO 끔, bus-off 자동 회복 켬
   - 이벤트·오류 콜백 이름 두 개. Network HAL이 그 이름의 함수를 정의하므로 같은 이름의 다른 함수를 두지 않는다
4. **LPUART(UART를 쓰는 노드만):** 3절 값, 인터럽트 방식, 콜백 이름 하나.
5. **IntCtrl:** FlexCAN0 인터럽트와 LPUART 인터럽트를 켠다. UART를 쓰는 노드는 LPUART 우선순위를 CAN보다 같거나 높게 둔다(초안, CIS와 같은 방식).
6. **Network 소스 넣기:** `firmware/network/tools/sync_to_s32ds.py`로 복사한다.
7. **프로젝트 설정(`.cproject`):** include 경로, 노드 프로필 define, 보드 hook은 노드 연동 문서를 따른다. 생성 설정 이름이 Network 기본값과 다르면 `-D`로 맞추고, Network 소스는 고치지 않는다.

## 5. 보드별 값

| 항목 | DOMAIN | VSS | CIS | BCM |
|---|---|---|---|---|
| MCU | S32K344 | S32K344(VSS 오디오 보드) | S32K144 | S32K144 |
| 프로필 define | `NETWORK_UPPER_NODE_PROFILE=5` | `=6` | `=1` | `=3` |
| CAN RX / TX 핀 | PTA6 / PTA7 | PTA6 / PTA7 | PTE4 / PTE5 | PTE4 / PTE5 |
| CAN 트랜시버 켜기 | PTG5 출력(초안) | PTG5 출력 | 없음(평가 보드는 GPIO 없이 동작) | 없음(평가 보드는 GPIO 없이 동작) |
| CAN 클럭 | 24 MHz | 24 MHz | 48 MHz | 48 MHz |
| `max_num_mb` | 16 | 16 | 7(64 B에서 하드웨어 상한) | 7 |
| MB 배치(Network 기본값) | RX 0~3, TX 8~11 | RX 0~3, TX 8~9 | RX 0~1, TX 2~5 | RX 0~1, TX 2~5 |
| UART | UART-M: LPUART11, RX PTC11 / TX PTC10(초안) | 없음 | UART-C: LPUART1, RX PTC6 / TX PTC7 | 없음 |
| UART define | `NET_BOARD_UART_M_INSTANCE=11U`, `NET_BOARD_UART_M_CONFIG=Lpuart_Uart_Ip_xHwConfigPB_11` | — | `NET_BOARD_UART_C_INSTANCE=1U`, `NET_BOARD_UART_C_CONFIG=Lpuart_Uart_Ip_xHwConfigPB_1` | — |
| 인터럽트 우선순위(지금) | 미정 | 오디오(PIT·DMA·SAI) 0, CAN 8 | LPUART 5, CAN 6 | CAN 6 |
| 연동 문서 | [DOMAIN](../../firmware/network/nodes/domain/DOMAIN_NODE_INTEGRATION.md) | [VSS](../../firmware/network/nodes/vss/VSS_NODE_INTEGRATION.md) | [CIS](../../firmware/network/nodes/cis/CIS_NODE_INTEGRATION.md) | [BCM](../../firmware/network/nodes/bcm/BCM_NODE_INTEGRATION.md) |

- **DOMAIN 핀:** S32K344 평가 보드에서 Network가 쓴 핀이다. UART-M LPUART11(PTC10/PTC11)은 그 보드에서 115200으로 실제 통신한 경로라 초안으로 정했다. DOMAIN 보드가 다른 모델이면 다시 정한다.
- **MB 배치:** `NET_BOARD_CAN_*` define으로 바꿀 수 있다. 바꾸면 Network가 컴파일할 때 겹침과 `max_num_mb` 초과를 검사한다.
- **CIS 실제 보드:** 계획(`feat/cis-rtd-network`의 `firmware/CIS/board/README.md`)도 CAN0 PTE4/PTE5, LPUART1 PTC6/PTC7로 시험대와 같다.

## 6. 미정·확인 필요

- [ ] **트랜시버 켜기 핀:** S32K344 PTG5(CAN0)·PTG10(CAN1로 보임)의 극성과 기능을 회로도로 확인한다. 지금은 초기 출력 Low에서 통신한다.
- [ ] **DOMAIN 보드:** 실제 보드 모델, UART-M 핀(초안 LPUART11), 인터럽트 우선순위.
- [ ] **전압 레벨:** S32K144 보드 I/O 전압이 5 V면 Pi·ESP32(3.3 V)와 바로 잇지 않는다. 보드 전압 설정을 확인한다.
- [ ] **CIS↔Pi 배선:** 지금 시험은 S32K144 평가 보드의 OpenSDA 가상 COM(LPUART1)을 쓴다. 실제 Pi를 PTC6/PTC7에 이을 때 OpenSDA UART 연결과 겹치지 않는지 확인한다.
- [ ] **버스 종단:** 노드가 늘 때 120 Ω을 어느 보드에 둘지 정한다.
- [ ] **WINDOW:** MCU와 핀.
