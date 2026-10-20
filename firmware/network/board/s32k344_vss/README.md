# VSS 보드 프로젝트 연결 파일 (S32K344)

실제 VSS S32DS 프로젝트에 Network를 붙일 때 Network 쪽에서 넣는 보드 파일이다. VSS 소스에는 넣지 않는다.

| 파일 | 내용 |
|---|---|
| `net_vss_board.c/.h` | `Net_Init` 노드 설정 hook `VssBoard_NetNodeConfig` |

- hook은 Network 타입 `NetNodeConfig`를 쓰므로 Network 쪽에 둔다. `network/net_node.h`만 include하고 `vss_net_api.h`는 include하지 않는다. VSS는 자기 `vss_net_api.h`를, Network는 `network/app/net_if_*.h`를 쓰고 둘은 링크에서 만난다(두 헤더 모델). 두 헤더의 배치가 같은지는 host 시험 `vss_integ.vi0_api_layout`이 확인한다.

## VSS 프로젝트에 넣는 법

1. NET_git에서 dry run으로 확인한 뒤 `--apply`로 복사한다. Network 소스는 `src/network/`, 이 폴더는 `src/net_board/`로 들어간다.
   ```
   python firmware/network/tools/sync_to_s32ds.py --project <VSS 프로젝트> --board vss
   python firmware/network/tools/sync_to_s32ds.py --project <VSS 프로젝트> --board vss --apply
   ```
2. `.cproject`(Debug_FLASH C 컴파일러)의 include 7개와 define 목록은 `tools/sdk_swap_check.py --vss <VSS 프로젝트>` 보고서(`summary.md`)의 "VSS 프로젝트 설정" 절을 그대로 옮긴다. 시간원은 방식 A(`HalTimer_GetTicks` / `VssTime_TickToMs32`), 빌드 스위치는 `VSS_CFG_NET_STUB=0`, `VSS_CFG_TEST_INPUT=0`이다.

## µs hook을 넣지 않은 이유

`NET_BOARD_US_HOOK`은 정의하지 않는다. VSS에는 µs 자유 카운터가 없고, DWT 사이클 카운터를 160으로 나눈 값은 약 26.8초마다 끊긴다. 그래서 `NetNodeDiag.main_max_us`는 0으로 남는다. 슈퍼루프 시간은 VSS 자체 `loopMaxTicks`로 본다.

## BOOT_ID (HOLD)

BOOT_ID 출처(NVM, noinit 카운터)는 아직 정하지 않았다. 지금은 시험대 상수 0x56535331이다(S32K344 벤치의 0x344와 다르다). 상수이므로 리셋하면 같은 BOOT로 TX가 1부터 다시 시작하고, 수신 쪽은 이를 옛 프레임으로 거부한다(VSS_NODE_INTEGRATION.md 계약 C4).
