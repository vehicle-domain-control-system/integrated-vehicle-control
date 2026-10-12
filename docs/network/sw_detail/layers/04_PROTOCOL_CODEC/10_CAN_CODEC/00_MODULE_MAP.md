# CAN Codec — 두 RAW 프로파일의 명시 직렬화

[↑ Codec Layer](../00_LAYER_OVERVIEW.md) · [내부 함수 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN TX/RX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md) · [동작](10_BEHAVIOR.md) · [의사코드](20_PSEUDOCODE.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / NOT_COMPILED / BINDING_OPEN.** 기준은 `docs/network@9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4`다. G3 PASS는 기존 U3 Host RAW 진단에 한정한다. 아래 두 함수는 U4C용 설계 후보이며 새 공개 API·설치 헤더·SDK ABI·팀 Wire 채택이 아니다.

<a id="source-evidence"></a>
## 읽은 원본과 재사용 경계

| 기준 SHA의 실제 Source | 파일 SHA256 | 이번 사용 |
|---|---|---|
| [I1 실제 C](../../../../../../tools/network_host/u3_raw_diag/i1/i1_host_codec.c) | `07a9dd07bbac485067f2478870941fe56858b40ddb5a660892108de6d0f4263a` | CAN encoder/decoder 네 개를 직접 호출한다. byte offset/LE/패딩 알고리즘을 복제하지 않는다. |
| [I1 실제 H](../../../../../../tools/network_host/u3_raw_diag/i1/i1_host_codec.h) | `803d44cd09784168aae68a00a99eef59f70a260ac3149f1f94e222ad1969cd20` | 실제 `i1_status`, `i1_cis_rear`, `i1_domain_permission`과 함수 원형을 재사용한다. |
| [U3 진단 C](../../../../../../tools/network_host/u3_raw_diag/u3_raw_diag.c) | `669be76441c489a107c39af7d2779d259fefba697101f01e63aeb0df97d70293` | 선행 profile 검증·실패 시 사본 보존의 근거다. runtime에서 `u3_raw_decode/encode/format`을 호출하지 않는다. |
| [U3 진단 H](../../../../../../tools/network_host/u3_raw_diag/u3_raw_diag.h) | `eb70af02b92c61e4e57e8b7858708e263b1f74c94ff6eea725b81a4d00cead1a` | diagnostic record/호출 수명을 확인했다. 진단 DTO를 제품 TX API로 승격하지 않는다. |
| [CAN Wire 후보](../../../../interface/03_COMMUNICATION/10_CAN_FD.md) | `d081fe7295bf105119f2d785c77cabed340381def2c79aa4845a01f35d79c71a` | 두 ID·flags·19/20 B·DLC·offset의 원 근거다. |
| [Data/ID/AGE](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) | `cd7d0bbf2891d75dbeca1eb154e9589ecee9e768388e14574a428ea68b63a5af` | BOOT/TX/UPDATE/AGE·수신 메타의 소유를 분리한다. |
| [Enum/Wire mapping](../../../../interface/04_COMMON/30_ENUM_WIRE_MAPPING.md) | `a5aaf0677c694df23c82f5e96cbc2fe5119d1dca616d57170ed71d5dda60dfa7` | C2 의미 변환을 raw cast나 정상값 보정으로 대체하지 않는다. |
| [I1 CAN Golden 9개](../../../../../../tools/network_host/u3_raw_diag/i1_can_vectors.json) | `25c071e92bb6350512da5d0aa5c38f9724e04648c52416541b7a3bfdcc8b0fa9` | 후속 U4C의 encode/decode Byte oracle다. expected bytes를 encoder로 생성하지 않는다. |
| [독립 RAW 입력 2개](../../../../../../tools/network_host/u3_raw_diag/independent_vectors.json) | `cfd651ce767fc06527e0d010bf291894ee9bbb9ab14d8c6e198b4c174345d5f8` | 정상 CIS와 열화 Permission 입력·기대 필드의 독립 근거다. |

실제 I1 원형은 다음과 같다. `DECLARED_SOURCE / G3_HOST_TESTED_U3_SCOPE`이며 이 문서의 후보 함수 수에 합산하지 않는다.

```c
i1_status i1_cis_rear_encode(const i1_cis_rear *fields, uint8_t *out,
    size_t capacity, size_t *written);
i1_status i1_cis_rear_decode(uint32_t id, uint8_t dlc, const uint8_t *data,
    size_t length, i1_cis_rear *out);
i1_status i1_domain_permission_encode(const i1_domain_permission *fields,
    uint8_t *out, size_t capacity, size_t *written);
i1_status i1_domain_permission_decode(uint32_t id, uint8_t dlc,
    const uint8_t *data, size_t length, i1_domain_permission *out);
```

I1 원본 archive의 직접 독립 재추출 미수행이라는 G3 OBS-02 증거 한계는 유지한다. G3 판정과 현재 고정 C/H·Golden의 SHA를 재사용하는 것이며 새 Host 실행을 주장하지 않는다. 원 C 파일에 함께 있는 UART·CRC 함수는 CAN 경로에서 호출하지 않고 원본 파일도 변경하지 않는다.

<a id="profile-and-types"></a>
## 프로파일과 자료형

| private kind | ID | IDE/FDF/BRS/RTR | DLC / 사용 / 실제 byte | 실제 I1 field |
|---|---|---|---|---|
| `CAN_REAR` | 표준 `0x0A0` | `0/1/1/0` | `11 / 19 / 20`, B19=0 | `i1_cis_rear` |
| `CAN_PERMISSION` | 표준 `0x080` | `0/1/1/0` | `11 / 19 / 20`, B19=0 | `i1_domain_permission` |

이 두 상수 분기로 충분하므로 profile registry·새 프레임워크·Codec instance·설정 queue를 두지 않는다. RX Driver는 used_length=19를 생성하지 않는다. actual length와 원 flags/bytes만 인계하고 사용 길이 19는 Codec의 기존 profile 근거다. struct padding·`sizeof(i1_*)`는 Wire 길이 결정에 쓰지 않는다.

공유 `CanMsgKind`, `CanFrame`, `CanRawMessage`, `CanCodecResult`, `CanCoreStatus`의 전체 선언·필드 의미는 [Transport Map의 공유 타입 정본](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#common-types)에 있다. 추가 Codec 상태/type는 없다. `CanRawMessage.fields`는 실제 I1 두 struct의 union이다. [NetCisRear](../../../../interface/02_NODES/CIS/10_DATA.md#type-netcisrear)·[NetDomainPermission](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission)와 같다고 cast하지 않는다. Node별 의미 투영과 C2 binding은 기존 상위 경계다.

<a id="function-map"></a>
## 내부 후보 두 함수

| FN-ID / 정확한 원형 | 호출자·동기 결과 | Behavior / Pseudocode |
|---|---|---|
| `FN-U4D-CAN-CODEC-ENCODE` · `CanCodecResult CanCodec_Encode(const CanRawMessage *input, CanFrame *out);` | Service의 구조 검증 또는 Transport TX 준비. `CAN_OK`는 완전한 후보 bytes 생성만 증명한다. | [설명](10_BEHAVIOR.md#fn-u4d-can-codec-encode) · [논리](20_PSEUDOCODE.md#fn-u4d-can-codec-encode) |
| `FN-U4D-CAN-CODEC-DECODE` · `CanCodecResult CanCodec_Decode(const CanFrame *input, CanRawMessage *out);` | owner 문맥의 Service RX 처리. `CAN_OK`는 원 raw 필드 구조 수용만 증명한다. | [설명](10_BEHAVIOR.md#fn-u4d-can-codec-decode) · [논리](20_PSEUDOCODE.md#fn-u4d-can-codec-decode) |

<a id="ownership-budget"></a>
## 저장·수명·commit

| 객체 | owner / 수명 | 상한·commit·실패 |
|---|---|---|
| borrowed `input` | Service pending 사본 또는 Transport RX 사본 / 동기 호출 동안 읽기 전용 | caller가 전체 객체의 생존·안정성·OUT과 비중첩을 보장한다. 주소를 저장하지 않는다. |
| encode scratch | 호출자의 owner stack / 한 함수 호출 | `sizeof(CanFrame)+sizeof(size_t)+sizeof(i1_status)` 및 반환 record. byte 부분은 정확히 20 B다. 실패는 OUT 불변이다. |
| decode scratch | 호출자의 owner stack / 한 함수 호출 | `sizeof(CanRawMessage)+sizeof(i1_status)` 및 반환 record. I1 자체 local scratch가 하위 호출 stack에 더해진다. |
| 성공 OUT | caller의 독립 저장소 / caller가 정한 후속 수명 | 완성 scratch를 한 논리 commit으로 복사한다. struct 대입의 CPU 한 명령 원자성을 주장하지 않고 owner 직렬화로 관측을 보호한다. |
| raw RX와 시간 | `CanRxRecord` owner / [Service 인수](../../03_MESSAGE_SERVICE/10_CAN_SERVICE/00_MODULE_MAP.md)까지 | `frame`, `captured_at`, `backend_generation`을 함께 보관한다. Codec OUT에 시간 근거가 없는 이유로 원 메타를 버리면 안 된다. |

Codec는 malloc/free·영구 slot·상태 FSM·timer·retry·진단 카운터를 소유하지 않는다. 처리량은 고정 두 kind 분기와 최대 20 B I1 처리/사본에 한정한다. 위 `sizeof`는 메모리 예산 산식이며 실제 S32K RAM·stack 상한은 U10 검증 대상이다. 두 owner의 독립 IN/OUT은 재진입 가능하지만 같은 객체의 동시 수정은 금지하며 Core 호출은 동일 worker/superloop에서 직렬화한다. ISR은 복사 후 deferred 인계만 한다.

## 기능 의미와 차단 범위

BOOT 예약 0, 원 code 범위, 패딩 오류는 구조 오류다. 반면 정상 raw INVALID/STALE/NO_DATA·FAULT·NO_OBJECT·65535 AGE는 원 품질과 값 그대로 성공 경로로 전달할 수 있다. 코드 범위 안 필드 조합의 물리 센서 타당성·생성자 coherence·원 출처 신뢰성을 Codec가 새로 판정하지 않는다.

Codec는 BOOT/TX/UPDATE/AGE를 생성·증가·재초기화하지 않는다. TX=0과 UPDATE=0은 기존 Golden에 있는 구조 정상값이며 송신 초기 TX=1과 메시지별 allocation은 Service 책임이다. local RX timestamp와 다른 노드 timestamp를 빼지 않는다. AGE 누적·순서/wrap·source admission·liveness는 [Service](../../03_MESSAGE_SERVICE/10_CAN_SERVICE/00_MODULE_MAP.md), accepted/terminal correlation은 [Transport](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md), 실제 FD capability는 [Port](../../06_DRIVER_PORT/20_CAN_LINKPORT/00_MODULE_MAP.md) 소유다.

`BLOCKED_BY OPEN-WIRE/UNIT/ENUM/PERM/PROVENANCE/C2-SOURCE/PEER-OWNER`는 팀 채택·C2 typed 투영·기능 허용을 제한한다. 두 RAW profile의 encode/decode 문서 설계는 완성한다. `OPEN-PORT-CAPABILITY/SDK/HW/TIME`은 실제 target binding을 제한하며 bounded pure Codec 논리를 보류시키지 않는다. 새 CRC/ACK/enum·mm 환산·C2 헤더·Permission ALLOW를 만들지 않는다.

## 후속 검증 연결

[기존 CIS Flow](../../../flows/10_CIS_REAR_FLOW.md)·[Permission Flow](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)의 기능 의미를 보존하고 [CAN 공통 Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)에 연결한다. U4C 예약은 CAN-D03/04/13/14/15/18/20/21/22이며 각 분기와 기대 OUT는 두 함수 문서에 있다. 이번 U4D는 문서 정적 검증만 수행하며 `HOST_BUILD=NOT_EXECUTED / U4C=NOT_STARTED / G4D_INDEPENDENT=PENDING`이다.
