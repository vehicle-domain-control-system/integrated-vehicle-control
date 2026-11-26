# 제품 어댑터 자리 (비어 있음)

이 폴더에는 코드가 없다. 제품 API 어댑터는 M03(`src/l02_typed_handoff/m03_typed_handoff/`, 한 노드만 쓰는 것은 `nodes/<node>/` 아래 같은 폴더)에 있다.

- DOMAIN: `NetDomainC2`(`net_domain_*.c`, 공개 헤더 `nodes/domain/include/network/net_domain_c2.h`). DOMAIN C2 공개 헤더(`Domain_Interface_v0.5_C2_target.h`, `Vehicle_Types_v0.2.h`)는 CMake include 경로로 읽기만 한다. 이 헤더로 컴파일된다는 것이 ABI 채택 승인은 아니다. Network는 `DomainIf_*` 함수를 따로 선언하거나 정의하지 않는다(시험용 fake는 `tests/` 아래에만 있다).
- VSS·CIS·BCM: App Port facade(`net_app_port_*.c`). 노드 앱 쪽 선언은 `nodes/<node>/include/network/app/net_if_<node>.h`에 있다. 송신 함수(`VssNet_*`, `CisNet_*`, `BcmNet_*`)는 Network가 facade에서 구현하고, 수신 콜백(`VssApp_*`, `CisApp_*`, `BcmApp_*`)은 각 노드 앱이 구현한다. VSS 앱은 같은 선언의 자기 사본 `vss_net_api.h`를 쓰고 Network와 링크에서 만난다.
- 의미 규칙이 정해지지 않은 경로는 그 경로만 HOLD한다. 규칙을 추측해 만들지 않는다.

일반 WINDOW 실행·감시와 ESP32 구현은 이 범위 밖이다.
