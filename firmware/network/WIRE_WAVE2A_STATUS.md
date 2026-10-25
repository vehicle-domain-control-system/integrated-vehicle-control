# CAN Wave2A Host 후보 (과거 기준)

이 파일은 Wave2A의 source67 / ON101 / OFF41 기준을 기록한다. 지금의 UART21 추가와 CAN22/UART-M23/UART-C3 raw 기능은 `WIRE_WAVE2B_STATUS.md`에 있다.

범위: CAN22 raw 형식 후보, UART5 형식 후보, registry에만 있는 UART21. 모두 채택되지 않은 후보다. 분리된 `network_wire_candidate` 대상은 기본 OFF 그대로이고, `NET_WIRE_CANDIDATE_ONLY`를 명시해 불러야 한다. Runtime, C2, M03 handoff, 장치 Port, HAL, scheduler, 차량 consumer는 연결하지 않았다. 기존 Layer 5개/Module 9개는 바뀌지 않았다.

| L04/M05 소스 | raw 후보 메시지 |
|---|---|
| 보존한 `net_wire_can.c` | CIS_REAR, DOMAIN_PERMISSION (Wave1) |
| `net_wire_can_bcm_raw.c` | BCM_DOOR_COMMAND, BCM_CLIMATE_COMMAND, BCM_LIGHT_COMMAND, BCM_DOOR_STATE, BCM_CLIMATE_STATE, BCM_LIGHT_STATE, BCM_RESULT, BCM_EVENT, BCM_STATUS |
| `net_wire_can_nodes_raw.c` | WINDOW_COMMAND, WINDOW_STATE, WINDOW_EVENT, WINDOW_RESULT, WINDOW_FAULT, CIS_ENVIRONMENT, CIS_OCCUPANT, CIS_STATUS, VSS_EVENT, VSS_WARNING_STATE, VSS_STATUS |
| `net_wire_can_common.c` | 하나뿐인 Registry48을 쓰는 인자/별칭 검사와 정확한 envelope/padding |

공개 선언은 `net_wire_can.h`로 쓸 수 있다. BCM/노드 헤더는 읽기 쉽게 raw 타입을 나눈 것이다. type마다 명시적인 encode/decode 쌍이 있다. 구조체 padding은 wire 데이터가 아니다. 응용 CRC, heap, 벤더 헤더는 들여오지 않는다. 공용 LE helper에는 이식형 signed-i16 변환이 있다.

`NetWireRegistryEntry.typed_wave1`은 일부러 과거의 일곱 항목을 그대로 둔다. 지금의 기능(CAN22와 UART5)은 `net_wire_registry_has_typed_codec(link,id)`로 확인한다. 이 기능 정보는 메타데이터이고 실행을 허락하지 않는다. 기존 Wave1 fixture 소스와 원래 CTest 69개(all69)는 그대로다.

payload를 해독하기 전에 정확한 표준 ID, FDF/BRS/RTR/extended/driver_error 플래그, DLC, capture 길이, 모든 padding 0 바이트를 검사한다. BOOT_ID0은 예약값이다. 원 카운터, 명령 식별, 관련 식별, source AGE, 한도 필드는 정의된 곳의 0/FFFF까지 그대로 둔다. 신선도, 순서, epoch 전환, 대조, 식별 할당, 허가를 추정하지 않는다. 실패하면 출력 객체 전체를 그대로 둔다. NULL과 입력·출력의 완전/부분 겹침은 거부한다. 호출자 버퍼는 살아 있고 안정되어야 하며, 호출은 ISR 밖에서 직렬로 한다. ISR/스레드 안전 binding은 주장하지 않는다.

CIS_ENVIRONMENT에는 온도/습도/조도의 서로 독립된 lane 3개가 있고, 각자 validity/reason/sequence/AGE를 가진다. signed 온도는 raw centi-C이고, 무효 sentinel은 측정값과 구별된다. kit 예시에 신호 네 개라는 표현이 있어도 네 번째 lane을 더하지 않는다. TTL2500/1000 제안은 채택되지 않은 채다. BCM RGB 순서는 R/G/B이고, 측정 UNKNOWN과 명령 범위는 서로 다르다. 결과/이벤트는 실행 성공, 확인, 고장 전환을 지어내지 않고 증거를 그대로 둔다. 지원하지 않는 의미 조합 가운데 안전한 후보 해석이 문서에 없는 것은 HOLD를 돌려주고, 예약 스칼라 값은 거부한다.

WARNING60은 used60/frame64/DLC15이고 padding은 B60..63이다. Format0은 정해진 부재 표현(B50=0, B52..59=0)이 필요하고, format1은 넘겨받은 metadata를 그대로 둔다. 잘못된 format, 틀린 길이, 0이 아닌 padding은 거부한다. 예전 51바이트 capture는 거부한다. 바이트가 정해진 format0과 정확히 같은, 예전 producer의 64바이트 전체 프레임은 바이트만으로 구별할 수 없다. 후보 프로필 선택은 명시적이고, 실제 producer/프로필 채택은 HOLD 그대로다. 새 구별자를 지어내지 않는다. VSS_STATUS의 현재 FaultMask는 u8이고 마지막 고장 이력과 따로다. A2 same-K State/Fault commit은 여기서 구현하지 않는다.

WINDOW의 raw DTO 5개는 Host 전용 설계 후보다. 명령 TX, 감시, 응용 실행, 표시를 켜지 않는다. VENT70/80 정책은 고르지 않았다. Round4 지적 ID 11개/위치 32개, G01..G08, C2 ABI, Permission 전파, Query/finalTX, VSS 투영, 실제 MCU binding은 OPEN/HOLD 그대로다. S32K144/S32K344는 NOT_CONFIGURED 그대로이고, 하드웨어/flash/물리 시험은 NOT_RUN이다.

## Golden 검증 도구 P2 수정

`tools/fixtures/wave1_golden.json`은 받은 Wave1 fixture를 바이트 그대로 복사한 것이다(SHA256 `4c979747887b76a0aed9a295309cb050650ea83da145b9d321569d8f475bd07e`). `tools/verify_wave1_golden.py`는 기본으로 읽기 전용 검사를 하고, `--check`도 같다. fixture의 정확한 바이트, 해석한 모든 필드, 독립 Python struct/CRC 기준, C literal 9개, 원래 시험 소스 SHA를 확인한다. fixture, 소스, 보고서, cache를 쓰지 않는다. 생성/출력 스위치는 지원하지 않고 거부한다. 원래 배포한 Wave1 ZIP, JSON, 검증 도구는 건드리지 않았다.

```sh
python firmware/network/tools/verify_wave1_golden.py --check
python firmware/network/tests/host/test_wave1_verifier.py
cmake -S firmware/network -B firmware/network/.build-wave2a -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_TARGET=host
cmake --build firmware/network/.build-wave2a --parallel
ctest --test-dir firmware/network/.build-wave2a --output-on-failure
```

후보 Host 시험에는 CMake와 C11 컴파일러 말고도 Python3.8+가 필요하다. 라이브러리는 Python이 필요 없다. 검증 도구 회귀 시험은 보호된 network 빌드 폴더 아래에 분리된 임시 사본을 만들고, 그 사본만 망가뜨리며, 도구 호출이 성공할 때와 실패할 때 모두 앞뒤 SHA를 확인한다. CRLF 원본 fixture는 어느 host에서도 받아들인다. LF로 바꾼 대체본은 바이트 손상으로 잡고 자동으로 고치지 않는다. Python `-O`로 검증을 건너뛸 수 없다. 현재의 정확한 명령, 독립 CAN20 기준, type별 결과, 한계, 소스 불변 증거는 `_local_review/wire_wave2a_20261017_v1.0/`에 있다.
