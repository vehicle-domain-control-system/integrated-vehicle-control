# Transport

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Function](../../03_FUNCTION_INDEX.md)

기존 [Layer 책임](../../../network_sw_architecture.md#4-layer별-책임)에 따라 bounded TX/RX 저장소 수명, 안정된 data handoff, Driver admission과 local terminal outcome을 맡는다. 차량 기능 Result를 생성하지 않으며 `NET_SUBMIT_OK`, Driver accepted, 실제 TX 완료, peer 수신, 차량 DONE을 구별한다.

| 찾을 근거 | 기존 Source |
|---|---|
| 포인터·복사·Owner 문맥 | [포인터 수명](../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-ownership) |
| Current/Discrete/Query의 보관 | [보관·재전송](../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-lifetime-ownership) |
| 수용 뒤 실패·Health | [원 실패 경계](../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-network-health) |
| 실제 자원·callback 미정 | [타깃 미확정 항목](../../../network_sw_architecture.md#11-현재-확정하지-않는-항목) |

현재는 `SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT`이며 U1에서 새 Queue·Buffer 구조·함수·Module 껍데기를 생성하지 않는다. 상위 [Message Service](../03_MESSAGE_SERVICE/00_LAYER_OVERVIEW.md) 및 [Protocol/Codec](../04_PROTOCOL_CODEC/00_LAYER_OVERVIEW.md), 하위 [Driver](../06_DRIVER_PORT/00_LAYER_OVERVIEW.md)의 책임을 유지한다.

실제 callback lifetime·capacity·recovery는 후속 상세 Gate의 확인 대상이다. 수치 자원이나 성공 보장을 이 지도에서 고정하지 않는다.
