# 노드 통합 폴더 (firmware/integration)

노드마다 보드에서 확인한 빌드 소스를 한 폴더에 모은 곳이다. 각 노드 폴더 하나로 그 노드를 빌드할 수 있다. 원본은 각 노드 브랜치와 Network 브랜치에 있고, 이 폴더는 그 사본이다.

## 1. 노드 목록과 출처

| 폴더 | 노드 | MCU | 출처 | 보드 확인 |
|---|---|---|---|---|
| `CIS/` | CIS (Network 프로필 1, CAN FD + UART-C) | S32K144 | CIS: `feat/cis-rtd-network` `129ef58` / Network: `feat/vss-network-node` `02051db` | 2026-10-29, S32DS 프로젝트 `network_144_cis_app` |
| `BCM/` | BCM (Network 프로필 3, CAN FD) | S32K144 | BCM 원본(git 저장소가 아님, 판은 `SOURCES.txt` 해시) / BCM 통합 작업 폴더 `bcm_net`(git 저장소가 아님) / Network: `feat/vss-network-node` `02051db` | 2026-10-29, S32DS 프로젝트 `network_144_bcm_app` |

두 노드 모두 상대는 S32K344 디버그 노드(DOMAIN 역할)였다. VSS와 DOMAIN 노드의 통합 폴더는 아직 없다.

## 2. 폴더 구성 규칙

각 노드 폴더는 같은 모양을 따른다.

```
firmware/integration/<CIS|BCM>/
  README.md          구성, 빌드 결과, 다른 IDE에 옮길 설정, 확인하지 않은 것, 다시 만드는 법
  ISSUES.md          보드 시험에서 찾은 결함
  SOURCES.txt        출처와 파일별 SHA-256(CRLF를 LF로 읽은 값)
  s32ds/<노드>144_board_tested.mex   보드에서 확인한 ConfigTools 설정
  src/
    main.c
    <노드 앱>/       CIS는 cis/, BCM은 bcm/
    net_bench/       Network 시험대 시계와 hook(net_board_time144.c/.h)
    network/         Network 스택 사본(노드 프로필), NETWORK_BUNDLE.txt
```

- **Network 사본은 노드 폴더 안의 `src/network/` 하나뿐이다.** 노드 사이에 공유하는 Network 사본을 따로 두지 않는다. 노드마다 프로필이 달라 들어가는 파일이 다르다.
- `src/network/`는 `sync_to_s32ds.py --node <노드>`로 만든 생성 사본이다. 손으로 고치지 않는다.
- 결함은 코드를 고치지 않고 `ISSUES.md`에 남긴다. 코드는 보드에서 확인한 판 그대로 둔다.

## 3. 다시 만드는 법

**통합 폴더의 파일은 직접 고치지 않는다.** 원본(노드 브랜치, Network 브랜치)에서 고친 뒤 다시 만든다. 자세한 절차는 각 노드 README 6절에 있다.

이 브랜치에는 원본이 없다. 원본을 따로 체크아웃하고, 도구는 원본 체크아웃의 것으로 실행하며, 대상만 이 브랜치의 체크아웃(`<통합 체크아웃>`)을 가리킨다.

| 원본 | 위치 |
|---|---|
| Network | `feat/vss-network-node` 브랜치의 `firmware/network`(`02051db`). 체크아웃 위치를 `<Network 체크아웃>`이라 한다 |
| CIS 앱 | `feat/cis-rtd-network` 브랜치의 `firmware/CIS`(`129ef58`). 체크아웃 위치를 `<CIS 체크아웃>`이라 한다 |
| BCM 앱 | 받은 BCM 폴더(`<BCM 폴더>`, git 저장소가 아님)와 BCM 통합 작업 폴더(`<bcm_net>`, git 저장소가 아님). 판은 `BCM/SOURCES.txt` 해시로 가린다 |

1. Network 사본(출력 폴더 이름은 `network`):
   ```
   python <Network 체크아웃>/firmware/network/tools/sync_to_s32ds.py --node <CIS|BCM> --check <통합 체크아웃>/firmware/integration/<CIS|BCM>/src/network
   python <Network 체크아웃>/firmware/network/tools/sync_to_s32ds.py --node <CIS|BCM> --out <통합 체크아웃>/firmware/integration/<CIS|BCM>/src/network --apply
   ```
   `--check`의 마지막 줄이 `OK`면 같고, `STALE`면 원본이 바뀌었으니 다시 만들고, `EDITED`면 누가 손으로 고쳤으니 원본에서 고친 뒤 다시 만든다. `--apply`는 Network 체크아웃이 커밋된 상태(clean)에서만 되고, 그 커밋이 `NETWORK_BUNDLE.txt`에 기록된다.
2. 노드 앱, `main.c`, 시계 파일: 각 노드 README 6절대로 위 원본에서 복사한다.
3. `SOURCES.txt`의 커밋과 해시를 갱신한다.

## 4. 넣지 않는 것

- **NXP 파일:** RTD 드라이버(`RTD/`), ConfigTools 생성 설정(`generate/`, `board/`), 시작 코드와 링커 스크립트(`Project_Settings/`). 각 IDE에서 RTD 설치본(S32K1 RTD 3.0.0)과 ConfigTools로 만든다. ConfigTools 설정 파일(`.mex`)은 넣는다.
- IDE 파일과 빌드 산출물(`.cproject`, `.project`, `Debug_FLASH/`).
- 디버그 노드 파일(`debug_*.c/.h`)과 Network 시험대 앱(`*_bench_app.c/.h`).
- 압축 찌꺼기(`__MACOSX/`, `.DS_Store`).

## 5. SysTick 소유

- **`SysTick_Handler`는 각 노드의 `src/net_bench/net_board_time144.c`만 정의한다.** 1 kHz ms 카운터가 Network 시각(`Board_TickMs32`)이고, `Board_Us32`가 µs 카운터다. 코어 클럭은 FIRC 48 MHz(`net_board_time144.h`의 `BOARD_CORE_CLK_HZ`)다.
- 시작 코드의 weak 정의는 덮인다. `OsIf_Init()`을 부르지 않는다(OsIf가 SysTick을 다시 설정한다). 생성 설정의 `OSIF_USE_SYSTEM_TIMER`는 `STD_OFF`다.
- 노드 앱은 SysTick을 쓰지 않는다. BCM 원본의 SysTick 코드(`bcm_port_rtd.c`)는 통합 폴더에 넣지 않았다.
- RTOS처럼 SysTick을 따로 쓰는 환경으로 옮기면 시간원(`Board_*` hook)을 다시 정해야 한다.

## 6. CAN 비트 타이밍 (버스의 모든 노드가 맞춰야 할 값)

CAN FD ISO, BRS, FlexCAN PE 클럭 48 MHz, payload 64 B, `max_num_mb` 7, 자동 bus-off 복구 켬.

| 단계 | 속도 | 분주 | prop | seg1 | seg2 | SJW | tq | 샘플 포인트 |
|---|---|---|---|---|---|---|---|---|
| 공칭 | 500 kbit/s | 6 | 7 | 6 | 2 | 2 | 16 | 87.5 % |
| 데이터 | 2 Mbit/s | 2 | 4 | 4 | 3 | 2 | 12 | 75 % |

| 노드 | 설정 | 위 값과 |
|---|---|---|
| CIS | `CIS/s32ds/cis144_board_tested.mex` | 같다(보드 확인) |
| BCM | `BCM/s32ds/bcm144_board_tested.mex` | 같다(보드 확인). CIS `.mex`와 FlexCAN 설정 47개 항목이 모두 같다 |
| S32K344 디버그 노드(DOMAIN 역할) | 디버그 노드 프로젝트 | 같은 값으로 두 노드와 확인했다 |
| `feat/cis-rtd-network` 브랜치의 새 프로젝트용 `firmware/CIS/s32ds/cis_node.mex` | 공칭 분주 6, prop 7, seg1 5, seg2 3(81.25 %) / 데이터 분주 1, prop 14, seg1 4, seg2 5(79 %) | **다르다.** 버스에 올리기 전에 위 값으로 맞춘다 |
| VSS, DOMAIN 실제 노드 | 각 노드 프로젝트 | 확인하지 않았다. 위 값과 같아야 한다 |

- 두 보드만 이은 짧은 버스 기준이다. 데이터 단계 TDC 설정이 없다. 노드가 늘거나 버스가 길어지면 다시 본다.

## 7. 알려진 결함 요약

| 노드 | 항목 | 상태 | 요약 |
|---|---|---|---|
| BCM | BCM-1 실내등 PWM duty 단위 | 열림 | duty 100 %를 0x8000으로 계산하지만 RTD `Ftm_Pwm_Ip`는 주기 tick(8000)으로 받는다. 1~24 %는 약 4배로 들어가고, 25 % 이상은 RTD가 거절해 반영되지 않는다. 값은 BCM 원본에서 왔다 |
| BCM | BCM-2 실내등 적용 결과가 늘 성공 | 열림 | `BcmPort_LightApply()`가 늘 true라, 출력이 바뀌지 않아도 결과 DONE과 상태 APPLIED를 보고한다 |
| CIS | CIS-1 DHT22 76 °C | 열림 | 76.30 °C, 7.30 %RH가 체크섬 통과·VALID로 나왔다. 원인 미확인(센서 발열 추정) |

자세한 근거와 고칠 안, 확인 방법은 각 노드 폴더의 `ISSUES.md`에 있다.
