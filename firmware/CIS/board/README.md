# CIS 보드 프로젝트에 Network 붙이기 (S32K144, RTD)

Network 담당자의 S32K344(VSS) 연결과 같은 방식이다. Network의 RTD HAL(`src/platform/rtd`)이 RTD로 생성한
FlexCAN·LPUART 설정을 쓰고, 이 폴더의 파일이 노드 설정 hook과 시간 hook을 준다. Network 소스는 담당자의 브랜치
(`feat/vss-network-node`)에서 **읽기만** 한다.

레포에는 NXP 파일(RTD, SDK, `.mex`, 생성 소스)을 넣지 않는다. 아래 1~2번은 S32DS가 있는 Windows PC에서 한다.

## 1. S32DS에서 RTD 프로젝트 만들기

S32K144 / S32K1 RTD 3.0.0 프로젝트를 새로 만들고 ConfigTools에서 다음을 설정한다.

| 구성요소 | 설정 |
|---|---|
| Clock | 코어 클럭을 정해서 `CIS_BOARD_CORE_HZ`와 맞춘다(RUN 모드 80 MHz면 80000000). SOSC 8 MHz는 켜 둔다. FlexCAN 클럭 소스로 500 kbps / 2 Mbps가 나오는지 비트 타이밍 계산기로 확인한다 |
| Port | CAN0 RX = PTE4, TX = PTE5 (**회로도로 확인할 것**), LPUART1 RX = PTC6, TX = PTC7. 센서 핀은 현재 배선 유지: HC-SR04 TRIG PTE7 / ECHO PTE8, DHT22 PTA14, VEML7700 SDA PTA2 / SCL PTA3 |
| FlexCAN | 인스턴스 0, CAN FD, 공칭 500 kbps / 데이터 2 Mbps(BRS), 페이로드 64 B. 64 B 페이로드에서는 메시지 버퍼가 **7개**(`max_num_mb`). 컨트롤러 콜백과 에러 콜백 이름을 `Can0_BringupCallback`, `Can0_BringupErrorCallback`처럼 하나 정해 둔다(HAL이 같은 이름의 함수를 정의한다. 같은 이름을 쓰는 다른 파일을 두지 않는다) |
| LPUART | 인스턴스 1, 115200 8N1, 인터럽트 방식. 콜백 이름은 하나 정한다(HAL이 정의한다). 콘솔 등 다른 LPUART이 같은 콜백을 쓰면 라우팅 모드(`NetHalRtdUart_OnEvent`)를 쓴다 |
| 인터럽트(IntCtrl) | FlexCAN0와 LPUART1 인터럽트 활성화 |
| OsIf / Lpi2c / Gpio / Ftm 입력 캡처 | 센서 드라이버를 RTD로 옮길 때 사용(아직 코드 없음, 아래 4번) |

생성한 뒤 다음 이름을 알려주면 `-D` 값에 반영한다: FlexCAN 설정/상태 심볼, LPUART 설정 심볼, 두 콜백 이름, `max_num_mb`.

## 2. 소스 복사

```
python firmware/CIS/tools/sync_network_to_s32ds.py --network <NET_git>/firmware/network --project <CIS RTD 프로젝트>          # dry run
python firmware/CIS/tools/sync_network_to_s32ds.py --network <NET_git>/firmware/network --project <CIS RTD 프로젝트> --apply
```

- Network 소스는 `src/network/`, 이 폴더의 `.c/.h`는 `src/net_board/`로 들어간다.
- 모의 SDK, 시험, 도구, `s32k144` 뼈대, RTD 이름의 대역 헤더는 복사하지 않는다(실제 RTD 헤더를 가리지 않도록).
- 우리 CIS 소스(`firmware/CIS/src`)도 `src/`에 넣는다. 레지스터를 직접 만지는 구 드라이버(`timebase.c`, `hcsr04.c`, `dht22.c`, `veml7700.c`, `pi_uart.c`, `regs_s32k144.h`)는 RTD 프로젝트와 충돌하므로 **넣지 않는다**.

## 3. `.cproject` (Debug_FLASH C 컴파일러)

include 경로:

```
${ProjDirPath}/src/network/include
${ProjDirPath}/src/network/src/l04_frame_codec
${ProjDirPath}/src/network/src/l02_typed_handoff/m03_typed_handoff
${ProjDirPath}/src/network/src/l05_transport_port/m07_buffer_outcome
${ProjDirPath}/src/network/src/l05_transport_port/m09_observation
${ProjDirPath}/src/network/src/l03_message_service/m04_message_service
${ProjDirPath}/src/network/src/platform/rtd
${ProjDirPath}/src/net_board
```

define:

```
NETWORK_UPPER_NODE_PROFILE=1
NET_NODE_BUILD
CIS_USE_NETWORK_API
NET_BOARD_NOW_MS_HOOK=CisBoard_NowMs
NET_BOARD_NODE_CONFIG_HOOK=CisBoard_NetNodeConfig
NET_BOARD_CAN_MAX_MB=7U
NET_BOARD_CAN_RX_MB_COUNT=2U
NET_BOARD_CAN_TX_MB_FIRST=2U
NET_BOARD_UART_C_INSTANCE=1U
NET_BOARD_UART_C_CONFIG=Lpuart_Uart_Ip_xHwConfigPB_1
```

생성 설정의 실제 이름이 다르면 `NET_BOARD_CAN_CONFIG`, `NET_BOARD_CAN_STATE`, `NET_BOARD_CAN_CALLBACK`, `NET_BOARD_CAN_ERROR_CALLBACK`,
`NET_BOARD_UART_CALLBACK`(또는 라우팅 모드)도 같은 방식으로 더한다. 기본값과 의미는 `net_board_cfg.h` 주석을 따른다.
(위 define 조합은 Network의 모의 SDK로 `net_hal_rtd*.c`, `net_entry.c`를 문법 검사한 것이며, 실제 RTD 헤더로는 Windows에서 컴파일해 확인해야 한다.)

## 4. `main.c` 호출 순서

```c
/* RTD: Clock_Ip_Init, Port_Ci_Port_Ip_Init, IntCtrl 등 생성 초기화 */
CisBoard_TimeInit();           /* SysTick 1 kHz */
cis_app_init();
cis_net_adapter_init();        /* 안에서 Net_Init() */
for (;;) {
    uint32_t now_ms = CisBoard_NowMs32();
    cis_net_adapter_poll(now_ms);   /* Net_MainFunction: 수신 콜백이 이 안에서 실행된다 */
    cis_app_run(now_ms);
    cis_net_adapter_run(now_ms);    /* CisNet_Publish* */
}
```

센서 드라이버(HC-SR04 입력 캡처, DHT22, VEML7700 I2C)를 RTD API로 옮기는 일은 생성된 RTD 헤더의 함수 이름을 보고 해야 한다
(`firmware/CIS/src/hw_rtd/`, 아직 없음).

## 알아 둘 점

- `net_node.c`는 플랫폼 이름표를 S32K344로 붙이지만 이 값은 라벨일 뿐 동작에 쓰이지 않는다. 담당자에게 확인할 것.
- `BOOT_ID`: 재부팅마다 달라야 한다. 지금은 `.noinit` 카운터(리셋마다 증가, 전원을 껐다 켜면 시드 값)이며 제품용이 아니다. 프로젝트 링커 스크립트에 `.noinit`(NOLOAD)이 없으면 추가한다.
- Network 한 바퀴(`Net_MainFunction`)는 15 ms를 넘지 않아야 하고, UART 수신 링(256 B)은 약 22 ms 분량이다. DHT22 읽기(약 7 ms)가 그 안에 들어오는지 확인한다.
- Network 모드에서는 LPUART1을 Network가 쓰므로 벤치 보고(`P_BENCH_SENSORS`)와 구 `pi_uart.c`는 쓸 수 없다.
