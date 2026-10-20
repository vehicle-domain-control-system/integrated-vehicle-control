# VSS 노드 Network 통합 메모

VSS 노드(profile 6) 펌웨어에 Network를 붙일 때 지켜야 할 사항만 모았다. 설계 근거는 `docs/network/`이다. VSS 쪽 전제는 VSS 펌웨어의 `src/net_if/vss_net_api.h`, `src/app/vss_main.c`, `src/infra/vss_time.h`, `README.md` §6·§7이다.

## App 경계 (정본 헤더)

- `include/network/app/net_if_common.h`: `04_COMMON/10_DATA_AND_ID_RULES.md` 공통 타입 중 이 펌웨어의 노드 헤더(VSS·CIS·BCM, Pi UART-C 기록)가 쓰는 것을 옮긴 것. DOMAIN 전용 타입 4개(`NetResultConfirmation`, `NetWarningType`, `NetSeverity`, `NetFunctionId`)는 이 노드 헤더에서 제외했다.
- `include/network/app/net_if_vss.h`: `02_NODES/VSS/20_API.md` 원형을 그대로 옮긴 것. 아래 진입 헤더도 include한다.
- `include/network/app/net_if_entry.h`: 공개 진입 `Net_Init`, `Net_MainFunction`. 선언은 VSS 사본(`vss_net_api.h`)과 글자 그대로 같다.
- VSS 앱도 이 헤더를 그대로 쓴다. 별도 사본을 만들면 struct 배치가 어긋날 수 있다.

| 함수 | 구현 | 호출 문맥 |
|---|---|---|
| `void Net_Init(void)` | Network | main, 전원 주기당 1회. 보드 시계가 돈 뒤(VSS: `VssTime_Init` 뒤), `Net_MainFunction` 전 |
| `void Net_MainFunction(uint32_t nowMs)` | Network | main, 슈퍼루프 1바퀴마다 1회. ISR 아님 |
| `VssApp_OnDomainEvent` | VSS 앱 | `Net_MainFunction()` 안에서만(동기 호출). ISR 아님 |
| `VssApp_OnDomainWarningState` | VSS 앱 | `Net_MainFunction()` 안에서만(동기 호출). ISR 아님 |
| `VssNet_PublishStatus` | Network | 슈퍼루프 문맥. 콜백 안에서 불러도 됨(아직 나가지 않은 제출은 최신 것으로 교체). 복사 후 즉시 반환 |

- `Net_Init`/`Net_MainFunction`은 조립 루트 `NetNode_Init`/`NetNode_Step`을 감싸는 얇은 층이다(`src/node/net_entry.c`). `NetNode_*`는 내부와 벤치용으로 남는다.
- BOOT_ID와 TX_SEQUENCE는 Network(M04 `net_upper_publish.c`)가 프레임마다 쓴다. 앱은 채우지 않는다.
- 반환값: 링크·문맥이 준비되지 않음(`Net_Init` 전, bus-off로 링크 잠김) → `NET_SUBMIT_NOT_READY`, 보관 공간 없음 → `NET_SUBMIT_NO_CAPACITY`, 인자 오류 → `NET_SUBMIT_INVALID_ARGUMENT`. VSS는 앞의 둘만 재제출한다.

## VSS 계약 (C1~C12)

VSS 펌웨어(브랜치 `feat/vss-firmware` 커밋 `8fcf25c`, 2026-10-20 S32K344 보드에서 동작 확인)가 Network에 전제하는 조건이다. 코드와 시험 주석은 이 표를 `VSS contract Cn (VSS_NODE_INTEGRATION.md)`로 인용한다. 메시지 필드·배치의 정본은 `docs/network`다.

| # | 계약 | 어기면 VSS에서 생기는 일 |
|---|---|---|
| C1 | **콜백은 main 문맥에서만** 부른다. ISR은 프레임과 수신 tick을 큐에 넣기만 하고 `Net_MainFunction`에서 꺼내 콜백한다 | INPUT/STORE는 재진입을 고려하지 않았다. ISR에서 부르면 상태가 깨진다 |
| C2 | **`received_at_ms`는 VSS 시계 축**이다. ISR에서 `HalTimer_GetTicks()` 값을 기록하고, main에서 `VssTime_TickToMs32(tick)`로 바꾼다(또는 꺼내는 시점의 `VssTime_NowMs32()`). 자체 ms 카운터나 tick×1.25 직접 계산 금지. `VssTime_*`는 main 전용이라 ISR에서 부르지 않는다 | 시계 축이 다르면 Age가 틀어지고, 미래 시각은 쓸 수 없는 시각으로 처리되어 입력이 거부된다 |
| C3 | **TX_SEQUENCE는 CAN ID(메시지)별**로 관리한다. 수용한 송신 BOOT 안에서 1부터, 프레임마다 증가(같은 원본 재전송도 증가). `VSS_EVENT`와 `VSS_WARNING_STATE`는 서로 독립 | VSS는 메시지별로 엄격 증가를 검사한다. 같은 TX의 다른 내용·더 작은 TX는 거부 |
| C4 | **DOMAIN `BOOT_ID`는 기동마다 다른 값**(비휘발 카운터 등), 0 제외, 재사용 금지 | 같은 BOOT로 재기동하면 TX가 1로 돌아가 순서 위반으로 사실상 입력 정지. 옛 BOOT 재사용도 거부(최근 교체된 BOOT 4개를 기억). VSS는 자동 복구하지 않는다 |
| C5 | **구조 검증은 Network가 먼저** 한다: DLC·padding·예약 코드·enum 범위·`activation_meta_format` 규칙(0이면 활성 필드 전부 0, 1이면 meta 형식, 그 외 미지원), BOOT≠0, 메시지별 TX 증가, 완전 중복 폐기, RX 시점까지 Age 누적 | VSS도 다시 검사해 버리지만 1차 책임은 Network. 구조 오류 프레임은 세대 전환도 일으키지 않는다 |
| C6 | **새 DOMAIN BOOT를 수용하면 그 BOOT의 프레임을 그대로 전달**한다. VSS는 `meta.source.boot_id`가 바뀌면 새 세대로 처리한다 | — |
| C7 | **`VssNet_PublishStatus`는 블로킹하지 않는다.** 복사 후 즉시 반환하고 실제 송신은 `Net_MainFunction`/TX 경로에서 한다 | 슈퍼루프가 막히면 오디오 반버퍼(21.3 ms) 마감을 놓쳐 LATE fault → 리셋까지 무음 |
| C8 | **`Net_MainFunction` 1회 실행은 짧게**: 목표 수 ms, 15 ms 초과 금지. 수신 큐는 루프당 처리 상한을 두고 나눠 비운다 | 위와 같음. 현재 VSS 슈퍼루프 최대 3.75 ms라 Network 몫은 여유가 있다 |
| C9 | `DOMAIN_PERMISSION`(0x080)과 `VssNetPriv_OnDomainPermission`은 **Network 내부 처리**다. VSS로 올라오지 않는다 | — |
| C10 | **반환값 의미를 지킨다**: 링크·문맥이 준비 안 됨 → `NET_SUBMIT_NOT_READY`, 보관 공간 없음 → `NET_SUBMIT_NO_CAPACITY`, 인자 오류 → `NET_SUBMIT_INVALID_ARGUMENT` | VSS는 NOT_READY·NO_CAPACITY만 재제출한다. 일시적 실패를 INVALID_ARGUMENT로 돌려주면 그 보고는 버려진다 |
| C11 | **DOMAIN은 `VSS_WARNING_STATE`를 `vss_accepting_events`와 무관하게 주기 송신**한다(50 ms, 감시 150 ms). Event 송신을 accepting으로 막을지는 DOMAIN 선택 | Warning이 150 ms 넘게 끊기면 VSS는 경고 근거 상실로 처리한다 |
| C12 | **품질을 잃은 경고 lane은 마지막 유효 평가의 source sequence를 유지하고 Age를 계속 늘려** 보낸다(새 sequence·Age 0으로 위조 금지, `04_COMMON/10_DATA_AND_ID_RULES.md` 원 성공 평가 보존과 반복·현재 품질) | VSS 결과는 같지만 문서 규칙 위반이다. VSS 시험 입력기는 이미 이 형태로 보낸다 |

## VSS 수신 처리 (VSS 쪽 동작, 시험 기대값의 근거)

VSS 앱이 받은 필드를 다루는 방식이다. Network는 바꾸지 않으며, 통합 하네스(`tests/vss_integ`)의 기대값이 이를 따른다.

- **`VSS_EVENT`:** `event_kind` 1~5만 사용한다(WELCOME·GOODBYE·LOCK_COMPLETE·UNLOCK_COMPLETE·LOCK_ERROR). 중복 판정 키는 `{origin, DOMAIN, boot_id, occurrence_id}`이고, `occurrence_id`는 DOMAIN 한 기동 안에서 단조 증가해야 한다. 사용 한도 L = min(`use_limit_ms`, 500, 2000) ms이고, `use_limit_ms`·`occurrence_id`·`tx_sequence`가 0이면 거부한다. 같은 occurrence 재전송은 사용 한도 안이면 중복, 뒤면 기한 초과로 거부되어 다시 울리지 않는다. `original_age_ms`가 65535(포화)인 Event는 AGE가 불완전하므로 재생하지 않는다. VSS 기동 전에 발생한 Event는 재생하지 않는다(`02_NODES/VSS/10_DATA.md` 입력 연결·시간·복구의 한정 계약).
- **`VSS_WARNING_STATE`:** lane은 후방 활성, 후방 위험(CLEAR/CAUTION/EMERGENCY), 탑승자, 끼임이다. `*_source_boot`/`*_source_sequence`가 둘 다 0이 아니어야 출처가 있는 근거다. quality OK + reason NONE이면 확정, 그 밖은 미확정이다. 끼임 경고는 PRODUCT origin(실제 CAN)에서 재생하지 않는다.
  - TTL / Hold: 끼임 250/250, 탑승자 1000/500, 후방 150/150, 후방 활성 500/0 ms.
  - 우선순위: 끼임 9 > 후방 비상 8 > 탑승자 7 > 후방 주의 6 > 잠금 오류 5 > 해제 4 > 잠금 3 > Goodbye 2 > Welcome 1.
- **DOMAIN 재부팅:** `boot_id`가 바뀌면 새 세대로 입력을 다시 연다. 옛 세대의 미재생 Event는 재생 기회를 잃고, 옛 세대의 늦은 프레임은 새 세대를 닫지 않고 거부된다.

## VSS 연동 확인 시험

VSS 펌웨어와 Network를 결합한 빌드에서 확인할 항목이다. host에서는 `tests/vss_integ`(아래 "VSS 통합 하네스")가 같은 항목을 돌린다.

- DOMAIN 재부팅(BOOT 변경) 뒤 입력 재개, 옛 BOOT 늦은 프레임 거부.
- Event·Warning TX가 서로 독립(한쪽이 앞서도 다른 쪽 수용).
- 같은 occurrence 재전송이 다시 울리지 않음.
- Warning 150 ms 끊김 → 경고 상실·Hold 뒤 정지.
- `VSS_STATUS`가 250 ms 주기 + 변화 시 즉시 나가고, `update_sequence`가 증가.
- 슈퍼루프 1바퀴 최대 시간(`g_vssMain.loopMaxTicks`, 1 tick = 1.25 ms)이 12 tick 미만, 오디오 fault 0.

## VSS_STATUS(0x340) 송신 규칙

- **0x340은 앱이 주기적으로 제출하고, Network는 반복하지 않는다.** VSS 앱은 상태가 바뀌면 즉시, 아니면 250 ms마다 새 평가(새 `update_sequence`)로 제출한다. Network는 제출마다 프레임 1건을 보낸다. M04 주기 표에 0x340을 등록하지 않는다. 등록하면 같은 주기에 프레임이 두 번 나간다.
- TX_SEQUENCE는 1부터 프레임마다 +1이다. `0xFFFFFFFF`를 쓴 뒤에는 송신을 멈추고(HOLD) `sequence_hold`를 올리며, 제출은 `NET_SUBMIT_NOT_READY`를 받는다.
- **대기 Age:** 제출 시각부터 실제로 장치에 넘기는 시각까지 기다린 시간을 SOURCE_AGE에 더한다(65535 포화). 제출 시각은 마지막 `Net_MainFunction`의 `nowMs`이고, 보통 다음 `Net_MainFunction()`에서 바로 나가므로 더해지는 것은 루프 1바퀴 정도다. 직전 0x340 프레임이 아직 TX MB에 있으면 새 제출은 그 프레임이 끝날 때까지 기다리고, 기다린 시간이 더해진다.
- 진단: `NetNodeDiag.status_tx`(M04 슬롯). `sent`, `replaced`, `pending_age_added_ms_max`, `sequence_hold`를 본다. `periodic_sent`는 항상 0이다.

## DOMAIN 재부팅과 같은 평가의 반복 (수신)

- **재부팅:** DOMAIN이 재부팅해 새 BOOT_ID로 TX 1부터 보내면, CAN ID(0x160, 0x0C0, 0x080)마다 그 ID의 첫 새 BOOT 프레임에서 새 세대로 바꾸고 그대로 콜백한다. 구조 검증(DLC, padding, 예약 코드, enum, BOOT≠0, TX_SEQUENCE≠0)을 통과한 프레임만 바꾼다. TX_SEQUENCE 0은 예약값이다(`10_CAN_FD.md` TX_SEQUENCE "최초 1"). 그래서 구조 오류로 버린다(`rx_invalid`, 콜백 없음). 앞 세대의 문맥(TX 순서, 같은 관측 기억, AGE 비교 기준, permission 감독 기록)은 그 ID에서만 버린다. 콜백이 거절한 프레임은 세대를 바꾸지 않는다.
- **이전 BOOT로는 돌아갈 수 없다.** 끝난 BOOT는 CAN ID마다 최근 4개를 기억하고, 그 BOOT의 늦은 프레임은 콜백 없이 버린다(`NetNodeDiag.upper.old_generation`, `stale`에도 포함). 바뀐 횟수는 `generation_changes`다. DOMAIN이 같은 BOOT_ID로 다시 시작하면 TX가 1로 돌아가 과거 TX로 버려지므로 복구되지 않는다(VSS 계약 C4: BOOT_ID는 기동마다 다르다).
- **같은 평가의 반복:** 수신 중단 감시가 있는 주기 메시지(0x0C0 `VSS_WARNING_STATE`, 0x080)는 같은 원 관측을 새 TX로 반복해도 콜백한다. 원 관측 번호는 그대로이고 AGE는 늘어난 값이다. VSS INPUT은 TX가 앞선 프레임으로 150 ms 감시를 갱신한다(C11). 정확한 중복(같은 TX, 같은 바이트)은 버린다. 반복은 `NetNodeDiag.upper.repeat_delivered`로 세고 `rx_delivered`·`duplicates`에는 넣지 않는다.
- **늦게 도착한 반복:** 반복 프레임이 버스 중재 등으로 늦어 AGE가 "직전 AGE + 수신 간격"보다 5 ms 이내로 모자라면, AGE를 그 하한으로 올려 전달한다(`repeat_age_raised`). 낮추지는 않는다. 5 ms는 문서의 CAN 전달 여유 후보(PROPOSED)다. 더 모자라면 보류한다(`rx_held`).
- `VSS_EVENT`(0x160)는 발생 메시지라 수신 중단 감시가 없다. 같은 occurrence를 새 TX로 다시 보내도 두 번 전달하지 않는다.

## 공개 진입과 시각

### `Net_Init()`

- 인자가 없으므로 BOOT_ID와 epoch는 보드 hook `NET_BOARD_NODE_CONFIG_HOOK`에서 얻는다. 원형은 `bool f(NetNodeConfig *out)`이다.
  - hook에 들어올 때 `out`은 `boot_id` 0, epoch(로컬, CAN, UART-M, UART-C) 모두 1이다. 프로필에 없는 UART 링크의 epoch는 쓰지 않는다. hook은 0이 아니고 재부팅마다 바뀌는 BOOT_ID를 넣고 true를 반환한다. epoch는 그대로 두어도 된다.
  - false를 반환하거나 BOOT_ID가 0이면 초기화하지 않는다. 이유는 `NetNodeDiag.entry_init_result`에 남는다(`CONFIG_REFUSED`, `BOOT_ID_ZERO`, hook 없이 빌드하면 `NO_CONFIG_HOOK`, 조립 실패면 `NODE_FAILED`와 `stage`/`stage_status`).
- 실제로 동작하는 것은 첫 호출뿐이다. 두 번째 호출부터는 `entry_init_calls`만 올리고 아무것도 바꾸지 않는다(실패한 첫 호출도 다시 시도하지 않는다).
- 초기화가 실패하면 `Net_MainFunction`은 아무것도 하지 않고, `VssNet_PublishStatus`는 `NET_SUBMIT_NOT_READY`를 반환한다.

### `Net_MainFunction(nowMs)`

- `nowMs`는 앱의 ms 시계다(VSS: `VssTime_NowMs32()`). 뒤로 가면 안 되고 32비트 wrap은 허용한다. Network는 이를 64비트로 확장해 현재 시각으로 쓴다. 이전 값보다 작으면 시각을 그대로 두고 `entry_now_backward`를 올린다.
- 순서: 현재 시각 등록 → ISR 링 비우기(상한 있음) → 수신 처리와 콜백 → permission 감독 → TX 감시(100 ms 무응답 abort) → M04 송신.
- `Net_Init` 성공 전에는 아무것도 하지 않는다(`entry_main_calls`만 올린다).

### ISR → main 인계 (`net_board_cfg.h`)

- CAN ISR은 Network 코어를 부르지 않는다. 수신 프레임, TX 완료, bus-off 사실을 **원시 시각과 함께 backend 내부 ISR 링**에만 넣는다. `net_hal_capture_*` 호출과 ms 변환은 모두 `Net_MainFunction` 안(main 문맥)에서 한다.
- 링 크기 `NET_BOARD_CAN_ISR_RING`(기본 32)은 두 main 바퀴 사이에 쌓일 수 있는 수신 프레임 수다. TX 완료와 bus-off는 따로 자리를 잡아 두어 버려지지 않는다.
- 링이 차서 들어오지 못한 수신 프레임은 버리고 `isr_ring_overflow`를 올린다. MB overrun(그 MB의 앞 프레임이 덮어써짐)은 `rx_overruns`로 센다. 손실 뒤 처음 링에 들어간 프레임은 그 자체로 온전하므로 받은 그대로 전달하고, 손실 경계는 `rx_loss_events`로 한 번 센다(`docs/network/sw_detail/functions/CAN_PORT_RECOVERY.md` RX overflow: 실제 손실만 기록, 합성 없음, 결과 OK). 링크는 잠그지 않는다.
- 한 바퀴에 링에서 꺼내는 수는 `NET_BOARD_ISR_DRAIN_BUDGET`(기본 16, 어댑터 RX 큐 16 이하)으로 제한한다. 나머지는 다음 바퀴에서 꺼낸다. `Net_MainFunction` 1회 실행을 짧게(수 ms, 15 ms 미만) 유지하기 위한 것이다.

### 시간원: 셋 중 하나

| 방식 | 정의 | ISR이 기록하는 값 | main에서 ms로 | Network 현재 시각 |
|---|---|---|---|---|
| A. 앱 시계(보드 빌드) | `NET_BOARD_ISR_TICK_HOOK`과 `NET_BOARD_TICK_TO_MS_HOOK` 둘 다 | `NET_BOARD_ISR_TICK_HOOK()` tick | `NET_BOARD_TICK_TO_MS_HOOK(tick)` | 마지막 `Net_MainFunction`의 `nowMs`(64비트 확장). 자체 ms 카운터 없음 |
| B. 보드 ms hook(벤치·host) | `NET_BOARD_NOW_MS_HOOK` | 그 hook의 ms | 항등 | hook 값 |
| C. OsIf(기본) | 없음. OsIf "Use System Timer" ON 필요 | OsIf 기반 ms | 항등 | OsIf 기반 ms |

- 원형: `uint32_t NET_BOARD_ISR_TICK_HOOK(void)`(ISR에서 불림, 값만 읽어야 함), `uint32_t NET_BOARD_TICK_TO_MS_HOOK(uint32_t tick)`(main 전용), `uint64_t NET_BOARD_NOW_MS_HOOK(void)`(ISR과 main 양쪽).
- 방식 A에서 변환한 32비트 ms는 현재 `nowMs`를 기준으로 64비트 축에 놓는다. 현재보다 미래면(앱이 `nowMs`를 읽은 뒤 같은 바퀴 안에서 들어온 프레임) 현재 시각으로 두고 `tick_future_clamped`를 올린다. 수 건씩 오르는 것은 정상이다.
- tick hook을 하나만 정의하거나, tick hook과 `NET_BOARD_NOW_MS_HOOK`을 함께 정의하면 컴파일 오류다. B·C에서는 `Net_MainFunction`의 `nowMs`를 현재 시각으로 쓰지 않는다(각자 시계를 쓴다).
- 방식 A에서는 `Net_Init`부터 첫 `Net_MainFunction` 전까지 Network 시각이 0이다. 그 사이에 받은 프레임은 첫 `Net_MainFunction`에서 제 시각으로 놓인다. 그 사이에 `VssNet_PublishStatus`를 부르면 대기 Age가 첫 `nowMs`만큼 크게 잡힌다(VSS는 루프 안에서만 제출하므로 해당 없음).
- 선택 hook `NET_BOARD_US_HOOK`(`uint32_t f(void)`, µs 자유 증가 카운터)을 정의하면 `Net_MainFunction` 1회 최대 실행 시간을 `NetNodeDiag.main_max_us`에 남긴다. 없으면 0이다.

## VSS 연동 절차

1. **헤더 교체:** VSS 소스의 `#include "vss_net_api.h"`를 `#include "network/app/net_if_vss.h"`로 바꾼다(include 경로에 `firmware/network/include` 추가). 두 헤더를 한 번역 단위에서 함께 include하지 않는다(같은 타입을 정의한다).
2. **Network 대역 끄기:** `VSS_CFG_NET_STUB=0`으로 빌드해 `net_stub.c` 대신 Network 소스(`tools/s32ds_sources.txt` 목록)를 링크한다.
3. **hook define 3개**(프로젝트 설정의 `-D` 또는 `net_board_cfg.h`):
   - `NET_BOARD_ISR_TICK_HOOK=HalTimer_GetTicks`: PIT ISR이 올리는 `volatile` 카운터를 읽기만 하므로 CAN ISR에서 불러도 된다(1 tick = 1.25 ms).
   - `NET_BOARD_TICK_TO_MS_HOOK=VssTime_TickToMs32`: main 전용. Network는 `Net_MainFunction` 안에서만 부른다.
   - `NET_BOARD_NODE_CONFIG_HOOK=<VSS 쪽 함수>`: VSS가 만든다. 예시는 아래.
   - 선택: `NET_BOARD_US_HOOK=<µs 카운터 함수>`
4. **부팅 순서:** `HalTimer_Init` → `VssTime_Init(HalTimer_GetTicks)` → … → `Net_Init()` → 오디오 시작. 슈퍼루프는 지금처럼 `Net_MainFunction(VssTime_NowMs32())`를 부른다.
5. VSS 노드 BOOT_ID의 출처(NVM 카운터, noinit 카운터)는 아직 정해지지 않았다. 시험대에서는 상수를 쓴다.

```c
/* VSS 쪽 노드 설정 hook 예시. 이 파일은 network/net_node.h만 include한다. */
#include "network/net_node.h"

bool VssNetCfg_GetNodeConfig(NetNodeConfig *out)
{
    out->boot_id = VssBoot_NextBootId(); /* 0이 아니고 재부팅마다 다른 값 */
    return out->boot_id != 0u;
}
/* 빌드 설정: -DNET_BOARD_NODE_CONFIG_HOOK=VssNetCfg_GetNodeConfig */
```

## VSS 없는 보드 벤치 (방식 A 예)

SysTick 1 kHz 인터럽트가 올리는 ms 카운터를 tick으로 쓰고, 변환은 항등으로 둔다. `SysTick_Handler`는 시작 코드에 weak로 정의되어 있어 앱에서 재정의할 수 있다.

```c
#include "network/net_node.h"                 /* NetNodeConfig */
#include "network/app/net_if_vss.h"           /* Net_Init, Net_MainFunction, VssNet_PublishStatus */

static volatile uint32_t s_benchMs;           /* SysTick 1 kHz */
void SysTick_Handler(void) { ++s_benchMs; }

uint32_t Bench_Ticks(void) { return s_benchMs; }            /* ISR에서 읽기만 */
uint32_t Bench_TickToMs(uint32_t tick) { return tick; }     /* 1 tick = 1 ms */
bool Bench_NodeConfig(NetNodeConfig *out) { out->boot_id = 0x42454E31u; return true; } /* 시험대 상수 */

/* -DNET_BOARD_ISR_TICK_HOOK=Bench_Ticks -DNET_BOARD_TICK_TO_MS_HOOK=Bench_TickToMs
 * -DNET_BOARD_NODE_CONFIG_HOOK=Bench_NodeConfig */
int main(void)
{
    /* Clock/Port/IntCtrl 초기화, SysTick 1 kHz 시작 */
    Net_Init();                                /* FlexCAN 시작 */
    for (;;) {
        Net_MainFunction(s_benchMs);
        Bench_App_Service();                   /* 예: 250 ms마다 VssNet_PublishStatus */
    }
}
```

`NetNode_Init(&cfg)`/`NetNode_Step()`을 직접 쓰는 기존 방식도 B·C 시간원에서는 그대로 동작한다. 방식 A에서는 Network 시각이 `Net_MainFunction`으로만 흐르므로 반드시 `Net_MainFunction`을 쓴다.

## 보드 프로젝트에 넣을 소스

- 제품 C 소스 목록의 정본은 `tools/s32ds_sources.txt`다. host 빌드(`NETWORK_BUILD_RTD_MOCK`)도 같은 목록을 읽어 링크까지 확인한다.
- RTD backend는 세 파일이다. `src/platform/rtd/net_hal_rtd.c`(시간원, ISR 시각 변환, 임계구역, 시작 순서, backend ops 등록, 링 비우기 진입), `src/platform/rtd/net_hal_rtd_can.c`(FlexCAN RX/TX/error ISR → ISR 링, 링 → 어댑터, TX MB, abort), `src/platform/rtd/net_hal_rtd_uart.c`(LPUART: 1바이트 연속 수신 → 링크별 바이트 링, 링 → 어댑터, 링크당 송신 1건, 송신 시간 초과 abort). 서로 비공개 헤더 `net_hal_rtd_internal.h`로만 연결된다.
- `net_hal_rtd_uart.c`는 프로필에 UART 링크가 있을 때만 내용이 컴파일된다(UART-M: HOST·DOMAIN, UART-C: HOST·CIS). VSS 프로필에서는 빈 파일이므로 목록에 그대로 두어도 LPUART 심볼을 참조하지 않는다.
- 공개 진입은 `src/node/net_entry.c`다. 보드 hook을 쓰므로 `net_board_cfg.h`와 같은 define으로 컴파일한다.
- 조립 루트는 공통 부분 `src/node/net_node.c`와 노드 프로필 표 `src/node/net_node_profile_<node>.c`(VSS, CIS, BCM)로 나뉜다. App facade(`net_app_port_<node>.c`)와 프로필 표는 빌드의 `NETWORK_UPPER_NODE_PROFILE`이 자기 노드일 때만 내용이 컴파일되므로, 목록 전체를 그대로 넣어도 다른 노드의 심볼(`CisApp_*` 등)을 참조하지 않는다.
- RTD 헤더를 include하는 Network 파일은 `src/platform/rtd/` 안에만 있다.

## 보드 설정 요구 (`src/platform/rtd/net_board_cfg.h`)

- ConfigTools의 FlexCAN 콜백 이름(`NET_BOARD_CAN_CALLBACK`, `NET_BOARD_CAN_ERROR_CALLBACK`)은 HAL이 정의한다. 같은 이름을 정의하는 다른 파일은 빌드에서 빼야 한다.
- MB 배치 기본값: RX 0..3(마스크 0, 모든 표준 ID 수신 후 registry로 걸러냄), TX 8..11. `max_num_mb`(344=16) 안에 들어가야 한다.
- TX MB가 `NET_BOARD_CAN_TX_TIMEOUT_MS`(기본 100 ms) 안에 끝나지 않으면 `FlexCAN_Ip_AbortTransfer`로 abort하고 그 반환값대로 정산한다. `NO_TRANSFER_IN_PROGRESS`는 abort 전에 프레임이 이미 나간 경우라(RTD는 이때 TX_COMPLETE를 부르지 않는다) 성공으로 정산한다. `TIMEOUT`은 abort가 끝나지 않은 경우라 실패로 보고하고, 그 MB는 `FlexCAN_Ip_GetBuffStatusFlag`가 설 때까지(컨트롤러가 MB를 놓을 때까지) 다시 쓰지 않는다. RTD는 TIMEOUT 뒤 그 플래그를 지우므로, 그 직전에 끝난 MB는 플래그가 다시 서지 않아 격리가 풀리지 않을 수 있다(`tx_abort_timeouts`와 `tx_quarantine_released`의 차이로 관찰, 리셋까지 TX MB 하나가 준다). 그 밖의 결과는 실패로 보고한다.
- 시간원은 위 표의 A·B·C 중 하나다. 보드 빌드는 A를 쓴다. 셋 다 없으면(OsIf 타이머 OFF, hook 없음) 컴파일 오류(`OSIF_COUNTER_SYSTEM` 미정의)가 난다.
- ISR 링: `NET_BOARD_CAN_ISR_RING`(기본 32, 1 이상), `NET_BOARD_ISR_DRAIN_BUDGET`(기본 16, 1 이상 16 이하). 범위를 벗어나면 컴파일 오류다.
- UART 링크(CIS·DOMAIN 프로필): `NET_BOARD_UART_C_INSTANCE`/`NET_BOARD_UART_C_CONFIG`(기본 11 / `Lpuart_Uart_Ip_xHwConfigPB_11`), `NET_BOARD_UART_M_INSTANCE`/`NET_BOARD_UART_M_CONFIG`(DOMAIN 보드 미정, 자리 표시 기본값 0 / `Lpuart_Uart_Ip_xHwConfigPB_0`), 링크별 송신 시간 초과(기본 100 ms), `NET_BOARD_UART_ISR_RING`(기본 256 B). 콜백은 `NET_BOARD_UART_CALLBACK`을 정의하면 HAL이 그 이름으로 정의하고, 정의하지 않으면 보드 콜백이 Network 인스턴스 이벤트를 `NetHalRtdUart_OnEvent()`(`net_hal_rtd_uart.h`)로 넘긴다(DS344: LPUART0 콘솔과 LPUART11이 `Uart344_BringupCallback`을 공유하므로 이 방식).
- LPUART 이벤트: ERROR(수신 오류)와 IDLE_STATE(수신선 idle, 생성 설정 `LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT`가 ON일 때만 존재)는 RTD가 수신을 끝낸 뒤 오므로, 두 경우 모두 1바이트 수신을 다시 건다. IDLE은 손실이 아니라서 손실 표시를 남기지 않고 `uart_rx_idle_events`만 센다(시험 `uart_hal.u15_idle_event_rearm`). 두 브링업 설정은 OFF다. 송신 시간 초과의 `Lpuart_Uart_Ip_AbortSendingData` 반환값은 `uart_tx_abort_last_status`에 남고, SUCCESS가 아니면 `uart_tx_abort_errors`를 올린다(송신은 어느 경우든 실패로 보고).
- 한 바퀴에 비우는 양은 CAN 먼저, 그다음 UART 바이트로 `NET_BOARD_ISR_DRAIN_BUDGET`을 함께 쓴다. M06은 시각 검사보다 이전에 수신된 바이트를 받지 않는다(수신기 poll 계약). 그래서 서비스 Port(`net_hal_rtd_port()`, `net_node.c`가 쓴다)는 서비스가 읽은 시각 이전에 수신되어 UART 링에 남은 바이트가 있으면 "할 일 없음"을 알리지 않고 그 바이트를 넘긴다(poll 1회에 상한 이하, 서비스 step당 poll 32회). 바이트는 UART 링(256 B, 115200 bit/s 가득 찬 선로에서 약 22 ms)이 넘칠 때만 잃는다. main 주기는 링 시간보다 짧게, 선로 평균 부하는 바퀴당 64 B 이하로 둔다(예: 3 ms 주기 시험 `uart_hal.u13_main_period_3ms`, `cis_node.f16_vision_rx_main_3ms`).
- BOOT_ID는 0이 아니어야 하고 재부팅마다 바뀌어야 한다. `Net_Init`을 쓰면 `NET_BOARD_NODE_CONFIG_HOOK`이 주고, `NetNode_Init`을 직접 쓰면 `NetNodeConfig.boot_id`로 준다.

## 진단 (`NetNode_GetDiag`)

| 필드 | 뜻 |
|---|---|
| `entry_init_result`, `entry_init_calls`, `entry_main_calls` | `Net_Init` 결과와 진입 호출 수 |
| `entry_now_ms`, `entry_now_backward` | 64비트로 확장한 마지막 `nowMs`, 뒤로 간 `nowMs` 수 |
| `main_max_us` | `Net_MainFunction` 1회 최대 실행 시간(µs, `NET_BOARD_US_HOOK`이 있을 때만) |
| `rtd.time_source` | 0 OsIf, 1 `NET_BOARD_NOW_MS_HOOK`, 2 앱 시계(tick hook) |
| `rtd.isr_ring_capacity`, `rtd.isr_drain_budget` | 빌드한 링 크기와 바퀴당 상한 |
| `rtd.isr_ring_overflow`, `rtd.isr_ring_max_depth` | 링이 차서 버린 수신 프레임 수, 링에 동시에 쌓인 최대 항목 수 |
| `rtd.rx_overruns`, `rtd.rx_loss_events` | MB overrun 수, 손실 직후 링에 들어간(그리고 전달된) 프레임 수(손실 경계) |
| `rtd.uart_poll_drained` | 서비스 Port poll이 시각 검사 전에 UART 링에서 넘긴 항목 수 |
| `rtd.main_drained_max` | 한 바퀴에 링에서 꺼낸 최대 항목 수 |
| `rtd.tick_future_clamped` | 현재 시각보다 미래라 현재 시각으로 둔 캡처 수 |
| `rtd.tx_timeout_aborts` | TX MB가 100 ms 안에 끝나지 않아 abort한 수 |
| `rtd.tx_abort_already_sent` | 그중 abort가 `FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS`(abort 전에 이미 송신됨, TX_COMPLETE 없음)라 성공으로 정산한 수 |
| `rtd.tx_abort_timeouts`, `rtd.tx_quarantine_released` | 그중 abort가 `FLEXCAN_STATUS_TIMEOUT`이라 실패로 보고하고 그 MB를 격리한 수, 격리한 MB의 플래그가 서서 다시 쓰게 된 수 |
| `rtd.tx_abort_last_status` | 마지막 abort의 RTD 반환값 |

## VSS 통합 하네스 (host, `tests/vss_integ`)

실제 VSS 소스(`VssMain_Boot`·`VssMain_LoopOnce`, VSS HAL 대역 `tests/host/hal_mock.c`)를 이 Network 노드(VSS 프로필, mock RTD, 방식 A 시계)와 링크해 위 "VSS 연동 확인 시험"과 VSS 계약(C1~C12)을 host에서 돌린다. VSS 파일은 읽기만 하고 리포에 복사하지 않는다.

- **VSS 소스:** VSS 브랜치 `feat/vss-firmware` 커밋 `8fcf25c`의 worktree(아래 `<VSS_WT>`, 리포 밖 폴더)에서 `firmware/vss` 폴더를 쓴다. 경로에는 `/`를 쓴다.
  ```
  git fetch origin feat/vss-firmware
  git worktree add --detach <VSS_WT> 8fcf25c
  ```
- **빌드·실행:** VSS 프로필 빌드 옵션에 폴더 하나를 더한다. 폴더나 `src/net_if/vss_net_api.h`가 없으면 configure가 멈춘다.
  ```
  cmake -S firmware/network -B firmware/network/.build-vss-integ -G Ninja \
    -DBUILD_TESTING=ON -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON \
    -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON -DNETWORK_NODE_PROFILE=VSS -DNETWORK_BUILD_RTD_MOCK=ON \
    -DNETWORK_VSS_FW_DIR=<VSS_WT>/firmware/vss
  cmake --build firmware/network/.build-vss-integ
  ctest --test-dir firmware/network/.build-vss-integ -R vss_integ
  ```
- **구성:** VSS 라이브러리 `vss_fw_host`(`src/**/*.c`에서 `hal/s32k344/` 제외 + `hal_mock.c`, VSS `host_test.py`와 같은 플래그 + `VSS_CFG_NET_STUB=0 VSS_CFG_TEST_INPUT=0`), Network 라이브러리 `network_node_vss_integ`(`tools/s32ds_sources.txt` 목록 + mock SDK, `NET_BOARD_ISR_TICK_HOOK=HalTimer_GetTicks`, `NET_BOARD_TICK_TO_MS_HOOK=VssTime_TickToMs32`, 시험용 노드 설정·µs hook, `-funsigned-char`). GCC/Clang 전용이다(`-Wl,--wrap`).
- **시험 프로그램:** `vss_net_api.h`와 `net_if_*.h`는 같은 타입을 정의하므로 두 번역 단위로 나눈다. `test_vss_integ.c`는 VSS 헤더만, `vss_integ_bus.c`(DOMAIN 프레임 codec, 버스, 0x340 해독, Network 진단)는 Network 헤더만 include하고, 둘은 `vss_integ_bus.h`(기본 C 타입)로만 만난다. 슈퍼루프는 tick(1.25 ms)마다 1바퀴, 오디오 출력 경계는 48 kHz 1024 frame마다 올린다. `VssApp_On*`·`VssNet_PublishStatus`는 link spy(`--wrap`)로 기록한 뒤 실제 함수를 부른다.
- **케이스(`vss_integ.*`, 케이스당 프로세스 1개):** vi1 부팅 순서, vi2 수신 시각 축, vi3 메시지별 TX, vi4 DOMAIN 재부팅, vi5 같은 occurrence 재전송, vi6 구조 오류 선검사, vi7 Warning 수신 끊김, vi8 0x340 주기(10 s에 40±2건), vi9 링크 잠김 재제출, vi10 수신 폭주 처리 상한, vi11 한 lane의 품질 하향, vi12 같은 평가 1초 반복(도착 0~2.5 ms 흔들림)에도 경고 유지.

## 제한

- 버스오프 자동 복구는 없다. bus-off가 나면 CAN 링크가 잠기고, 보드를 리셋해야 다시 송수신한다.
- DOMAIN이 같은 BOOT_ID로 재기동하면 복구되지 않는다(위 "DOMAIN 재부팅" 절). 끝난 BOOT 기억은 CAN ID마다 4개라, 4번 넘게 재부팅한 뒤 아주 오래된 BOOT의 프레임이 다시 오면 새 세대로 받는다.
- VSS 노드는 UART 링크와 다른 노드의 facade를 쓰지 않는다.
