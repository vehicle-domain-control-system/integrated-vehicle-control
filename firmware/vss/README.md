# VSS 펌웨어 (S32K344)

Vehicle Sound System 펌웨어. DOMAIN이 보낸 Event와 경고 상태를 받아 우선순위에 따라 큐 음원(MP3)을 재생하고, 재생 결과와 서비스 상태를 Network로 보고한다. 단일 슈퍼루프로 동작하며 RTOS·동적 할당을 쓰지 않는다.

```
입력(Network 콜백 또는 SW2 시험 입력기)
  → INPUT 어댑터 → INPUT → STORE → POLICY → PLAYBACK
  → AUDIO_STREAM(MP3 디코드) → AUDIO_TX(SAI/eDMA 링) → SGTL5000
  → (출력 경계 시각) → PLAYBACK → STORE / HEALTH → VssNet_PublishStatus
```

## 1. 레이어 구조

```
src/
  app/        vss_main      부팅 순서와 슈퍼루프, main()
              vss_flow      서비스 1회 처리(FLOW)
              vss_test_input SW2 18단계 시험 입력기
  service/    input/        INPUT, Network 메시지 어댑터(vss_input_net)
              store/        STORE(발생 ledger, 재생 바인딩·사실, 회수)
              policy/       POLICY(Select_Choose)
              playback/     PLAYBACK(세션 1개)
              audio_stream/ AUDIO_STREAM, decoder/(minimp3)
              asset/        큐 음원 9개와 (signal, value) → 음원 표
              health/       HEALTH(고장 집합, 서비스 상태, 상태 보고)
  driver/     audio_tx/     SAI/eDMA 반버퍼 링 구동, 경계·고장 래치
              audio_control/ CS2100(클록)·SGTL5000(코덱) 설정, unmute/mute
              input/        SW2 디바운스
  hal/        include/      보드 API(RTD 타입 없음)
              s32k344/      RTD 호출, 생성 설정 심볼, 인터럽트 콜백
  infra/      vss_config.h  빌드 스위치(이 파일에서만 정의)
              vss_time      시스템 시계(VssTime)
  net_if/     vss_net_api.h Network 인터페이스 선언
              net_stub      Network 대체(VSS_CFG_NET_STUB=1)
tests/
  host/       host 단위·E2E 시험, hal_mock(HAL 대체)
  rtd_mock/   ARM 컴파일·HAL 시험용 RTD 대체 헤더
tools/        host_test.py, arm_check.py, check_layers.py
```

include 방향(`tools/check_layers.py`가 검사):

```
app  →  service  →  driver  →  hal/include
 │         │           │
 └─────────┴───────────┴──→ infra
service/input/vss_input_net.{c,h}, app  →  net_if
hal/s32k344  →  RTD 헤더 (RTD include·호출은 이 폴더에만)
```

- 역방향 include 금지. 하위 → 상위 통지는 등록된 함수 포인터(HAL → driver 인터럽트 콜백) 또는 상위의 폴링으로만 한다.
- app이 include하는 hal 헤더는 `hal_board.h`뿐이다. 예외로 `app/vss_main.c`는 `hal_timer.h`를 include해 tick을 시작하고 `HalTimer_GetTicks`를 `VssTime_Init`에 시스템 시계 공급원으로 1회 등록한다(infra가 hal을 include하지 않기 위한 유일한 경로).
- driver 헤더를 include하는 service는 `audio_stream`, `health`뿐이다. Network 타입이 든 `vss_input_net.h`는 어댑터 자신과 app만 include한다.
- 모든 소스에서 extern 데이터 선언 금지(상태는 함수로만 공유). 디버거 관찰 변수는 각 `.c`의 static 변수다.
- 인터럽트에서는 플래그·카운터·tick 기록만 한다. 디코드·I2C·서비스 호출은 슈퍼루프에서만 한다.

## 2. 부팅 순서와 슈퍼루프 (`app/vss_main.c`)

부팅:

| 순서 | 호출 | 실패 시 |
|---:|---|---|
| 1 | `HalBoard_Init` (클록·PLL, 핀, 인터럽트, LPI2C) | INIT 고장 보고(5단계 뒤) |
| 2 | `HalTimer_Init` → 80 tick 대기(코덱 기준 클록 안정) | **진행하지 않고 정지** |
| 3 | `VssTime_Init(HalTimer_GetTicks)` | — |
| 4 | `Health_Init(활성 origin)` | — |
| 5 | `AudioControl_Init` (CS2100 lock, SGTL5000 확인·설정) | INIT 고장, **AUDIO_TX를 시작하지 않음** |
| 6 | `AudioControl_Unmute(VSS_CFG_VOLUME_PERCENT)` | OUTPUT 고장 |
| 7 | `Sw2Button_Init`, `AudioAsset_Init`, `AudioStream_Init`, `VssTestInput_Init` | — |
| 8 | `Flow_Init(활성 origin)`, `Net_Init` | — |
| 9 | `AudioTx_Init` → `AudioTx_Start` | INIT 고장 |
| 10 | `Health_BootComplete` → 슈퍼루프 | — |

- tick 카운터는 리셋 때 0이고 2단계에서 세기 시작한다. **VSS 기동 시각은 VssTime 시계 0**이다.
- 블로킹 I2C와 대기는 모두 9단계 전에 끝난다. `AudioTx_Start` 뒤에는 두 번째 경계(약 43 ms) 전에 반버퍼를 채우지 못하면 LATE 고장이다.
- 활성 origin은 `VSS_CFG_TEST_INPUT=1`이면 TEST, 0이면 PRODUCT.

슈퍼루프 1바퀴:

1. `AudioStream_Service` — 빈 반버퍼 채우기(마감 약 21.3 ms)와 출력 사실 생성
2. 입력 전달 — `Net_MainFunction(VssTime_NowMs32())`, `VSS_CFG_TEST_INPUT=1`이면 시험 입력기
3. `Flow_Process` — 2에서 어댑터가 메시지를 INPUT에 넘겼거나(어댑터 카운터 전후 비교), 직전 처리가 FOLLOW_UP/RECOLLECT를 반환했거나, 직전 처리 뒤 `VSS_CFG_FLOW_PERIOD_MS`가 지났을 때. Flow는 시각을 스스로 읽으므로 반드시 입력 전달 뒤에 부른다.
4. 출력 고장 mute — `AudioStream_OutputFailed()`가 참이면 즉시 `AudioControl_MuteLatched()`, 코덱이 mute를 수용하지 않는 동안 50 ms마다 다시 시도

한 바퀴 안에 블로킹 호출은 없다(출력 고장 뒤 mute 재시도만 예외).

디버거 관찰 변수:

| 변수 | 내용 |
|---|---|
| `g_vssMain` | 부팅 단계·결과, 루프 횟수, Flow 실행 횟수(입력·후속 구분), 루프 최대 시간(tick), mute 호출·수용 |
| `g_vssTestInput` | 시험 단계, press 수, Event 수용·중복·거부 수, Warning 프레임 수, 어댑터 거부 래치 |
| `Health_Read`/`Flow_Read`/`Playback_Read`/`VssInputNet_Read` | 서비스별 관측 구조체 |
| `NetStub_ReadLastStatus` | stub이 받은 마지막 `NetVssStatus` |

## 3. 빌드 스위치 (`src/infra/vss_config.h`)

모두 컴파일러 `-D`로 덮어쓸 수 있고, 범위 밖 값은 `#error`로 빌드가 실패한다.

| 스위치 | 기본 | 범위 | 의미 |
|---|---:|---|---|
| `VSS_CFG_TEST_INPUT` | 1 | 0/1 | 1: SW2 시험 입력기 사용, 활성 origin TEST. 0: Network 콜백만, 활성 origin PRODUCT |
| `VSS_CFG_NET_STUB` | 1 | 0/1 | 1: `net_if/net_stub.c`가 Network를 대신함. 0: 실제 Network 구현과 링크 |
| `VSS_CFG_HOST_TEST` | 0 | 0/1 | host 시험 빌드(시험용 reset 함수 포함, `main()` 제외). 타깃은 항상 0 |
| `VSS_CFG_VOLUME_PERCENT` | 70 | 10..100 | 부팅 unmute 헤드폰 레벨. 100 %는 코덱 −12 dB(0 dB 이상 불가), 70 %는 −24 dB |
| `VSS_CFG_RX_GAP_BUDGET_MS` | 5 | 0..100 | 수신 경로 여유(Age 상한에 더함) |
| `VSS_CFG_FLOW_PERIOD_MS` | 10 | 1..20 | 입력이 없을 때 Flow 처리 간격 |
| `VSS_CFG_STATUS_PERIOD_MS` | 250 | ≥ 50 | 내용 변화가 없을 때 상태 보고 평가 주기 |
| `VSS_CFG_STORE_RECLAIM_MS` | 2500 | > 2000 | 원 발생 구간 종료 후 STORE 레코드 회수까지의 시간 |
| `VSS_CFG_STORE_RECLAIM_PERIOD_MS` | 100 | 10..1000 | STORE 회수 주기 |
| `VSS_CFG_TEST_WARNING_PERIOD_MS` | 50 | ≥ 10 | 시험 입력기의 Warning 송신 주기 |

## 4. 빌드·시험

리포 루트에서:

```
python firmware/vss/tools/host_test.py     # host 단위·E2E 시험 전체(일반 + UB/범위 trap 구성), 설정 가드, check_layers 자체 시험
python firmware/vss/tools/arm_check.py     # 타깃 gcc로 src 전체 컴파일, main 경로·인터럽트 스택, 오브젝트 크기
python firmware/vss/tools/check_layers.py  # include 방향·RTD 사용 위치·extern 데이터 검사
```

- 툴체인·RTD 헤더·생성 설정 위치는 로컬 환경 파일(JSON)에 두고 환경 변수 `VSS_ENV_JSON`으로 경로를 준다. 리포에는 넣지 않는다.
  - `arm_check.py`: 변수가 없거나 파일·경로가 없으면 `tests/rtd_mock` 대체 헤더로 컴파일하고 보고서 첫머리에 `MOCK`이라고 표시한다(이때 PATH에 `arm-none-eabi-gcc`가 있어야 한다). MOCK 결과는 실제 헤더 확인을 대신하지 않는다.
  - `host_test.py`: 환경 파일의 host 컴파일러를 쓰고, 없으면 PATH의 gcc/clang을 쓴다.
- 공통 옵션: `--build-dir DIR`(기본은 임시 폴더, 끝나면 삭제), `--report FILE`(결과를 파일로도 저장). `host_test.py --filter 이름일부`로 시험 일부만 실행.
- 스택 한도: main 경로 ≤ 3072 B, main 경로 + 가장 깊은 인터럽트 + 예외 프레임 ≤ 4096 B(보드 스택 영역). 256 B 이상 지역 변수는 쓰지 않는다(모듈 static 스크래치).

## 5. SW2 시험 입력기 18단계 (`VSS_CFG_TEST_INPUT=1`)

SW2를 한 번 누를 때마다(디바운스된 press) 한 단계 진행한다. 메시지는 Network 콜백과 같은 어댑터를 TEST origin으로 거친다.

| 단계 | 입력 | 기대 동작 |
|---:|---|---|
| 0 | 부팅, 모든 경고 CLEAR | 재생 없음 |
| 1 | Event WELCOME (새 occurrence) | WELCOME 1회 |
| 2 | 1단계의 재전송: 같은 occurrence, 다음 TX, 새 수신 시각, Age = 1단계 이후 경과 | 재생 없음(사용 한도 500 ms가 지나 거부) |
| 3 | Event LOCK_COMPLETE | LOCK |
| 4 | Event UNLOCK_COMPLETE | UNLOCK |
| 5 | Event LOCK_ERROR | LOCK_ERROR |
| 6 | Event GOODBYE | GOODBYE |
| 7 | 후방 장애물 CAUTION | REAR_CAUTION 반복 |
| 8 | 후방 EMERGENCY | REAR_EMERGENCY로 선점 |
| 9 | 끼임 ACTIVE | ANTI_PINCH로 선점(최고 순위) |
| 10 | 끼임 CLEAR | REAR_EMERGENCY로 재중재 |
| 11 | 탑승자 위험 ACTIVE | 변화 없음(탑승자 < 후방 긴급) |
| 12 | 후방 CLEAR | OCCUPANT로 전환 |
| 13 | 탑승자 CLEAR | 정지 |
| 14 | 후방 quality INVALID(값 CLEAR) | 변화 없음(INVALID는 CLEAR가 아님, 마지막 유효 값이 CLEAR) |
| 15 | 후방 quality OK + EMERGENCY | REAR_EMERGENCY |
| 16 | 후방 CLEAR | 정지 |
| 17 | Event WELCOME (새 occurrence) | WELCOME |
| 18 | 입력 종료 | 이후 SW2 무시, Warning 송신 중지, 무음 유지 |

- 메시지: `boot_id` 1, Event·Warning TX는 각각 1부터 증가, `received_at_ms = VssTime_NowMs32()`. Event는 Age 0, 사용 한도 500 ms, quality OK, available. Warning은 50 ms마다(경고가 바뀌는 단계에서는 즉시) 보내며, 활성 메타 format 1·활성 ENABLED, 모든 lane source boot 1, lane sequence는 프레임마다 증가(매 프레임이 새 평가).
- 2단계 분류: 같은 occurrence의 재전송은 원 발생 뒤 사용 한도 안이고 Age가 정확하면 STORE가 중복으로 받아 재생하지 않는다. 사용 한도가 지난 뒤면 INPUT이 시간 조건으로 거부한다. 같은 TX·같은 수신 시각의 바이트 동일 프레임은 INPUT이 변화 없음으로 처리한다. SW2는 WELCOME을 들은 뒤 누르므로 2단계는 항상 거부 경로다.
- 끼임(창문) 경고는 TEST origin에서만 재생된다. PRODUCT origin은 끼임 입력을 처리하지 않는다.
- host 시험 `tests/host/test_e2e_test18.c`가 실제 부팅·슈퍼루프로 18단계 전체를 확인한다.

## 6. Network 연동 조건

Network 모듈은 별도로 구현된다. VSS는 `net_if/vss_net_api.h`의 선언을 따르고, 실제 Network 헤더가 오면 그것으로 대체한다. 아래는 VSS가 전제하는 조건이다.

| 항목 | 조건 |
|---|---|
| 호출 문맥 | VSS 슈퍼루프가 매 바퀴 `Net_MainFunction(nowMs)`를 부르고, 그 안에서 `VssApp_OnDomainEvent`/`VssApp_OnDomainWarningState`가 동기 호출된다. 인터럽트에서 부르지 않는다. VSS는 전달받은 포인터를 호출 뒤에 보관하지 않는다 |
| 함수 | `Net_Init(void)`, `Net_MainFunction(uint32_t nowMs)`, `NetSubmitResult VssNet_PublishStatus(const NetVssStatus *)` |
| 수신 시각 | `NetCanRxMeta.received_at_ms`는 VssTime 축(`VssTime_NowMs32`)의 값이다. 인터럽트는 tick만 기록하고, 슈퍼루프 문맥에서 `VssTime_TickToMs32(tick)`로 바꾼다. 다른 모듈이 tick에서 직접 ms를 계산하지 않는다. 미래 시각은 쓸 수 없는 시각으로 처리된다 |
| TX_SEQUENCE | CAN ID(메시지)별로 따로 증가한다. VSS는 Event와 Warning의 순서를 각각 검사한다 |
| BOOT_ID | DOMAIN 기동마다 다른 값이며 0이 아니다(송신 측 책임). Network가 새 BOOT를 수용하면 그대로 전달하고, VSS는 새 세대로 입력을 다시 연다. 재사용된 BOOT·바뀌지 않는 BOOT는 교체된 세대로 거부되어 입력이 멈추며 VSS가 자동 복구하지 않는다 |
| occurrence_id | DOMAIN 한 기동 안에서 단조 증가한다(같은 순위 Event의 순서 근거) |
| Network 검사 | DLC·padding·enum 범위·WARNING format 0/1 규칙, BOOT≠0, 메시지별 TX 증가, 완전 중복 폐기, RX 시점까지 Age 누적은 Network가 한다 |
| accepting | `vss_accepting_events` = 부팅 완료 ∧ 상태가 FAULT 아님 ∧ ledger 여유. 첫 DOMAIN 메시지 수신 여부와 무관하다(첫 메시지 전에 0이면 교착). 재생 시작을 보장하지 않는다 |
| 상태 보고 | VSS는 새 평가가 있을 때만 제출한다: 상태·가용성·accepting·고장 mask·마지막 고장이 바뀌면 즉시, 바뀌지 않아도 직전 보고 평가로부터 `VSS_CFG_STATUS_PERIOD_MS` 뒤. 거부(NOT_READY/NO_CAPACITY 등)되면 다음 Flow 처리마다 같은 번호·같은 내용으로 다시 제출하고 Age만 다시 계산한다. 그 사이 새 평가가 나오면 그것으로 바꾼다(번호 하나 건너뜀). 번호는 1부터, `UINT32_MAX` 다음은 1(0 없음). BOOT_ID/TX_SEQUENCE 부여, 주기 송신, 대기 Age 누적은 Network 담당 |
| 고장 이력 | 고장 종류 5개 각각 현재 1개 + 최근(해제된 것) 1개를 둔다. 같은 종류가 다시 해제되면 최근 기록을 덮어쓴다 |

## 7. 동작 규칙

- **출력 근거**: 출력 사실의 시각은 DMA 반버퍼 경계 인터럽트 시각이다. 세션의 첫 PCM이 든 반버퍼가 재생을 시작한 경계 = 시작, 마지막 PCM 반버퍼 재생이 끝난 경계 = 완료, 정지 요청 뒤 재생 중 반버퍼에 세션 PCM이 없게 된 경계 = 정지. SAI FIFO·코덱 지연(< 1 ms)과 tick 내림(≤ 1.25 ms)은 사실 시간 구간 `[at, at+3 ms]`에 포함된다.
- **TX 순번**: Event와 Warning은 CAN ID별 TX_SEQUENCE를 따로 쓴다. 한쪽 스트림이 앞서 가도 다른 쪽이 거부되지 않는다. 같은 TX의 바이트 동일 재전달은 변화 없음, 같은 TX의 다른 내용과 더 작은 TX는 거부.
- **DOMAIN 재부팅**: `boot_id`가 바뀌면 새 세대로 입력을 다시 연다. 옛 세대의 미재생 Event는 replay를 잃고, 옛 세대의 늦은 프레임은 새 세대를 닫지 않고 거부된다. 최근 교체된 BOOT 4개를 기억해 거부한다.
- **Event 재생 불가 기준 = VSS 기동 시각(VssTime 시계 0)**: 원 발생 구간이 시계 0 이전일 수 있는 Event(Age + 수신 여유 > 기동 경과)는 재생하지 않는다. 기동 뒤 발생한 Event는 INPUT을 여는 첫 메시지여도, 첫 Warning보다 먼저 발생했어도 재생된다.
- **사용 한도**: Event는 원 발생 시각 이후 `use_limit_ms` 안에서만 시작할 수 있다.
- **STORE 회수**: 원 발생 구간 종료가 `VSS_CFG_STORE_RECLAIM_MS`(2500 ms) 이상 지났고 진행 중인 재생 바인딩이 없는 레코드는 바인딩·사실과 함께 제거한다(사용 한도 상한 2000 ms가 지난 같은 발생은 다시 와도 재생할 수 없다). 회수해도 STORE revision은 감소하지 않는다.
- **같은 순위 Event**: 원 발생 구간이 떨어져 있으면 시간 순. 겹치면 같은 DOMAIN 기동(origin·source·generation)일 때 `occurrence_id`가 작은 쪽이 먼저, 다른 기동이면 다시 읽어 판단한다.
- **선점과 시작 전 정지**: 첫 PCM 전에 정지된 Event는 시작하지 않은 것으로 기록되어 사용 한도 안에서 다시 선택될 수 있다. 시작 뒤 정지된 Event는 중단으로 끝나고 다시 재생하지 않는다.
- **디코드 실패**: 시작 전에 디코드 실패로 끝난 Event는 재시도하지 않고 종결한다(음원은 플래시 고정 데이터). ASSET 고장으로 남는다.
- **vss_fault_active** = `fault_mask` ≠ 0 (현재 고장 5종 중 하나라도 있으면 1).
- **서비스 상태**: INIT 또는 OUTPUT 고장, 또는 재생 가능한 지원 큐가 하나도 없으면 FAULT/UNAVAILABLE. 부팅 미완료면 STARTUP/UNAVAILABLE. 시작이 확인된 세션이 종료 전이면 PLAYING, 아니면 READY. 지원 큐가 모두 준비되고 현재 고장이 없으면 FULL, 아니면 DEGRADED. 지원 큐 집합은 활성 origin이 재생할 수 있는 행이다: PRODUCT는 끼임 제외 8개, TEST는 9개 전부.
- **출력 고장**: AUDIO_TX의 LATE·OVERLAP·SAI 오류·경계 정지(STALL)는 리셋까지 래치된다. 전송을 1회 중단하고, 슈퍼루프가 코덱을 mute한다(수용될 때까지 50 ms마다 재시도). mute가 요청됐는데 수용되지 않은 동안도 OUTPUT 고장이다. 출력 고장 뒤에는 새 세션을 시작하지 않는다. 복구는 리셋이다.
- **보드 운용 주의**: 재생 중 디버거로 멈추면 경계 마감을 넘겨 출력 고장(fail-closed)이 된다.
