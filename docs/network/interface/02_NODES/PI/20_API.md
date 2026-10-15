# Pi API — 비전 생산과 차량용 사용 허용

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

권장 프로필과 실제 연동 검증 전 조건을 구분한다. 각 API의 호출·품질·시간 조건을 적용한다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래는 각 노드 안의 로컬 Application/Network 경계다. 제공 DOMAIN C2·ESP32 Gateway 헤더와 달리 해당 노드의 실제 설치/firmware 선언은 확인되지 않았다. 기존 Port가 동일 책임을 충족하면 그 Port에 아래 계약을 매핑하고 공개 wrapper를 중복 설치하지 않는다.

## 송신·수신 함수 빠른 찾기

| 반환형·전체 원형 | 방향 / 메시지 | 호출자 → 구현 담당 후보 |
|---|---|---|
| [`NetSubmitResult PiNet_PublishVisionResult(const NetVisionResult *message);`](#api-pinet_publishvisionresult) | App→Network · `P_VISION_RESULT` | Pi 비전 판정 생산자 → Pi Network/Serial 연결 |
| [`NetSubmitResult PiNet_PublishVisionStatus(const NetVisionStatus *message);`](#api-pinet_publishvisionstatus) | App→Network · `P_VISION_STATUS` | Pi 비전 기능·고장 평가 생산자 → Pi Network/Serial 연결 |
| [`void PiApp_OnCisPermission(const NetPiPermission *message, const NetUartRxMeta *meta);`](#api-piapp_oncispermission) | Network→App · `P_PERMISSION` | Pi UART-C 수신 Dispatcher → Pi 차량용 관측 허용 소비 Application 연결 |

송신은 비차단 로컬 인계이며 [반환/실패·수명 공통 규칙](../../04_COMMON/20_API_USAGE_AND_FAILURE.md)을 적용한다. 수신은 owner 처리 문맥(superloop 또는 task)에서 등록된 소비자에 전달하며 ISR에서 의미 API를 직접 호출하지 않는다. 실제 task/queue/driver/OS/SDK 배치는 확정하지 않는다. **`NET_SUBMIT_OK ≠ CAN/UART 송신 완료 ≠ 상대 ECU ACCEPTED ≠ 차량 DONE`**.

Pi는 언어 중립 record/호출 규격을 사용한다. 각 상세의 C 원형은 정확한 C 바인딩 후보로 병기한 한 가지 표현이며 실제 구현 언어를 강제하지 않는다.

<a id="local-call-contract"></a>
## 로컬 호출·수명·실패 계약

<a id="local-tx-return"></a>
### 송신 반환

접수 전 INVALID_ARGUMENT/NOT_READY/NO_CAPACITY는 그 항목이 접수되지 않았다는 반환이다. 이를 접수 후 최종 전송 실패 사건으로 중복 생성하지 않는다. DOMAIN의 기존 `DomainIf_RxNetworkLocalFailure()`는 DOMAIN 측 계약이며 CIS나 Pi에 같은 공개 함수를 설치하지 않는다. 정상 접수 뒤 실패·만료를 관측하는 통신 처리와 주기 상태·Query 실패 경로는 각각 분리한다.

**반환·실패:** `NET_SUBMIT_OK`=필요한 필드의 로컬 복사/수용 완료, `NET_SUBMIT_INVALID_ARGUMENT`=NULL·예약값/범위·필드 조합 오류, `NET_SUBMIT_NOT_READY`=필요한 링크/현재 원 문맥 미준비, `NET_SUBMIT_NO_CAPACITY`=유한 보관 공간 없음. OK는 전송 완료·상대 수신·차량 DONE이 아니다. 정상 형식 INVALID/STALE 자료는 품질을 보존하여 수용할 수 있다.

<a id="local-rx-return"></a>
### 수신 전달

정상 형식의 INVALID/NO_DATA/STALE 보고와 CRC·길이·예약 코드 오류를 구분한다. 전자는 현재 품질을 전달할 자료이며, 후자를 버렸다고 마지막 정상 관측의 원 AGE가 다시 시작되지는 않는다. 후속 처리에 필요한 message·meta·수신 시각은 콜백 반환 전에 복사한다.

**반환·실패:** `void`는 의미 전달 notification이며 ECU ACCEPTED/DONE을 반환하는 API가 아니다. 필수 소비자가 연결·준비된 owner 처리 문맥에서만 호출한다. CRC/length/예약 코드/수용되지 않은 세대/명령 Age 오류는 Network에서 격리하며 실제 기능 Result를 합성하지 않는다. 정상 형식의 INVALID/FAULT 상태 전달은 허용한다. 소비자 연결 실패의 실제 진단/무효화 경계는 BINDING_OPEN이다.

<a id="pending-current-value"></a>
### 현재값 보관

같은 의미 key의 아직 송신을 시작하지 않은 현재값은 최신 평가로 교체할 수 있다. 이미 송신한 원 관측을 새 관측으로 만들거나 필요한 discrete 결과/발생을 덮어쓰지 않는다. 실제 scheduler/capacity/전송 후 실패 관측은 BINDING_OPEN이다.

<a id="api-pinet_publishvisionresult"></a>
## `PiNet_PublishVisionResult()`

```c
NetSubmitResult PiNet_PublishVisionResult(
    const NetVisionResult *message
);
```

**호출 주체:** Pi 비전 판정 생산자. **구현 담당 후보:** Pi Network/Serial 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

언어 중립 바인딩: `publish_vision_observation(message: NetVisionResult) → NetSubmitResult`. 위 C 서명과 **같은 한 경계**의 표현이며 두 API를 동시에 설치하라는 뜻이 아니다.

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetVisionResult *` · IN/read-only · OUT 없음 | [`NetVisionResult`](10_DATA.md#type-netvisionresult) 전체 의미 record. NULL 불가. Pi 비전 판정 생산자가 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 새 판정과 독립된 presence/count 품질·사유, Pi 동반 기능/고장을 제공한다. 관측 sequence/Age는 비전 owner, pi_boot_id는 runtime-owned여서 Network가 수용 시 원 관측 generation과 결합하고 송신 전 동일 세대를 확인한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** CIS가 받은 뒤에는 원 Pi Boot/sequence/품질이 된다. UART 제출 성공은 DOMAIN 탑승자 경고 완료가 아니다.

**경로·상대 경계:** [`P_VISION_RESULT`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-result) · 반대편 수신 [`CisApp_OnPiVisionResult()`](../CIS/20_API.md). 양쪽 로컬 API 사이에는 UART-C Codec/프레임이 존재한다. 실제 serial/consumer 바인딩은 BINDING_OPEN이다.

<a id="api-pinet_publishvisionstatus"></a>
## `PiNet_PublishVisionStatus()`

```c
NetSubmitResult PiNet_PublishVisionStatus(
    const NetVisionStatus *message
);
```

**호출 주체:** Pi 비전 기능·고장 평가 생산자. **구현 담당 후보:** Pi Network/Serial 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

언어 중립 바인딩: `publish_vision_status(message: NetVisionStatus) → NetSubmitResult`. 위 C 서명과 **같은 한 경계**의 표현이며 두 API를 동시에 설치하라는 뜻이 아니다.

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetVisionStatus *` · IN/read-only · OUT 없음 | [`NetVisionStatus`](10_DATA.md#type-netvisionstatus) 전체 의미 record. NULL 불가. Pi 비전 기능·고장 평가 생산자가 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 관측이 없는 때에도 독립 기능 상태/active·last fault·status sequence/Age·영향 기능을 제공한다. pi_boot_id는 runtime이 수용 시 generation과 결합하고 송신 전 동일 세대를 확인한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** status_sequence 증가와 VISION_SEQUENCE 증가를 구분한다. 기능 복구만으로 이전 관측을 새 VALID로 만들지 않는다.

**경로·상대 경계:** [`P_VISION_STATUS`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-status) · 반대편 수신 [`CisApp_OnPiVisionStatus()`](../CIS/20_API.md). 양쪽 로컬 API 사이에는 UART-C Codec/프레임이 존재한다. 실제 serial/consumer 바인딩은 BINDING_OPEN이다.

<a id="api-piapp_oncispermission"></a>
## `PiApp_OnCisPermission()`

이 경계는 Permission의 수신·해석을 제공한다. 이를 카메라/비전 기능의 실행 조건으로 사용할지는 Pi 기능 모듈이 결정한다. 비전 기능을 적용하지 않더라도 통신 경계와 만료 해석은 검증할 수 있다. 기능 적용 시험을 선택한 경우에만 실제 기능 소비 연결까지 확인한다.

```c
void PiApp_OnCisPermission(
    const NetPiPermission *message,
    const NetUartRxMeta *meta
);
```

**호출 주체:** Pi UART-C 수신 Dispatcher. **구현 담당 후보:** Pi 차량용 관측 허용 소비 Application 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

언어 중립 바인딩: `on_permission_update(message: NetPiPermission, meta: NetUartRxMeta) → Unit(반환값 없음)`. 위 C 서명과 **같은 한 경계**의 표현이며 두 API를 동시에 설치하라는 뜻이 아니다.

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetPiPermission *` · IN/read-only · OUT 없음 | [`NetPiPermission`](10_DATA.md#type-netpipermission) 전체 의미 record. NULL 불가. Network 검증 후 Pi 차량용 관측 허용 소비 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetUartRxMeta *` · IN/read-only | [`NetUartRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netuartrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** CIS relay BOOT/revision, 허용·품질·사유·use_limit·원 DOMAIN 판단 AGE를 받는다. [허용 소비와 원본 Age](10_DATA.md#허용-소비와-원본-age)에 따라 원 AGE에 수신 이후 경과를 누적하고 수용한 세대와 원 수명/USE_LIMIT를 함께 검사한다. 후방 원 500ms를 적용하며 `USE_LIMIT=500ms` 전송값은 조건부 권장 프로필이다. UART-C 반복 500ms/수신 중단 1500ms를 원 수명과 혼동하지 않는다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** 최초 기동/기동 변경·무수신·만료 이후 현 문맥 유효 허용 재확인. 원 허용 상실을 CIS relay가 복구했다고 해석하지 않는다.

**경로·상대 경계:** [`P_PERMISSION`](../../03_COMMUNICATION/30_UART_C.md#message-p-permission) · 원 송신 [`CisNetPriv_RelayPiPermission()`](../CIS/20_API.md). 양쪽 로컬 API 사이에는 UART-C Codec/프레임이 존재한다. 실제 serial/consumer 바인딩은 BINDING_OPEN이다.


## 사용 순서와 구현 한계

1. Pi 비전 producer가 새 판정의 presence/count·각 품질/사유·sequence/원 Age를 제공한다. pi_boot_id는 Network/runtime이 수용 시 원 관측 세대와 결합해 고정한다. 기동/process 관측 세대 변경의 이전 pending/cache는 무효화하며 새 관측 전 old record를 새 Boot로 재표시하지 않는다.
2. 상태 평가/고장은 별도 status record로 제공한다. 판정이 없는 기간에도 상태만 제공할 수 있다.
3. Pi consumer는 CIS 허용 record의 원 source Age와 자기 수신 이후 경과를 확인한다. 무수신/세대 변경/만료 이후 기존 허용을 재사용하지 않는다.

```text
local: NetSubmitResult = publish_vision_observation(observation: NetVisionResult)
status_local: NetSubmitResult = publish_vision_status(status: NetVisionStatus)
on_permission_update(permission: NetPiPermission, meta: NetUartRxMeta) -> Unit
```

예제는 설계 사용 흐름이며 실제 함수 구현/실행 시험이 아니다. Age 기준점·consumer 등록/무효화·통신 health 전달·capacity/재시도 연결은 BINDING_OPEN이다. MOBILE Query는 DOMAIN의 기존 snapshot getter를 사용하며 BCM/CIS/Pi에 새 원격 Query 메시지/API를 설치하지 않는다. 실제 SDK/Firmware/Codec/Board/무선 시험 **NOT_EXECUTED**.

**Generation 수용 조건:** 생산자와 runtime의 실제 기동/관측 세대 연결은 BINDING_OPEN이다. runtime reset 후 이전 record를 다시 제출하여 새 BOOT를 얻는 방식은 금지하며 새 판정/상태 평가가 필요하다. 이는 신규 wire 필드·별도 App DTO를 추가하는 계약이 아니다.
