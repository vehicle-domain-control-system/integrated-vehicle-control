# VSS 노드 Network 통합 메모

VSS 노드(profile 6) 펌웨어에 Network를 붙일 때 지켜야 할 사항만 모았다. 설계 근거는 `docs/network/`이다. VSS 쪽 전제는 VSS 펌웨어의 `src/net_if/vss_net_api.h`, `src/app/vss_main.c`, `src/infra/vss_time.h`, `README.md` §6·§7이다.

## App 경계 (정본 헤더)

- `include/network/app/net_if_common.h`: `04_COMMON` 공통 타입을 그대로 옮긴 것.
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

## VSS_STATUS(0x340) 송신 규칙

- **0x340은 앱이 주기적으로 제출하고, Network는 반복하지 않는다.** VSS 앱은 상태가 바뀌면 즉시, 아니면 250 ms마다 새 평가(새 `update_sequence`)로 제출한다. Network는 제출마다 프레임 1건을 보낸다. M04 주기 표에 0x340을 등록하지 않는다. 등록하면 같은 주기에 프레임이 두 번 나간다.
- TX_SEQUENCE는 1부터 프레임마다 +1이다. `0xFFFFFFFF`를 쓴 뒤에는 송신을 멈추고(HOLD) `sequence_hold`를 올리며, 제출은 `NET_SUBMIT_NOT_READY`를 받는다.
- **대기 Age:** 제출 시각부터 실제로 장치에 넘기는 시각까지 기다린 시간을 SOURCE_AGE에 더한다(65535 포화). 제출 시각은 마지막 `Net_MainFunction`의 `nowMs`이고, 보통 다음 `Net_MainFunction()`에서 바로 나가므로 더해지는 것은 루프 1바퀴 정도다. 직전 0x340 프레임이 아직 TX MB에 있으면 새 제출은 그 프레임이 끝날 때까지 기다리고, 기다린 시간이 더해진다.
- 진단: `NetNodeDiag.status_tx`(M04 슬롯). `sent`, `replaced`, `pending_age_added_ms_max`, `sequence_hold`를 본다. `periodic_sent`는 항상 0이다.

## 공개 진입과 시각

### `Net_Init()`

- 인자가 없으므로 BOOT_ID와 epoch는 보드 hook `NET_BOARD_NODE_CONFIG_HOOK`에서 얻는다. 원형은 `bool f(NetNodeConfig *out)`이다.
  - hook에 들어올 때 `out`은 `boot_id` 0, 두 epoch 1이다. hook은 0이 아니고 재부팅마다 바뀌는 BOOT_ID를 넣고 true를 반환한다. epoch는 그대로 두어도 된다.
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
- 링이 차서 들어오지 못한 수신 프레임은 버리고 `isr_ring_overflow`를 올린다. 그 손실은 다음에 링에 들어간 프레임의 캡처 `overflow`로 표시된다. 손실이 표시된 그 프레임은 엔진이 정상 프레임처럼 전달하지 않는다. 링크는 잠그지 않는다.
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
- RTD backend는 두 파일이다. `src/platform/rtd/net_hal_rtd.c`(시간원, ISR 시각 변환, 임계구역, 시작 순서, backend ops 등록, 링 비우기 진입)와 `src/platform/rtd/net_hal_rtd_can.c`(FlexCAN RX/TX/error ISR → ISR 링, 링 → 어댑터, TX MB, abort). 둘은 비공개 헤더 `net_hal_rtd_internal.h`로만 연결된다.
- 공개 진입은 `src/node/net_entry.c`다. 보드 hook을 쓰므로 `net_board_cfg.h`와 같은 define으로 컴파일한다.
- RTD 헤더를 include하는 Network 파일은 `src/platform/rtd/` 안에만 있다.

## 보드 설정 요구 (`src/platform/rtd/net_board_cfg.h`)

- ConfigTools의 FlexCAN 콜백 이름(`NET_BOARD_CAN_CALLBACK`, `NET_BOARD_CAN_ERROR_CALLBACK`)은 HAL이 정의한다. 같은 이름을 정의하는 다른 파일은 빌드에서 빼야 한다.
- MB 배치 기본값: RX 0..3(마스크 0, 모든 표준 ID 수신 후 registry로 걸러냄), TX 8..11. `max_num_mb`(344=16) 안에 들어가야 한다.
- 시간원은 위 표의 A·B·C 중 하나다. 보드 빌드는 A를 쓴다. 셋 다 없으면(OsIf 타이머 OFF, hook 없음) 컴파일 오류(`OSIF_COUNTER_SYSTEM` 미정의)가 난다.
- ISR 링: `NET_BOARD_CAN_ISR_RING`(기본 32, 1 이상), `NET_BOARD_ISR_DRAIN_BUDGET`(기본 16, 1 이상 16 이하). 범위를 벗어나면 컴파일 오류다.
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
| `rtd.main_drained_max` | 한 바퀴에 링에서 꺼낸 최대 항목 수 |
| `rtd.tick_future_clamped` | 현재 시각보다 미래라 현재 시각으로 둔 캡처 수 |

## 제한

- 버스오프 자동 복구는 없다. bus-off가 나면 CAN 링크가 잠기고, 보드를 리셋해야 다시 송수신한다.
- VSS 노드는 UART 링크와 다른 노드의 facade를 쓰지 않는다.
