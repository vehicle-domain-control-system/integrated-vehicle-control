# CIS 앱을 Network 시험 프로젝트(S32K144)에 붙이기

Network 담당자가 만든 S32K144 CIS 시험대(`nodes/cis/bench/s32k144_cis`, 보드 시험 통과: `docs/BOARD_TEST_RESULTS.md`)를
그대로 쓰고, 시험대 앱(`cis_bench_app.c`) 자리에 이 CIS 앱을 넣는다. 담당자 파일은 고치지 않는다.

## 구성

| 맡는 쪽 | 내용 |
|---|---|
| Network (담당자) | CAN FD(FlexCAN0)·UART-C(LPUART1)·시계(SysTick 1 kHz, 코어 FIRC 48 MHz)·`Board_*` hook·`BOOT_ID`. 복제본 프로젝트 `network_144_net_cis`의 RTD 설정 |
| CIS (우리) | 센서 3종(HC-SR04, DHT22, VEML7700), Pi 판정(`vision_logic`), 후방 허용(`power_gate`), `CisNet_Publish*` 제출, `CisApp_On*` 수신 |

우리 센서 드라이버는 레지스터를 직접 만진다(RTD API 없음). 핀 먹스는 RTD Port 초기화가 끝난 뒤 드라이버가 다시 설정한다.

## 1. 프로젝트 만들기와 소스 복사

1. 담당자의 144 디버그 노드 프로젝트를 복제한다(예: `network_144_net_cis`). 원본 `network_144_can_debuger`는 읽기 전용이다.
2. 담당자 브랜치(`feat/vss-network-node`)를 별도 폴더로 체크아웃한다(읽기 전용).
3. 복사:
   ```
   python firmware/CIS/tools/sync_cis_to_s32ds.py --network <NET_git>/firmware/network --project <network_144_net_cis>          # dry run
   python firmware/CIS/tools/sync_cis_to_s32ds.py --network <NET_git>/firmware/network --project <network_144_net_cis> --apply
   ```
   - 담당자 도구(`--board cis144`)가 Network 소스를 `src/network/`, 시계 파일을 `src/net_bench/`에 넣는다.
   - 시험대 앱 `cis_bench_app.c/.h`는 이 스크립트가 지운다(우리 `CisApp_On*`와 이름이 겹친다).
   - 우리 소스는 `src/cis/`로 들어간다(`main.c`, `pi_uart.c`, `net_api_shim.h` 제외).

## 2. `.cproject` (Debug_FLASH C 컴파일러)

담당자 시험대와 같은 include 경로 7개와 define 11개에, 우리 폴더만 더한다.

include 경로에 추가:
```
${ProjDirPath}/src/cis
```

define에 추가:
```
CIS_USE_NETWORK_API
```
(나머지 11개는 `nodes/cis/bench/s32k144_cis/README.md` 그대로: `NETWORK_UPPER_NODE_PROFILE=1`, `NET_NODE_BUILD`, `NET_BOARD_*_HOOK`,
FlexCAN·LPUART 콜백 이름, `NET_BOARD_UART_C_INSTANCE=1U` 등. 생성 설정과 이름이 다르면 그 define 값만 맞춘다.)

`CIS_USE_NETWORK_API`를 켜면 우리 쪽 UART 구동과 `P_PERMISSION` 송신, 벤치 보고(0x70)가 꺼지고 Network 스택이 LPUART1을 쓴다.

## 3. `main()` 수정 (프로젝트 쪽, 저장소에는 복사하지 않음)

담당자 시험대의 `NET_NODE_BUILD` 경로에서 시험대 앱 호출만 바꾼다.

```c
#include "cis_node.h"

/* ... Clock, Port, IntCtrl 초기화 ... */
Board_TimeInit();
(void)CisNode_Init();          /* 센서 시작 + Net_Init() ; CisBench_Init() 자리 */
for (;;) {
    CisNode_Service();         /* Net_MainFunction + CIS 기능 + CisNet_Publish* ; CisBench_Service() 자리 */
}
```

`Net_Init()`은 `CisNode_Init()` 안에서 부른다(담당자 순서와 같다: `Board_TimeInit` 다음).

## 4. 확인할 것 (S32DS)

- 컴파일/링크: `src/cis`의 17개 `.c`와 `src/network`가 같이 빌드되는지. 이름이 겹치면 로그를 보고 맞춘다.
  - 우리 `regs_s32k144.h`의 매크로 이름이 RTD/디바이스 헤더와 겹치면 에러가 난다.
  - `SysTick_Handler`는 `net_board_time144.c`에만 있어야 한다(우리 코드에는 없음).
- SOSC: 클럭 설정이 이미 SOSC를 켜 두었으면 `timebase_init()`은 SOSCDIV1만 켠다(`SOSCDIV1`이 0이면 1로). 끝나도 `false`면 FTM0 1 MHz가 없다는 뜻이다.
- 핀: HC-SR04 TRIG PTE7 / ECHO PTE8, DHT22 PTA14, VEML7700 SDA PTA2 / SCL PTA3. 디버그 노드 Port 설정이 이 핀을 다른 용도로 쓰고 있지 않은지 확인한다.
- 시간 예산: `g_net_diag.main_max_us`(Net 1회 최대)가 15 ms 미만이고, DHT22 읽기(약 5 ms)와 겹쳐도 UART 링(256 B, 약 22 ms) 안인지.
  DHT22 읽기 중 인터럽트는 막지 않는다. 비트 시각이 흐트러지면 체크섬이 틀려 그 회차만 실패로 세고(연속 3회 실패해야 고장), 2초 뒤 다시 읽는다.

## 5. 달라지는 점

- `BOOT_ID`, 시계, 복사 도구는 담당자 것을 쓴다. 이 저장소의 `firmware/CIS/board/`와 옛 복사 스크립트는 없앴다.
- Pi 쪽(`tools/pi_vision`)은 그대로다. 담당자의 `tools/pi_bench`는 노트북 시험 도구이고, 실제 Pi는 우리 스크립트가 맡는다. 프레임 형식(29/26/25 B)은 같다.
