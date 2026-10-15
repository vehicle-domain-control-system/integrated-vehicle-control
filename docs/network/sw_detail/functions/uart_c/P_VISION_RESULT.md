# P_VISION_RESULT — Pi 원 판정과 두 독립 품질

`WIRE-P03-P_VISION_RESULT` / `RX-P03-P_VISION_RESULT-CIS`. P03, **PI → CIS**, VERSION **1**, TYPE **0x60**, Payload **19B**, Frame **29B**, 필드 **11개**다. 상세 상태는 `DESIGNED_CANDIDATE`이며 양단 형식 채택·기동 수용·Driver 바인딩은 `BLOCKED_BY_CONTRACT`다. 문서 Codec 성공과 제품 통신 성공을 구분한다.

[Network UART-C 개요](../../06_UART_C_OVERVIEW.md) → [L04](../../layers/L04_FRAME_CODEC.md) / [M06](../../modules/M06_UART_CODEC.md) → 본 함수 → [Data 정본](../../data/UART_C_DATA.md) → [Pi→CIS 흐름](../../flows/UART_C_PI_TO_CIS.md). [Frame·Router](../UART_C_FRAME_ROUTER.md)와 [송수신·Driver 수명](../UART_C_TX_RX_CONTROL.md)은 이 메시지 앞뒤의 공통 조건이다.

<a id="data-api"></a>
## Data·기존 API·소유자

전체 DTO 선언의 단일 정본은 [NetVisionResult](../../data/UART_C_DATA.md#type-NetVisionResult), 수신 메타는 [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta)다. 원 선언은 S4A `data/T25.md#type-NetVisionResult` / T25 L26–39이며 11개 멤버를 그대로 사용한다. C 구조체 padding이나 `sizeof`를 Wire 길이로 쓰지 않는다. Pi 언어 중립 record의 C 참조 표현이며 Pi에 C ABI를 강제하지 않는다.

<a id="api-PiNet_PublishVisionResult"></a>
<a id="api-CisApp_OnPiVisionResult"></a>
```c
NetSubmitResult PiNet_PublishVisionResult(
    const NetVisionResult *message
);
void CisApp_OnPiVisionResult(
    const NetVisionResult *message,
    const NetUartRxMeta *meta
);
```

Pi 비전 생산자가 첫 함수를 호출하고 Pi Network/Serial 연결이 로컬 접수한다. CIS Network 수신 Dispatcher가 두 번째 함수를 자기 노드의 CIS App에 호출한다. 노드 간에는 UART 프레임이 존재하며 Pi가 CIS C 함수를 직접 호출하지 않는다. 원형 근거: T26 L41–43, T10 L181–184 및 기존 [Pi API](../../../interface/02_NODES/PI/20_API.md#api-pinet_publishvisionresult), [CIS API](../../../interface/02_NODES/CIS/20_API.md#api-cisapp_onpivisionresult).

송신 `message`는 NULL 불가·호출 동안 borrowed IN이다. API가 `NET_SUBMIT_OK`를 반환하기 전에 전체 값·원 기동 연결·원 AGE 기준을 Network 소유 사본으로 확보한다. `pi_boot_id`는 runtime/Network가 **해당 원 관측과 같은 기동**을 local admission에서 고정한다. App에 Boot 초기화 의무가 없으므로 입력의 아직 결합되지 않은 Boot 슬롯을 바로 검사하거나 읽어 INVALID로 만들지 않는다. 아래 Wire Validator는 이 결합이 끝난 안정 DTO용이다. 결합 증거가 없으면 접수 준비 미완료이고, 과거 record를 현재 Boot로 stamp하여 통과시키지 않는다.

CIS callback의 두 포인터는 같은 수신 사건의 읽기 전용 사본이며 반환까지 유효하다. 이후 처리할 CIS 소비자는 11개 필드와 동일 `meta`를 반환 전에 함께 복사한다. `void`는 호출 사실만 뜻하며 CIS 기능 소비·CAN 송신·DOMAIN 위험 판단 완료를 반환하지 않는다. ISR에서 payload parsing이나 App callback을 수행하지 않는다. 실제 Task/Superloop·동기화·소비자 등록은 D-05/G06 HOLD다.

<a id="wire"></a>
## Payload 11필드

Payload B0=Frame B8, 추가 padding 없음. CRC16/CCITT-FALSE 입력은 **Frame B2..B26(25B)**, CRC 저장은 **B27..B28 LE**다. A5 5A·VERSION1·Type0x60·P19·방향을 모두 확인한 뒤 payload를 읽는다. 전체 공통 규칙은 [Frame·Router](../UART_C_FRAME_ROUTER.md)다.

| Payload offset / 폭 | Frame offset | 원 필드 → typed 멤버 | 부호·순서·코드/단위 | 조건 / T35 행 |
|---|---|---|---|---|
| B0 / 4 | B8 | `PI_BOOT_ID` → `pi_boot_id` | u32 unsigned LE, 식별자 | 0 예약; 실제 발급/수용 HOLD / L45 |
| B4 / 4 | B12 | `VISION_SEQUENCE` → `vision_sequence` | u32 unsigned LE, 원 판정 번호 | 초기값·wrap 미정; 반복 유지 / L46 |
| B8 / 2 | B16 | `VISION_AGE` → `vision_age_ms` | u16 unsigned LE, ms | 65535 포화 하한 / L47 |
| B10 / 1 | B18 | `OCCUPANT_PRESENCE` → `occupant_presence` | u8 unsigned, 순서 N/A; 0 없음 / 1 있음 / 255 미확인 | 255를 없음으로 바꾸지 않음 / L48 |
| B11 / 1 | B19 | `PRESENCE_VALIDITY` → `presence_validity` | u8 unsigned, N/A; 0 INVALID / 1 VALID | Count 품질과 독립 / L49 |
| B12 / 1 | B20 | `PRESENCE_REASON` → `presence_reason` | u8 unsigned, N/A; NetQualityReason 0..9 | Pi 원 사유 / L50 |
| B13 / 1 | B21 | `OCCUPANT_COUNT` → `occupant_count` | u8 unsigned, N/A; 0..5명 / 255 미확인 | 255를 0명으로 바꾸지 않음 / L51 |
| B14 / 1 | B22 | `COUNT_VALIDITY` → `count_validity` | u8 unsigned, N/A; 0 INVALID / 1 VALID | Presence 품질과 독립 / L52 |
| B15 / 1 | B23 | `COUNT_REASON` → `count_reason` | u8 unsigned, N/A; NetQualityReason 0..9 | Pi 원 사유 / L53 |
| B16 / 1 | B24 | `VISION_STATUS` → `vision_status` | u8 unsigned, N/A; 0 NOT_READY / 1 READY / 2 ACTIVE / 3 FAULT / 4 RECOVERING | 원 판정에 동반한 Pi 상태 / L54 |
| B17 / 2 | B25 | `VISION_FAULT_CODE` → `vision_fault_code` | u16 unsigned LE; 0 없음 / 1 초기화 / 2 카메라 / 3 판정 / 4 데이터 오류 | 품질 사유와 다른 코드 집합 / L55 |

NetQualityReason은 0 NONE, 1 NOT_READY, 2 OUT_OF_RANGE, 3 SENSOR_FAULT, 4 VISION_FAULT, 5 STALE, 6 NO_DATA, 7 COMMUNICATION_FAULT, 8 RECOVERING, 9 DATA_INVALID다(T37 L264–299). `NetTriState/NetValidity/NetQualityReason/NetSensorFunctionState`는 고정 폭 raw 의미 별칭이다. C2 enum을 숫자 cast하여 이 코드라고 주장하지 않는다. T35 L30의 100ms 보고·300ms 수신 중단은 후보이며 원 판정의 최종 사용 한도·실측 지연·재시도 기한이 아니다.

<a id="codec"></a>
## M06 F17/F18 — 원 필드별 Validator·Encode·Decode

다음 세 함수는 **PROPOSED_PRIVATE_FUNCTION**이며 제품 함수/헤더가 아니다. `message/payload`는 호출 내내 안정된 IN, OUT은 충분한 독립 저장소이며 입력과 겹치지 않는다. `bool true`는 해당 payload 형식의 완전한 산출만 뜻한다. 프로파일 채택·현재 Boot/순서·시간·기능 소비 허용은 [Router](../UART_C_FRAME_ROUTER.md)에서 따로 검사한다. 실패 시 OUT 전체 불변이다. Encode는 성공 시 정확히 19B만 쓰고 남는 용량을 건드리지 않는다. LE 도우미의 폭·산술은 [공통 정의](../UART_C_FRAME_ROUTER.md)를 따른다.

```c
bool NetUartC_ValidateVisionResult(const NetVisionResult *message)
{
    if (message == NULL) return false;
    if (message->pi_boot_id == 0) return false; // 원 관측/runtime 결합 완료 DTO
    if (message->occupant_presence != 0 && message->occupant_presence != 1 &&
        message->occupant_presence != 255) return false;
    if (message->presence_validity > 1) return false;
    if (message->presence_reason > 9) return false;
    if (message->occupant_count > 5 && message->occupant_count != 255) return false;
    if (message->count_validity > 1) return false;
    if (message->count_reason > 9) return false;
    if (message->vision_status > 4) return false;
    if (message->vision_fault_code > 4) return false;
    // sequence와 AGE는 u32/u16 표현 전체를 허용한다. fresh/순서 승인 아님.
    // 원문에 없는 validity/reason/count 상호 조합 규칙을 만들지 않는다.
    return true;
}

bool NetUartC_EncodeVisionResultPayload(const NetVisionResult *message,
    uint8_t *out, uint16_t outCapacity)
{
    if (out == NULL || outCapacity < 19) return false;
    if (!NetUartC_ValidateVisionResult(message)) return false;
    uint8_t p[19]; // 로컬 형식 산술용. 실제 Queue 용량 제안 아님.
    NetUartC_WriteLe32(&p[0], message->pi_boot_id);
    NetUartC_WriteLe32(&p[4], message->vision_sequence);
    NetUartC_WriteLe16(&p[8], message->vision_age_ms);
    p[10] = message->occupant_presence;
    p[11] = message->presence_validity;
    p[12] = message->presence_reason;
    p[13] = message->occupant_count;
    p[14] = message->count_validity;
    p[15] = message->count_reason;
    p[16] = message->vision_status;
    NetUartC_WriteLe16(&p[17], message->vision_fault_code);
    for (uint16_t i = 0; i < 19; ++i) out[i] = p[i];
    return true;
}

bool NetUartC_DecodeVisionResultPayload(const uint8_t *payload,
    uint16_t length, NetVisionResult *out)
{
    if (payload == NULL || out == NULL || length != 19) return false;
    NetVisionResult tmp; // 모든 멤버를 아래에서 채우기 전 공개하지 않는다.
    tmp.pi_boot_id = NetUartC_ReadLe32(&payload[0]);
    tmp.vision_sequence = NetUartC_ReadLe32(&payload[4]);
    tmp.vision_age_ms = NetUartC_ReadLe16(&payload[8]);
    tmp.occupant_presence = payload[10];
    tmp.presence_validity = payload[11];
    tmp.presence_reason = payload[12];
    tmp.occupant_count = payload[13];
    tmp.count_validity = payload[14];
    tmp.count_reason = payload[15];
    tmp.vision_status = payload[16];
    tmp.vision_fault_code = NetUartC_ReadLe16(&payload[17]);
    if (!NetUartC_ValidateVisionResult(&tmp)) return false;
    *out = tmp; // 전체 typed 검증 후 한 번만 commit
    return true;
}
```

Net 고정 폭 별칭의 raw 코드 범위를 확인한 뒤 명시 offset으로 기록하므로 enum 메모리 직렬화가 아니다. 정상 INVALID/UNKNOWN/FAULT도 해당 코드 범위이면 Decoder가 보존한다. 예를 들어 `count=255/INVALID/NO_DATA`는 오류 frame과 다르며 값 0으로 치환하지 않는다. 허용 범위지만 원문에 조합 의미가 없는 경우는 동일 raw를 보존하고 기능 Owner 확인 전 그 조합의 유효 소비를 보류한다.

<a id="lifecycle"></a>
## 송신·수신·원 AGE의 경계

Pi local admission은 NULL·원 필드 표현→runtime과 원 관측의 세대 결합→profile/경로/시간 근거→안정 사본/용량을 차례로 확인한다. 표현 오류는 `NET_SUBMIT_INVALID_ARGUMENT`, 미준비는 `NET_SUBMIT_NOT_READY`, 전체 사본 공간 부족은 `NET_SUBMIT_NO_CAPACITY`, 완전 접수 뒤만 `NET_SUBMIT_OK`인 기존 후보를 따른다. runtime 결합 실패와 잘못된 Wire Boot0를 동일 원인으로 기록하지 않는다.

dispatch는 사본의 원 관측 generation이 여전히 수용되는지 확인하고, 원 `vision_age_ms`에 아직 반영하지 않은 Pi 대기 구간만 한 번 더해 65535로 포화한다. 시간원 reset/wrap·대기 기준이 미확인이면 AGE0을 채우지 않고 해당 전송/최신성 사용을 제한한다. 이때 VisionSequence는 변하지 않고 별도 LINK_SEQUENCE를 제공하여 payload→Frame Builder→Driver로 보낸다. Driver accepted·최종 TX·참조 해제는 각각 관측하며 참조 중 프레임의 AGE/CRC를 수정하지 않는다. 실제 release가 먼저 확인되면 버퍼 반환과 결과 identity 보존을 분리한다. accepted 후 실패가 나도 이전 OK를 소급 FAILED로 바꾸거나 Pi/CIS에 DOMAIN C2 failure API를 신설하지 않는다.

CIS는 Stream이 확보한 정확 29B·CRC 경계를 소비한 뒤 이 Decoder를 사용한다. Decoder false 또는 후속 Boot/순서·소비자 거부이면 정상 App 전달·정상 source 관측을 갱신하지 않는다. **CRC 정상 typed 거부는 29B 전체를 소비**하며 payload 속 A5 5A를 재동기화 시작점으로 쓰지 않는다. CRC/헤더 오류는 공통 Stream의 후보 시작+1 suffix 재탐색을 따른다.

같은 Link/raw의 정확한 중복은 정상 감시·callback을 갱신하지 않는다. 수용된 새 Link·같은 PiBoot/VisionSequence/의미·검증된 AGE의 정상 반복은 기존 `CisApp_OnPiVisionResult`와 Result 수신 감시에 연결한다. 원 raw/meta와 보수적 현재 AGE는 구분해 안정 보관하며 callback void를 저장 ACK로 해석하지 않는다. 반복 수신의 새 LINK_SEQUENCE·새 `received_at_ms`는 새 VisionSequence·AGE0의 근거가 아니다. 동일 Pi Boot/VisionSequence라도 원 값·독립 품질이 달라지면 같은 판정으로 합치는 근거를 확인한다. 최초 순번·wrap·역전 허용 창은 미정이므로 수치 대소만으로 새 판정을 승인하지 않는다. 수용한 기동과 소비 계약이 확보된 정상 열화 보고는 원 품질로 전달하되 사용 가능 판정은 별도다.

<a id="projection"></a>
## CIS_OCCUPANT 연결과 품질 정보손실

[기존 CAN CIS_OCCUPANT](../can/CIS_OCCUPANT.md)와 [NetCisOccupant](../../data/CAN_NODE_DATA.md#type-NetCisOccupant)는 **Presence와 Count 각각** validity/reason을 가진다. 메시지 전체가 단일 품질 한 벌인 것이 아니다. 제한은 **각 값의 한 벌로 Pi 원 품질과 CIS 추가 판단 두 층의 사유를 동시에 싣지 못한다**는 점이다. T09 L88–101의 두 독립 쌍을 그대로 보존한다.

CIS App은 동일 Pi Boot/VisionSequence/원 AGE/두 품질을 보관한 후 별도의 실제 CIS 사용 제한을 평가한다. 선택 Interface의 Network 선정 규칙에 따라 원 record의 INVALID와 원 사유를 보존하고 이후 실제 통신 단절·만료·기동 변화·해당 기능 Fault가 확인되면 현재 reason을 제한 사유로 바꿀 수 있다. [CheckVisionProjection](../../flows/UART_C_PI_TO_CIS.md#projection)은 원 사유 유지/검증된 현재 제한을 구분하고 근거 없는 변경·INVALID→VALID를 거부한다. 대표 사유 우선순위·`vision_function_status` 변환·같은 사건 연결 근거가 없는 개별 투영은 HOLD이며 실제 callback→CAN→C2 연결은 BINDING_NOT_VERIFIED다. Pi `VISION_STATUS`를 CIS `vision_function_status`로 raw cast하거나 최신 독립 STATUS를 과거 Result에 덮지 않는다. Pi fault code는 CAN OCCUPANT에 같은 필드로 실리지 않으므로 없던 byte를 추가하지 않는다.

CIS CAN source Boot/TX_SEQUENCE는 Pi Boot/VisionSequence와 다른 주체다. DOMAIN C2에서 실제 소비된 같은 원 입력과 연결되는지는 기존 B12/B13 IF-O09/IF-O24·D-02 제한을 그대로 따른다. UART 정상 수신만으로 위험 경고·VSS 동작·차량 DONE을 만들지 않는다.

<a id="counterexamples"></a>
## 정상·거부·열화·복구 반례

| ID | 입력·사건 | 검사 위치 / 기대 결과·불변 조건 |
|---|---|---|
| VC-R01 | 조건부 정상: PiBoot7/Vision9/AGE20, Presence1/VALID/NONE, Count2/VALID/NONE, ACTIVE/고장0 | profile·원본·소비 바인딩 확인 시 19B→29B→같은 DTO/meta 전달. 반환/CRC만으로 CAN 성공 주장 없음 |
| VC-R02 | Count255/INVALID/NO_DATA, Presence1/VALID/NONE | Validator 성공, 두 품질 그대로. Count0 합성·Presence 자동 무효화 없음 |
| VC-R03 | Presence255/INVALID/VISION_FAULT, Count2/VALID/NONE | Validator 성공, Count 독립 보존. Pi 원 사유를 CIS NONE으로 덮지 않음 |
| VC-R04 | Presence2 / validity2 / reason10 / count6 / status5 / fault5 / 결합 후 Boot0 | Validator false, Encode/Decode OUT 불변. CRC 정상일 때 전체 frame 소비·callback 없음 |
| VC-R05 | P18/P20·VERSION2·CIS→PI 방향 Type0x60·CRC 오류 | Frame/registry 거부, 부분 typed 게시 없음; 신뢰 못하는 경계는 suffix 재탐색 |
| VC-R06 | 수용된 새 LINK_SEQUENCE·동일 PiBoot/Vision9/의미/보수 AGE | 기존 callback·해당 메시지 감시 연결, 새 원 평가/AGE0/기한 연장 없음. 미확인 AGE 감소는 TIME_HOLD |
| VC-R07 | 원 Result INVALID 뒤 독립 Status READY 수신 | Result 원 identity·두 품질·AGE 불변. 새 카메라 평가 전 VALID 복구 없음 |
| VC-R08 | Pi runtime 재시작 후 이전 pending record 재제출 / 구 Boot 지연 frame | 원 기동 결합·수용 검사에서 제한. old record에 새 Boot stamp 금지; 이전 half-frame도 새 링크와 결합 안 함 |
| VC-R09 | AGE65535 또는 clock reset/미측정 전달 구간 | 포화 하한 보존, fresh 증거로 사용 안 함; 외부 시각 직접 뺄셈·미측정 지연0 금지 |
| VC-R10 | Pi INVALID 사유와 CIS 통신 제한 동시 존재 | 각 값의 원 record 보존, 승인되지 않은 대표 사유 합성/CAN 투영 HOLD; 다른 값의 품질 임의 변경 없음 |
| VC-R11 | caller 반환 후 입력 변경 / 공간 없음 / accepted 후 실패·late callback | 완전 사본 전 OK 없음; 참조 중 frame 불변; old callback은 새 수명에 귀속하지 않음 |

원 근거: S4C `evidence/b08/uart_c_contract.json`, `uart_c/P_VISION_RESULT.md`, `evidence/sources/T35.txt` L36–55, T25 L26–55·L132 이후, T09 L88–114 및 Pi 원 품질 절, T26 L41–65, T10 L181–202. S4A 정확 선언은 위 Data/API anchor로 연결한다. 기존 [Interface UART-C](../../../interface/03_COMMUNICATION/30_UART_C.md#message-p-vision-result)는 읽기 전용으로 유지한다.

관련 기존 미결 IF-O09/IF-O12/IF-O13/IF-O19·D-02/D-03/D-05·G06, S6Q-04/06/07/12/14를 보존한다. 실제 합의 변경은 S6 정합 검토 후 확보된 승인 범위에서 통합하며 이 문서는 Owner나 Gate를 닫지 않는다. [상태 메시지](P_VISION_STATUS.md) · [Flow](../../flows/UART_C_PI_TO_CIS.md) · [Data](../../data/UART_C_DATA.md).
