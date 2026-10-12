# CAN Codec — 함수별 동작 계약

[↑ Module](00_MODULE_MAP.md) · [함수 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [의사코드](20_PSEUDOCODE.md) · [CAN Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / NOT_COMPILED / BINDING_OPEN.** 실제 Source·SHA·실존 I1 원형은 [Module 근거](00_MODULE_MAP.md#source-evidence), 공유 타입의 전체 정의는 [Transport 정본](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#common-types)을 따른다. 아래 반환의 private enum은 Wire에 쓰지 않는다.

<a id="result-mapping"></a>
## 반환과 I1 오류의 명시 mapping

`CanCodecResult.status`는 Core의 구조 검증 결과다. `i1_called=1`일 때만 `i1`을 실제 I1 호출 결과로 읽는다. 선행 reject는 `i1_called=0, i1=I1_OK`이며 이 기본값이 I1 성공을 증명하지 않는다. 동기 `CAN_OK`는 Profile/bytes 또는 RAW DTO 완성을 뜻한다. typed submit, Port accepted, terminal complete, peer RX, 기능 App 성공과 별개다.

| 조건 / 실제 I1 결과 | `status` | `i1 / i1_called` | OUT·상위 관측 |
|---|---|---|---|
| NULL IN/OUT | `CAN_INVALID_ARG` | `I1_OK / 0` | OUT 불변, 새 admission 없음 |
| 미지원 kind 또는 지원하지 않는 표준 ID | `CAN_UNSUPPORTED` | `I1_OK / 0` | U6 schema를 생성하지 않고 OUT 불변 |
| ID>0x7FF·IDE·FD flags·DLC·length 불일치 | `CAN_PROFILE_REJECT` | `I1_OK / 0` | caller의 원 frame/실패 조건을 진단에 보존, liveness·callback 없음 |
| 실제 `I1_ERR_ARGUMENT` | `CAN_CODEC_REJECT` | 원 결과 / 1 | 호출 계약 위반 원인 보존, OUT 불변 |
| 실제 `I1_ERR_CAPACITY` | `CAN_CODEC_REJECT` | 원 결과 / 1 | 고정 scratch20이므로 정상 실행에서 불가한 불변식 위반. queue full로 오인하지 않음 |
| 실제 `I1_ERR_ID/DLC/LENGTH/PADDING` | `CAN_CODEC_REJECT` | 원 결과 / 1 | I1 구조 오류 원인 보존, OUT 불변 |
| 실제 `I1_ERR_VALUE/RESERVED` | `CAN_CODEC_REJECT` | 원 결과 / 1 | 불법 코드 또는 BOOT=0. 정상 열화 품질과 구분 |
| 그 밖의 실제 I1 non-OK | `CAN_CODEC_REJECT` | 원 결과 / 1 | CAN 경로의 예기치 않은 결과도 fail closed; UART NEED_MORE로 진행하지 않음 |
| I1_OK인데 written≠20 또는 encode pad≠0 | `CAN_CODEC_REJECT` | `I1_OK / 1` | 완성 불변식 오류이며 실패를 I1 오류로 지어내지 않음 |
| I1_OK·전체 구조 완성 | `CAN_OK` | `I1_OK / 1` | scratch 전체 commit만; CAN send·Permission 생성 없음 |

Codec가 직접 `NetSubmitResult`를 반환하거나 integer cast하지 않는다. Service의 stable admission 뒤 `NET_SUBMIT_OK`와 원 public 오류 mapping은 [Service](../../03_MESSAGE_SERVICE/10_CAN_SERVICE/10_BEHAVIOR.md)의 책임이다.

<a id="fn-u4d-can-codec-encode"></a>
## FN-U4D-CAN-CODEC-ENCODE — CanCodec_Encode

```c
CanCodecResult CanCodec_Encode(const CanRawMessage *input, CanFrame *out);
```

[동일 함수 의사코드](20_PSEUDOCODE.md#fn-u4d-can-codec-encode) · [TX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md) · [Source](00_MODULE_MAP.md#source-evidence)

**책임·호출자:** owner 문맥에서 Service의 raw 검증 또는 Transport의 한 TX 준비가 호출한다. `input.kind`로 실제 I1 encoder 하나를 선택한다. scheduler·frame admission·정책·시간 생성은 하지 않는다. 재시도마다 새로운 scratch를 사용하며 I1 Wire 구현을 복제하지 않는다.

| 인자·반환 | 정확한 타입 / 방향 | 수명·유효 조건 |
|---|---|---|
| `input` | `const CanRawMessage *` · IN/borrowed/read-only | NULL 불가, 두 kind만 지원. 해당 union field는 원 I1 형식. 호출 중 변경 금지, OUT과 비중첩 |
| `out` | `CanFrame *` · OUT/caller-owned | 완전한 독립 객체, byte 저장소는 타입에 고정 20 B. 실패 시 이전 전체 객체 불변 |
| 반환 | `CanCodecResult` · 값 반환 | [mapping](#result-mapping)의 status/실제 I1 결과/호출 여부. 비동기 사건 없음 |

### 동작·rollback·분기

| 분기 ID | 조건·순서 | 상태·자원·결과 | U4C 예정 |
|---|---|---|---|
| CE01 | IN/OUT NULL 검사 | I1 호출·OUT 변경 없음, `CAN_INVALID_ARG` | CAN-D03 |
| CE02 | kind가 두 후보 밖 | 새 schema/DTO 생성 없음, `CAN_UNSUPPORTED` | CAN-D07 |
| CE03 | zero-initialized local `CanFrame`과 written=0에 해당 I1 encoder 1회 | 독립 scratch20에만 쓰며 원 input·영구 상태 불변 | CAN-D01, CAN-D22 |
| CE04 | 실제 I1 결과≠I1_OK | 원 i1 결과를 보존하고 `CAN_CODEC_REJECT`; scratch 폐기 | CAN-D04, CAN-D18 |
| CE05 | written≠20 또는 scratch B19≠0 | 성공처럼 commit하지 않고 `CAN_CODEC_REJECT`; scratch 폐기 | CAN-D03, CAN-D04 |
| CE06 | 성공 scratch 완성 | ID=0x0A0/0x080, flags=0/1/1/0,DLC11,length20 설정 → 전체 OUT commit → `CAN_OK` | CAN-D08, CAN-D13/14/15/20/21/22 |

동일 kind의 모든 의미 field를 원 I1 encoder에 넘긴다. CIS `validity<=1, quality_reason<=9, proximity_status<=5`; Permission `vehicle_power_permission/power_operation_state∈{0,1,255}`, quality 두 개<=3, operation_allowed<=1; BOOT!=0은 I1 원 검사다. 범위 안 INVALID/STALE/NO_DATA·NO_OBJECT·FAULT·65535 거리 sentinel·AGE는 원 값 그대로 직렬화한다. Field 조합의 센서 물리 타당성 또는 ALLOW/DENY를 여기서 합성하지 않는다.

### 시간·식별·호출 문맥

BOOT/TX/UPDATE/AGE는 입력값 그대로다. 초기/다음 TX allocation과 accepted 시 commit은 Service가 한다. 같은 원 UPDATE 재전송을 새 평가로 바꾸지 않는다. AGE65535는 포화 하한이며 fresh가 아니다. `CanFrame`은 Wire bytes만 가지므로 local clock·RX 시각을 인코딩하지 않는다. 송신 전 누적 AGE를 반영한 stable input을 준비하는 상위 owner와 Port accepted 뒤 immutable bytes를 분리한다. accepted 후 mailbox 대기를 0 또는 5ms 보장으로 보정하지 않는다.

shared slot은 없으며 추가 메모리는 [bounded scratch 산식](00_MODULE_MAP.md#ownership-budget)뿐이다. 재진입은 독립 IN/OUT 객체에 한해서 가능하다. Core owner 직렬화·caller nonalias가 logical commit의 관측 원자성을 보장하며 ISR에서 encode하지 않는다. 별도 late callback/reset은 소유하지 않는다. Port reset 중 호출되어도 pure 함수의 byte 성공이 Port ready를 증명하지 않는다.

**BLOCKED_BY:** `OPEN-WIRE/UNIT/ENUM/PERM/PROVENANCE/C2-SOURCE`는 제품 적용과 의미 투영에 남는다. raw fixture encode와 구조 검증은 설계 완료한다. raw permission 1+OK도 기능 ALLOW 채택이 아니다. 후속 U4C는 [원 I1 Golden 9개 및 독립 입력](00_MODULE_MAP.md#source-evidence)의 frozen byte 값을 사용하며 이번 실행 상태는 NOT_EXECUTED다.

<a id="fn-u4d-can-codec-decode"></a>
## FN-U4D-CAN-CODEC-DECODE — CanCodec_Decode

```c
CanCodecResult CanCodec_Decode(const CanFrame *input, CanRawMessage *out);
```

[동일 함수 의사코드](20_PSEUDOCODE.md#fn-u4d-can-codec-decode) · [RX Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md) · [Source](00_MODULE_MAP.md#source-evidence)

**책임·호출자:** owner 문맥의 Service RX 처리. Transport가 독립 복사한 `CanRxRecord.frame`을 빌려 구조만 검사한다. RX ISR, Driver, Codec에서 Node App·C2·Pi 함수를 직접 호출하지 않는다. Source admission·순서와 원 관측 수용은 Service가 후속 수행한다.

| 인자·반환 | 정확한 타입 / 방향 | 수명·유효 조건 |
|---|---|---|
| `input` | `const CanFrame *` · IN/borrowed/read-only | NULL 불가, fixed bytes20 객체. flags와 실제 length는 원 RX 값이며 호출 중 안정, OUT과 비중첩 |
| `out` | `CanRawMessage *` · OUT/caller-owned | 독립 전체 객체. 실패 시 byte-for-byte 이전 값 보존; 성공 후 caller가 수명 소유 |
| 반환 | `CanCodecResult` · 값 반환 | [mapping](#result-mapping). 정상 raw 구조는 품질 저하 여부와 별개로 `CAN_OK`; 비동기 사건 없음 |

### 동작·rollback·분기

| 분기 ID | 조건·순서 | 상태·자원·결과 | U4C 예정 |
|---|---|---|---|
| CD01 | IN/OUT NULL 검사 | I1 호출·OUT 변경 없음, `CAN_INVALID_ARG` | CAN-D03 |
| CD02 | can_id>0x7FF 또는 IDE!=0 | 표준11 profile reject, `CAN_PROFILE_REJECT` | CAN-D03 |
| CD03 | 표준 ID지만 0x0A0/0x080 밖 | 미지원 message, `CAN_UNSUPPORTED` | CAN-D03, CAN-D07 |
| CD04 | FDF!=1, BRS!=1, RTR!=0 | FD profile reject, `CAN_PROFILE_REJECT` | CAN-D03 |
| CD05 | DLC!=11 또는 actual length!=20 | payload byte·B19를 읽기 전에 `CAN_PROFILE_REJECT` | CAN-D03 |
| CD06 | metadata 구조 수용 | ID로 kind 선택 → zero scratch의 해당 union에 실제 I1 decoder 1회 | CAN-D04, CAN-D22 |
| CD07 | 실제 I1 결과≠I1_OK | 원 오류 보존, `CAN_CODEC_REJECT`; 정상 callback·liveness commit 없음 | CAN-D04, CAN-D18 |
| CD08 | I1_OK의 완전한 scratch | 원 BOOT/TX/UPDATE/AGE/value/quality 전체 OUT commit → `CAN_OK` | CAN-D12/13/14/15/18/20/21/22 |

length=19/21, DLC 11≠bytes20, IDE/FDF/BRS/RTR=2도 정규화하지 않고 reject한다. 길이 확인 후 I1이 B19=0·Boot 예약0·raw code 범위를 검사한다. 원 code 범위 안 INVALID/STALE/NO_DATA를 구조 오류로 바꾸지 않는다. input과 기존 OUT은 실패 시 모두 불변이다. 실패 객체의 raw bytes와 분기는 caller의 별도 진단 대상으로 남지만 가짜 DTO를 생성하지 않는다.

### 시간·원본·후속 수용

원 TX/UPDATE의 0과 uint32 최대값도 Codec에는 구조 정상값이다. 같은 TX, 다른 BOOT, wrap·역순의 의미 admission은 [Service](../../03_MESSAGE_SERVICE/10_CAN_SERVICE/00_MODULE_MAP.md) 소유다. wire AGE가65534/65535이면 그대로 반환하며 시각 차감·AGE0·원 사용 기한 연장을 하지 않는다. raw distance400을400 그대로 남기고 mm/cm 환산을 추가하지 않는다. Permission 원 raw 0/1/255와 두 품질은 원 byte대로 보존하며 허용·불허 판단을 생성하지 않는다.

caller는 `CanRxRecord.captured_at`, `backend_generation`, 원 frame을 보존하고 성공 DTO와 함께 `CanSvc_OnDecoded()`에 전달한다. Codec가 시간을 인자로 받지 않는 것은 원 RX 시각을 생략하라는 뜻이 아니다. 로컬 RX 대기·same-update AGE watermark·Clock uncertainty는 Service에서만 반영한다. controller error/bus-off 관측은 Port/Transport의 별도 health 사건이며 Codec 성공으로 지워지지 않는다.

scratch와 OUT의 owner 직렬화·nonalias·수명은 encode와 같다. 별도 queue/full/timer/late callback 상태가 없으므로 이 함수가 RX ring overflow를 복구하거나 이전 in-flight TX를 변경하지 않는다. RX full 때 이 함수 자체를 호출하지 않고 Transport가 실제 drop을 기록한다. ISR parse 또는 상위 observer 재진입은 owner 계약 위반이며 후속 Core 문맥에서 처리한다.

**BLOCKED_BY:** `OPEN-UNIT/ENUM/C2-SOURCE/PROVENANCE/PERM/PEER-OWNER`는 C2 투영·원 Producer·coherence·150ms 원 기한과 기능 소비에 남는다. RAW 검증·품질 보존은 완료된 설계다. 정상 열화 수신, 구조 reject, Node 사용 제한은 서로 다른 경로이며 `OPEN-WIRE/PORT-CAPABILITY/SDK/HW/TIME`의 실제 채택/target 시험은 이번 결과에 포함되지 않는다. U4C 예약 Test-ID는 위 분기표이며 이번 U4D Host 실행은 NOT_EXECUTED다.
