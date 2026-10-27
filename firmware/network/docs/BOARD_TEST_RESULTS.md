# 보드 시험 결과 (2026-10-25~27)

Network를 실제 보드에 올려 확인한 결과를 요약한다. Host 시험과는 별개다. 관측값 전체는 이 저장소 밖의 시험 기록에 있고, 여기에는 판정과 핵심 수치만 둔다. 여기 없는 항목을 보드에서 확인했다고 읽지 않는다(아래 "확인하지 못한 것").

## 시험 구성

| 시험 | 보드 | Network 커밋 | 보드에 올린 앱 | 상대 노드 |
|---|---|---|---|---|
| VSS 노드 | S32K344 VSS 보드(S32DS 프로젝트 `S32K344_WB_Audio_Bringup`), VSS 프로필 | `342df4f` | VSS 펌웨어(`feat/vss-firmware` `8fcf25c`). 연동 절차의 빌드 설정 외에 음량을 100 %로 올렸다(저장소 기본 70 %) | S32K144 CAN 디버그 노드(2026-10-20판, 메시지 정의 기준 `6274b050`). 콘솔 명령으로 DOMAIN 프레임을 보낸다 |
| BCM 노드 | S32K144(디버그 노드 프로젝트의 복제본 `network_144_net_bench`), BCM 프로필 | `e173cfd`(복사 원본 작업 트리) | 시험대 앱 `nodes/bcm/bench/s32k144_bcm/bcm_bench_app.c`(실제 BCM 앱 없음) | 위 S32K344 VSS 보드(같은 이미지) |
| CIS 노드 | S32K144(디버그 노드 프로젝트의 복제본 `network_144_net_cis`), CIS 프로필 | `5e9ff0c`(아래 "CIS 시험에서 고친 결함"의 두 수정 뒤) | 시험대 앱 `nodes/cis/bench/s32k144_cis/cis_bench_app.c`(실제 CIS 앱 없음) | DOMAIN 역할: S32K344로 옮긴 CAN 디버그 노드(2026-10-20판, 메시지 정의 기준 `6274b050`). Pi 역할: 노트북 도구 `tools/pi_bench/`(UART-C, S32K144 평가 보드의 OpenSDA 가상 COM 포트, 115200 8N1) |

- 버스: 두 보드만 연결한 CAN FD(ISO, 500 kbit/s / 2 Mbit/s, BRS). CIS 시험도 같은 S32K144 생성 설정으로 두 보드만 연결했다.
- BOOT_ID: Network 노드(S32K344 VSS, S32K144 BCM·CIS)의 BOOT_ID는 보드 상수다(보드 출처 HOLD). BCM 시험은 처음부터 다시 할 때 두 보드를 함께 리셋했다. VSS 시험의 상대인 디버그 노드는 명령마다 BOOT를 정해 보내므로, VSS 보드만 리셋하며 진행했다. CIS 시험은 CIS 보드만 리셋했다. 상대(디버그 노드, Pi 도구)는 Network 노드가 아니어서 순번이 처음으로 돌아간 CIS 프레임을 버리지 않고 센다.
- 관찰: S32DS 디버거의 Expressions로 읽었다. VSS 시험은 `'net_node.c'::service.observation`, `permission.view`, `net_hal_rtd_core.diag`, VSS 관찰 변수, BCM 시험은 `g_net_diag`(`NetNode_GetDiag` 사본, 100 ms마다 갱신), `net_hal_rtd_core.diag`, `g_bcm_bench`다. 값은 보드를 멈춰서 읽었고, 경과 시간은 보드 자신의 카운터(VSS 상태 제출 수 × 250 ms, 시험대 ms 카운터)로 쟀다. CIS 시험은 `g_net_diag`, `g_cis_bench`, `'net_node.c'::service.uart_rx[0]`이다. 상대 쪽은 S32K344 콘솔(받은 CIS 프레임의 간격 최소/평균/최대, 빠짐·중복·역행 수, 프레임 필드)과 Pi 도구 기록(보낸 프레임과 바이트, 받은 P_PERMISSION의 순번·간격·내용)이다.
- 판정: 기대값과 대조해 통과 / 부분 통과 / 해당 없음으로 적는다. VSS·BCM 시험에서 실패는 없었다. CIS 시험에서는 Network 결함 두 가지를 찾아 고친 뒤 처음부터 다시 했다(아래 "CIS 시험에서 고친 결함"). CIS의 판정은 고친 뒤의 실행이다.

## VSS 노드 (S32K344)

### 빌드

| 확인 | 결과 |
|---|---|
| 동기화 | `tools/sync_to_s32ds.py --board vss`로 복사. 용량·ISR 링·MB 배치는 기본값(define 덮어쓰기 없음) |
| SDK 교체 검사 | `tools/sdk_swap_check.py` 통과 |
| 빌드 | 빌드 확인 통과(경고 없음, 사용자 보고) |
| RAM(Network + RTD 백엔드) | 11,408 B, VSS 상한 11,776 B 안 |
| 스택(정적 추정) | main 경로 1,912 B, 인터럽트 포함 2,168 B |

### 동작

| 확인 | 방법 | 결과 | 판정 |
|---|---|---|---|
| 기동 | 리셋 뒤 실행 | `Net_Init` 결과 1, VSS 기동 단계 끝(6), RTD 시작, bus-off 0 | 통과 |
| VSS_STATUS(0x340) 주기 | 상대 노드가 받은 프레임의 수신 시각과 TX_SEQUENCE | 프레임당 249.8~249.9 ms. 세 번 읽은 구간(28프레임)에서 TX_SEQUENCE와 수신 번호가 같이 늘었고(손실 없음) UPDATE_SEQUENCE = TX_SEQUENCE(Network가 같은 평가를 반복 송신하지 않는다) | 통과 |
| VSS_WARNING_STATE(0x0C0, 60 B) 한 프레임 | `vss_bench`가 만든 바이트를 상대 노드가 송신 | VSS 전달 1, 검증 실패 0, 그 BOOT로 수신 세대를 열었다. 0x340을 몇 분 보내는 동안 자기 프레임 수신은 0 | 통과 |
| VSS_EVENT(0x160) 새 발생·재전송 | 새 발생 → 같은 바이트 두 번 → 다음 발생 | 수신 4, 전달 2, 중복 2(Network에서 버림). 소리는 새 발생 두 번만 났다 | 통과 |
| DOMAIN_PERMISSION(0x080) 한 프레임 | 한 번 보내고 몇 초 뒤 감독 상태를 읽음 | 원본 바이트 일치. 사유 SOURCE_EXPIRED, 원 AGE 7,666 ms = 보낸 AGE 10 + 경과 7,656. 코드상 감독은 원 AGE 500 ms 이상을 수신 끊김(150 ms)보다 먼저 보므로, 받은 직후 CURRENT, 150 ms 뒤 RX_TIMEOUT, 약 490 ms 뒤 SOURCE_EXPIRED로 바뀐다(보드에서는 마지막 상태만 읽었다). VSS 앱은 0x080을 쓰지 않아 Network 감독만 확인했다 | 통과 |
| DOMAIN 재부팅(BOOT 변경) | 옛 BOOT 프레임 → 새 BOOT 프레임 → 옛 BOOT의 다음 프레임(VSS_EVENT) | 수신 3, 전달 2, 세대 변경 1, 끝난 BOOT의 늦은 프레임 1건은 콜백 없이 거부(stale·old_generation 각 1). 받아들인 BOOT는 새 값. 소리는 남·남·안 남 | 통과 |
| 무정지 약 132 s | 상대 송신 없이 둔 뒤 한 번 읽음 | 0x340 527건 제출 = 송신 527 = 완료 527, abort 0. 수신 손실·overrun 0, ISR 링 최대 1, 시간 역행 0, TEC/REC 0. 슈퍼루프 한 바퀴 최대 1 tick(1 tick = 1.25 ms, 2.5 ms 미만) | 통과 |
| ACK 없음과 복구 | 상대 노드 전원(USB)을 뺐다가 다시 넣음(ACK 없는 구간은 abort 61 ÷ 초당 4로 약 15 s 추정) | 0x340 abort 61 = 송신 실패 61(100 ms TX timeout), abort가 끝나지 않아 MB를 격리한 일 0, bus-off 0. 끊긴 동안에도 상태 제출이 계속됐고 거절 0, 루프 최대 1 tick(2.5 ms 미만). 다시 넣은 뒤 0x340이 다시 보였고 TEC/REC가 0으로 돌아왔다 | 통과 |
| 후방 경고 50 ms 연속 | 상대 노드가 VSS_WARNING_STATE를 50 ms마다 새 평가로 389건 송신(후방 장애물 원 AGE 10 ms) | VSS 전달 389, 검증 실패 0. 경고음이 끊기지 않고 이어졌고 송신을 멈추자 곧바로 꺼졌다. 이 시험과 아래 50 ms permission 시험을 이어서 한 뒤 한 번 읽은 값: 수신 중복·stale·손실 0, ISR 링 최대 1, 0x340 219건 모두 성공, 루프 최대 3 tick(5 ms 미만) | 통과 |
| DOMAIN_PERMISSION 50 ms 연속 | 50 ms마다 새 평가로 송신하는 중에 읽음 | 사유 CURRENT, Power·Operation 사용 가능. 원 AGE 44 ms = 보낸 AGE 10 + 34, 수신 끊김 34 ms. Alive 사용 가능은 false(이 Wire에는 따로 증명된 Alive 필드가 없어서 늘 false) | 통과 |

- CAN 경로로 온 끼임(anti-pinch) 경고는 VSS가 평가되지 않은 값으로 받아 재생하지 않는다(VSS 설계). 그래서 경고음 확인에는 후방 장애물 경고를 썼다. 한 프레임 시험의 후방 장애물은 원 AGE 340 ms가 TTL 150 ms를 넘어 소리가 나지 않는 것이 맞다.

## BCM 노드 (S32K144 시험대)

시험대 앱은 BCM 상태 4종을 문서 주기로 제출하고(매번 새 평가, 품질 OK, SOURCE_AGE 0), DOMAIN 명령 콜백은 받은 수만 센다. 절차와 관찰 변수는 `nodes/bcm/bench/s32k144_bcm/README.md`에 있다.

### 빌드

| 확인 | 결과 |
|---|---|
| 동기화 | `tools/sync_to_s32ds.py --board bench144`로 108개 복사(Network 104, 시험대 4). define 8개(BCM 프로필, `NET_NODE_BUILD`, 시간원 hook 2개, 노드 설정 hook, µs hook, 생성 설정과 다른 FlexCAN 콜백 이름 2개) |
| 빌드 | 오류 0, 컴파일 경고 0. 링커 경고 4줄은 newlib(`memcmp`, `memcpy-stub`, `memmove`, `memset`)의 enum 크기 표시다. 프로젝트(`-fno-short-enums`)와 newlib 빌드 방식의 차이이고, 이 함수들은 enum을 주고받지 않는다 |
| 컴파일 확인 | `sdk_swap_check.py`는 BCM 프로필을 다루지 않아, 복제본의 C 파일 107개를 프로젝트 실제 옵션으로 ARM GCC 컴파일했다(오류 0, 경고 0) |
| 심볼 출처 | FlexCAN 콜백 두 개는 Network RTD 백엔드에만 정의된다. `SysTick_Handler`는 시험대 시계 파일에만 있다 |
| RAM(Network + RTD 백엔드) | Network 13,264 + RTD 백엔드 2,752 = 16,016 B, BCM 상한 16,384 B(여유 368 B). 시험대 앱 4,176 B는 따로 |
| 스택 | 최악 추정 2,536 B(main → `Net_MainFunction` 1,904, CAN 인터럽트 424, 예외 프레임 2단 208), 링커 스택 4,096 B |
| MB 배치 | 생성 설정 `max_num_mb` 7, RX 0..1, TX 2..5(기본 배치, define 없음) |

### 동작

| 확인 | 방법 | 결과 | 판정 |
|---|---|---|---|
| 기동 | 두 보드 함께 리셋 | `Net_Init` 결과 1, READY, RTD 시작, 초기화 실패 단계 0 | 통과 |
| 상태 4종 송신 | 260.2 s 뒤 한 번 읽음, 아래 ACK 없음 시험(63.7 s)에서도 읽음 | 설계 주기 0x200 100 ms, 0x210·0x220 200 ms, 0x230 250 ms. 송신 완료 6,245건 / 260.2 s = 초당 24.0, abort 0, 제출 거절 0. 63.7 s 실행의 상태별 제출 638 / 319 / 319 / 256이 주기 계산과 같고, DOOR의 TX 번호(638)가 DOOR 제출 수와 같았다. BOOT `0x42434D31` | 통과 |
| 0x340 수신 | 같은 읽기 | 1,042건 / 260.2 s = 초당 4.0, overrun 0. BCM 프로필은 0x340을 받지 않으므로 받지 않는 ID로 센다(정상, 63.7 s 실행에서 받지 않는 ID 195 = 수신 195) | 통과 |
| 상대 쪽 수신 개수 | S32K344만 디버거로 읽음 | BCM 프레임 1,513건을 받아 1,513건 모두 받지 않는 ID로 걸렀다, overrun 0. 경과 63.25 s × 24 = 1,518과 5건 차이(시작 시각 차이 약 0.2 s로 본다). 같은 실행의 S32K144 송신 수는 읽지 않았다 | 부분 통과 |
| 2분 이상 동작 | 260.2 s 뒤 읽음 | ISR 링 최대 3(용량 16), 넘침 0, 한 바퀴 최대 2,554 µs(평균 약 135 µs). 수신 버림 0은 63.7 s 실행에서만 읽었다 | 부분 통과 |
| ACK 없음과 복구 | S32K344 전원을 약 10 s 뺐다가 다시 넣음 | ACK 없는 구간 약 15 s(실패 수로 계산, 전원을 뺀 시간보다 긴 약 5 s는 S32K344 재부팅으로 본다): abort 361 = 송신 − 완료(1,532 − 1,171) = 송신 실패 361, MB 격리 0, bus-off 0, 제출 거절 0. 다시 넣은 뒤 송신이 재개됐고 TEC/REC 0. 서비스 성공 1,169건(100 ms 늦게 갱신된 사본) / 24 + 실패 구간 15 s = 63.7 s로 전체 시간과 맞다 | 통과 |

- RX MB 개수를 늘린 다시 빌드: 해당 없음(정상 운전에서 RX MB overrun이 없었다).
- ACK 없음 시험에서 서비스 수신 큐의 최대 깊이가 BCM 용량 8(한 단계 한도)에 닿았다. ACK 없는 구간에 abort가 몰린 때로 본다. 버린 사건은 0이고, 남은 사건은 다음 단계에서 처리된다.

## CIS 노드 (S32K144 시험대)

시험대 앱은 CIS 상태 3종을 문서 주기로 제출하고(CIS_REAR 0x0A0 50 ms, CIS_ENVIRONMENT 0x300 200 ms, CIS_STATUS 0x320 250 ms, 매번 새 평가), Pi 판정(P_VISION_RESULT)을 받을 때마다 CIS_OCCUPANT(0x310)를 제출한다. DOMAIN_PERMISSION(0x080)은 Network가 P_PERMISSION으로 Pi에 relay한다. 절차와 관찰 변수는 `nodes/cis/bench/s32k144_cis/README.md`에 있다.

- Pi 도구는 P_VISION_RESULT(29 B)를 100 ms, P_VISION_STATUS(26 B)를 1 s마다 보내고(문서 주기), 받은 P_PERMISSION의 순번·간격·내용을 검사한다. CIS가 받은 바이트 수는 Pi 도구 기록의 보낸 바이트 수와 대조했다.
- 실행마다 CIS를 리셋하고 시작했고(한 실행은 예외, "동작" 표 아래), 실행이 끝나면 한 번 멈춰 읽었다.

### 빌드

| 확인 | 결과 |
|---|---|
| 동기화 | `tools/sync_to_s32ds.py --board cis144`로 108개 복사(Network 104, 시험대 4). define 11개(CIS 프로필, `NET_NODE_BUILD`, 시간원 hook 2개, 노드 설정 hook, µs hook, FlexCAN 콜백 이름 2개, UART-C 인스턴스·설정·콜백 이름) |
| 빌드 | 빌드 성공(마지막 빌드의 콘솔 원문은 받지 않았다). 빌드 옵션 그대로(`-Os`, `-pedantic`) 복제본의 C 파일 64개를 따로 컴파일해 오류 0, 경고 0. `aa24a02` 빌드의 콘솔은 오류 0, 경고 0이고 링커 표시는 BCM 시험대와 같은 newlib enum 크기 표시다 |
| 심볼 출처 | FlexCAN 콜백 두 개는 Network CAN 백엔드(`net_hal_rtd_can.c`), LPUART 콜백은 Network UART 백엔드(`net_hal_rtd_uart.c`)에만 정의된다. `SysTick_Handler`는 시험대 시계 파일에만 있다. 디버그 노드 함수는 링크되지 않았다. `memcpy`·`memset`은 툴체인 `libc_nano.a`의 것이다(1바이트 루프, 아래 "CIS 시험에서 고친 결함") |
| RAM(Network + RTD 백엔드) | Network 18,976 + RTD 백엔드 4,904 = 23,880 B, CIS 상한 24,392 B(여유 512 B). Host 측정 23,862 B보다 18 B 많다. 시험대 앱 2,492 B(첫 빌드에서 잼, 그 뒤 `.bss` 합계 같음)는 따로 |
| 스택(정적 추정) | 최악 2,836 B 이하(main → `Net_MainFunction` 1,916 B 이하 + CAN 인터럽트 424 + LPUART 인터럽트 184 + 예외 프레임 3단 312), 링커 스택 4,096 B |
| MB 배치 | BCM 시험대와 같은 디버그 노드 생성 설정이고 MB define이 없다: `max_num_mb` 7, RX 0..1, TX 2..5(144 기본 배치) |

### 동작

| 확인 | 방법 | 결과 | 판정 |
|---|---|---|---|
| 기동 | 리셋 뒤 실행 | `Net_Init` 결과 1, READY, RTD 시작, UART-C 수신 대기 | 통과 |
| CIS 상태 3종 송신 | S32K344가 받은 프레임(80 s 실행) | 평균 간격 0x0A0 50.0 ms, 0x300 200.1 ms, 0x320 250.2 ms(두 시점의 TX_SEQUENCE와 수신 시각으로 계산). 빠짐·중복 0, TX_SEQUENCE = UPDATE_SEQUENCE, BOOT `0x43495331`. 제출 3,742 / 936 / 749건이 모두 받아들여졌다. 필드는 시험대 값 범위 안이다(거리 77.0·118.0 cm, 23.56 °C, 45.00 %RH, 326 lx) | 통과 |
| Pi 판정 → CIS_OCCUPANT | Pi 도구 80 s(판정 800, 상태 80) | 콜백 판정 800·상태 80 = Pi 송신. CIS_OCCUPANT 800건을 S32K344가 모두 받았다(간격 87~115 ms, 빠짐 0). 마지막 프레임의 Pi BOOT와 VISION_SEQUENCE가 Pi의 마지막 송신과 같다 | 통과 |
| DOMAIN_PERMISSION → P_PERMISSION relay | 같은 실행에서 S32K344가 0x080을 50 ms마다 새 평가로 900건 | S32K344 송신 900 = CIS 수신 900 = relay 900 = Pi 수신 900. relay 순번 1~900이 모두 +1, 간격 31~63 ms. 전원 허가·품질이 0x080 원본과 같고(1, 0), CIS가 매기는 사유 0, 사용 한도 1,500 ms | 통과 |
| 같은 평가의 반복 | S32K344가 같은 평가를 50 ms마다 8번(TX_SEQUENCE는 새로, AGE는 늘려서) | 콜백 8(그중 반복 7), relay 1(첫 프레임만), 보류 0 | 통과 |
| 0x080 끊김 감시 | 새 평가 20건을 보낸 뒤 멈춤 | relay 20 = Pi 20. 마지막 수신 + 160 ms에 RX_TIMEOUT(감시 150 ms, 시험대는 10 ms마다 읽는다), 그 뒤 원 AGE 500 ms에서 SOURCE_EXPIRED | 통과 |
| 망가진 Pi 프레임 | Pi 도구 25 s. 정상 프레임 사이에 CRC 오류, 잘린 프레임, 길이·버전·형식 오류, 잡음을 1건씩, 이어서 값이 코덱 계약 밖인 판정(인원 6) 1건 | 버린 바이트 136 = 망가진 바이트 합. CRC 오류 1, 조립 시간 초과 1, 헤더 오류 3. 콜백은 정상 프레임 수와 같다(판정 250, 상태 25). 계약 밖 판정은 콜백 없이 거절됐다(`typed_rejected` 1). 받은 바이트 8,065 = Pi 송신 | 통과 |
| 130 s 연속 | Pi 판정·상태와 0x080 2,400건(50 ms)을 함께 | 멈춤 없음: CIS 3종의 수신 수가 경과 시간과 맞고 빠짐 0. S32K344 새 평가 2,400 = relay 2,400 = Pi 수신 2,400, 판정 1,300·상태 130 = Pi. 받은 바이트 41,080 = Pi 송신. CAN ISR 링 최대 3(용량 16), UART-C 바이트 링 최대 26(용량 256), 넘침 0, 수신 버림 0, RX MB overrun 0, abort 0, bus-off 0 | 통과 |
| UART-C 바이트 손실 | 위 모든 실행 | LPUART overrun 0, 수신 오류 0, 손실 표시 0. 받은 바이트 = Pi 송신(80 s 25,280 B, 25 s 8,065 B, 130 s 41,080 B, 90 s 28,440 B, 10 s + 10 s 6,320 B) | 통과 |
| `Net_MainFunction` 1회 최대 시간 | `main_max_us`(실행마다) | UART-C 수신이 있는 실행 5,819~7,195 µs, 없는 실행 약 1,620 µs. 15 ms 미만(`CIS_NODE_INTEGRATION.md` 보드 확인 항목) | 통과 |
| CIS 주기 흔들림 | S32K344가 잰 CIS_REAR 간격(UART-C 수신 중, 실행 3개) | 46~56 ms(기준 40~60 ms), 빠짐 0. UART-C 수신이 없으면 48~52 ms | 통과 |
| ACK 없음과 복구 | S32K344 전원(USB)을 뺐다가 다시 넣음 | CIS CAN 송신 실패 47 = abort 47(100 ms TX timeout), bus-off 0, 읽을 때 TEC 0. 다시 넣은 뒤 S32K344가 0x0A0을 받았고(46~54 ms, 빠짐 0) 새 0x080도 다시 relay됐다. UART-C 쪽은 영향이 없었다(판정 900·상태 90 = Pi, 손실 0) | 통과 |
| DOMAIN 재시작 | 다시 켠 S32K344가 같은 BOOT로 순번 1 프레임 1건, 이어서 새 BOOT로 50 ms마다 | 같은 BOOT의 순번 1 프레임은 옛 것으로 콜백 없이 버렸다(`stale` 1). 새 BOOT의 첫 프레임에서 새 세대로 받아 relay를 다시 시작했다. relay 순번은 CIS가 매기므로 끊긴 앞뒤가 +1로 이어졌다(2889 → 2890) | 통과 |
| Pi 재부팅 | Pi 도구 10 s 뒤 새 Pi BOOT로 LINK 1부터 10 s | 새 BOOT의 첫 프레임에서 새 세대(`generation_changes` 1). 콜백이 이어졌다(판정 200·상태 20 = Pi). CIS_OCCUPANT의 Pi BOOT가 새 값으로 바뀌었고 CIS 송신 순번은 이어졌다 | 통과 |

- "같은 평가의 반복"과 "0x080 끊김 감시"는 서로 다른 실행에서 쟀다. S32K344 디버그 노드의 `repeat`는 마지막 평가에 실제 경과 시간을 AGE로 더해 보낸다. 몇 분 전 평가를 반복하면 AGE가 500 ms 이상이 되어 CIS가 콜백 없이 거른다(설계대로). 그래서 반복 시험은 새 평가 번호로 시작했다.
- "ACK 없음과 복구"와 "DOMAIN 재시작"은 한 실행이고, 직전에 CIS를 리셋하지 않았다. 약 4분 전 리셋 뒤 0x080 relay만 있던 구간이 카운터에 함께 들어 있다. 세대 변경 2건 중 1건은 준비 단계에서 S32K344 BOOT를 기본값으로 되돌린 것이다. relay만 있던 구간은 다른 판정 카운터를 바꾸지 않았다고 보지만, 실행 시작 때의 값은 읽지 않았다.
- UART-C 수신이 있는 동안 서비스 수신 큐의 최대 깊이가 용량 24에 닿았다(버림 0). 한 step의 poll 수만큼 채우는 설계 값이다(`CIS_NODE_INTEGRATION.md` 보드 확인 항목).

### CIS 시험에서 고친 결함

처음 두 번의 시험에서 Network 결함 두 가지가 나왔다. 고친 뒤 위 시험을 처음부터 다시 했다.

| 처음 본 것 | 원인 | 수정 | 고친 뒤 |
|---|---|---|---|
| (`3ee29af`) Pi 판정 전달과 P_PERMISSION relay가 Pi 도구를 시작하고 약 2 s 뒤 함께 멈췄고 리셋 전까지 돌아오지 않았다. 바이트는 끝까지 들어왔다. 어댑터 수신 큐 넘침 69, UART-C 링크 실패 | RTD 백엔드가 ISR 링에서 어댑터 수신 큐로 옮길 때 큐의 빈칸을 보지 않았다. 큐가 넘치면 어댑터가 그 링크를 실패로 두고, RTD 백엔드는 링크 리셋을 내지 않는다 | 옮기는 수를 어댑터 빈칸 이하로 줄였다(CAN·UART 공통, `aa24a02`, 시험 `uart_hal.u16_drain_adapter_room`) | 어댑터 넘침 0, 링크 실패 없음 |
| (`aa24a02`. overrun은 `3ee29af`에서도 12건 있었다) UART-C 바이트가 30 s에 약 7번 사라졌다(LPUART overrun). 80 s 실행에서 Pi 판정 6/800, 상태 1/80을 잃었다(overrun 8, 수신 오류 2). `Net_MainFunction` 1회 최대 약 28 ms, CIS_REAR 간격이 최대 84 ms까지 벌어졌다. UART-C 수신이 있을 때만 나왔다 | ① S32DS GCC 10.2의 newlib는 nano판·표준판 모두 `memcpy`·`memmove`·`memset`이 1바이트 루프이고, `-Os`에서는 구조체 대입도 이 `memcpy`가 된다. 어댑터 잠금(RTD 백엔드에서는 인터럽트 금지) 안의 이벤트·송신 시도·패킷 복사와 지우기가 약 90~150 µs로 UART 한 바이트 시간(약 87 µs)보다 길다고 추정했다(명령 수와 에뮬레이터, 보드에서 재지 않음). mem 함수만 워드 단위로 바꾼 진단 빌드(Network 소스 그대로)에서 overrun이 0이 되어 확인했다. ② 수신 바이트마다 수신 이력 138칸 전체를 검사하고 한 칸씩 밀었고, UART 코덱이 호출마다 registry 전체를 다시 검사했다(진단 빌드에서도 1회 최대 약 14 ms를 쟀다. 바이트당 약 0.2 ms는 에뮬레이터 추정) | 인터럽트 금지 구간의 큰 복사·지우기(이벤트, 송신 시도, 패킷, 진단)와 바이트마다의 복사·지우기를 Network 자체 워드 단위 함수(`net_wire_copy`, `net_wire_zero`)로 바꿨다. 수신 이력은 고리로 두고 바이트마다 조립 중인 부분만 대조하며, registry 범위 검사는 설정 때 한 번 한다(`5e9ff0c`, 시험 `memory.uart_receiver_history_ring`, `memory.uart_wire_copy_words`) | overrun 0, 손실 0, 1회 최대 5.8~7.2 ms, CIS_REAR 46~56 ms |

## 확인하지 못한 것

- **BCM의 DOMAIN 프레임 수신과 결과·이벤트:** 0x080·0x100·0x110·0x120 수신과 0x140·0x141 송신은 보드에서 보지 못했다(보낼 상대가 없다). Host 시험(`bcm_node.*`)만 있다.
- **버스 위 BCM 프레임 내용:** 상대 VSS는 BCM 프레임을 해독하지 않는다. 보드에서는 "나갔고 ACK를 받았다"까지다. 내용은 host 코덱 시험과, 시험대 앱을 host에서 10 s 돌려 나간 240프레임을 해독한 결과(저장소에 넣지 않은 일회성 실행)로 대신한다.
- **문서 통신량에서의 링 깊이:** 노드 문서의 보드 확인 항목(문서 통신량이 흐르는 버스에서 수 분, 링 최대 깊이 용량의 75 % 이하)은 남아 있다. 이번 부하는 VSS 초당 20프레임(한 흐름), BCM 송신 초당 24 + 수신 4였다. CIS는 CAN 송신 초당 약 39(상태 3종 29 + CIS_OCCUPANT 10)와 수신 20(0x080), UART-C는 문서 주기의 Pi 링크(초당 316 B)와 P_PERMISSION relay 초당 20이었다(CAN ISR 링 최대 3/16, UART-C 바이트 링 최대 26/256).
- **실제 DOMAIN 보드, 실제 Pi와 CIS 앱, DOMAIN UART-M, WINDOW:** 시험 범위 밖이다. CIS 시험의 DOMAIN은 디버그 노드, Pi는 노트북 도구, CIS 앱은 시험대 대역이었다.
- **bus-off:** 세 시험 모두 나지 않았다(ACK 없음 시험 포함). 생성 설정은 FlexCAN 하드웨어 자동 복구가 켜져 있다. 이 시험들 뒤에 Network는 bus-off 뒤 CAN 링크를 닫고, 남은 송신을 정산한 뒤 100 ms(다섯 번 넘게 이어지면 1 s) 지나 새 포트 epoch로 다시 열도록 바뀌었다(`VSS_NODE_INTEGRATION.md` "제한"). 이 동작은 host 시험으로만 확인했고, 보드에서는 보지 않았다.
- **TX abort 시간 초과와 RX 재등록 거절:** RTD가 TX abort를 TIMEOUT으로 끝내는 경우, 그 뒤 MB 플래그가 다시 서는지, RX 재등록(`FlexCAN_Ip_Receive`)을 거절하는 경우는 보드에서 일으켜 보지 않았다. Network는 플래그가 서지 않은 MB를 리셋 전까지 쓰지 않고(`rtd.tx_quarantined`), 거절된 RX MB는 main에서 다시 건다(`rtd.rx_unarmed`). host 시험으로만 확인했다.
- **UART abort 거절과 송신 시작:** RTD가 UART 송신 abort를 거절하는 경우, 임계 구역(인터럽트 금지) 안에서 부른 `Lpuart_Uart_Ip_AsyncSend`가 그 상태를 지키는지는 보드에서 보지 않았다. Network는 abort가 거절되고 송신 상태가 BUSY이면 버퍼를 돌려주지 않고 바퀴마다 abort를 다시 한다(`rtd.uart[n].uart_tx_abort_waiting`). host 시험으로만 확인했다.
- **링크 실패 뒤 회복:** 어댑터 수신 큐가 넘치면 Network는 그 링크(CAN 또는 UART-C)를 실패로 두고 리셋 전까지 회복하지 않는다(CIS 시험의 첫 결함에서 UART-C로 본 동작). 고친 뒤에는 넘침이 없었고, 지금 RTD 백엔드에서는 이 경로가 나지 않는다(drain은 빈칸 이하). 회복하는 것은 bus-off뿐이다(위 줄).
- **BOOT_ID의 보드 출처:** Network 노드의 BOOT_ID는 상수다. 한쪽만 리셋하면 상대 Network 노드가 그 노드의 프레임을 옛 것으로 거부한다.

## 시험 중 관찰

- 디버거로 CPU를 멈춰도 FlexCAN은 계속 받는다. S32K144 BCM 노드(260.2 s 실행)를 한 번 멈췄다가 다시 돌린 뒤 읽자, 멈춘 동안 RX MB 2개를 넘은 프레임이 덮어써져 overrun이 1건 생겼고, 다시 돌릴 때 쌓인 사건이 한꺼번에 처리됐다(ISR 링 4, 한 번에 5건, 시각이 now보다 1 ms 앞선 기록 1건). 멈추지 않은 ACK 없음 시험에서 overrun과 시각 앞섬은 0이었다(ISR 링 4·한 번 5건은 그 시험에서도 나왔고, 용량 16·기준 12 안이다). 판정에는 멈추기 전에 읽은 값을 썼다.
- VSS 보드를 디버거로 멈추면 오디오 출력 고장이 래치되어 리셋까지 소리가 나지 않는다(VSS 설계). 소리 확인은 멈추기 전에 끝냈다.
- VSS_EVENT 시험의 첫 시도에서 VSS 보드를 멈춘 상태로 일부 프레임을 보냈다. 보낸 6건이 모두 하드웨어에 도착했지만(수신 +6) 전달 1, 중복 1만 세어졌고, 새 발생(TX 2)은 전달되지 않았다. 나머지 4건의 행방은 그때 읽은 카운터로는 알 수 없었다. 리셋으로 카운터가 지워져 원인은 확인하지 못했고, 멈추지 않고 다시 한 시험에서는 재현되지 않았다. 판정에 넣지 않았다.
- 두 보드에 디버그 세션을 동시에 띄우면 S32DS가 멈췄다. 한 번에 한 세션만 쓰고, 상대 보드의 경과 시간은 그 보드의 카운터로 쟀다.
- S32K144 BCM 노드의 한 바퀴 최대 시간(2,554 µs, ACK 없는 실행 2,752 µs)이 언제 나왔는지는 보지 않았다. 기준(수 ms 미만)은 만족한다.
- 한 번은 앞 실행이 끝난 뒤 CIS가 보낸 P_PERMISSION 3프레임과 조각이 다음 실행의 앞머리에 함께 나왔다(S32K144 평가 보드의 OpenSDA 가상 COM 포트 버퍼로 추정. 다른 실행에서는 나오지 않았다). 그 뒤 판정에 쓴 0x080 송신은 Pi 도구가 포트를 연 뒤에 시작했다.
- CIS의 LPUART1 생성 설정은 115,942 bit/s로 노트북 115,200 bit/s보다 0.64 % 빠르다. 수신 오류(프레이밍·잡음)는 결함을 고친 뒤의 모든 실행에서 0이었다.
- `rtd.tick_future_clamped`는 UART-C 수신이 있는 실행에서 2,213~14,836건이었다(UART-C 수신이 없는 실행은 읽은 1회 0). 그 바퀴가 시작할 때의 now보다 늦게 ISR에 기록된 사건(CAN 수신·송신 완료, UART 바이트 등)을 now 시각으로 받은 수다. 판정에는 넣지 않았다. 시험 뒤 검토에서 정상 동작으로 봤다: 시각은 최대 한 바퀴 이르게만 잡히므로 AGE와 수신 감시는 오래된 쪽으로만 틀린다. 이후 이미지는 가장 많이 앞선 값(`tick_ahead_max_ms`, 최대값)을 남기고 축에 놓을 수 없는 값(`tick_unplaceable`)을 따로 센다.
- CIS의 `g_net_diag`는 시험대가 100 ms마다 갱신한 사본이라 앱 카운터(`g_cis_bench`)보다 최대 2건 늦은 값이 있었다. 판정에서는 이 차이를 따로 적었다.
