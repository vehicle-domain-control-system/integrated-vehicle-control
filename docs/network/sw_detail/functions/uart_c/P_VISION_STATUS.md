# P_VISION_STATUS — Pi 기능 상태와 현재·최근 고장

`WIRE-P03-P_VISION_STATUS` / `RX-P03-P_VISION_STATUS-CIS`. P03 **PI → CIS**, VERSION **1**, TYPE **0x61**, Payload **16B**, Frame **26B**, 필드 **7개**다. `DESIGNED_CANDIDATE`는 정적 문서의 상세 상태이며 양단 채택·Driver·원 세대 수용이 확인된 실행 상태가 아니다. 해당 기능 바인딩은 `BLOCKED_BY_CONTRACT`로 유지한다.

[UART-C 개요](../../06_UART_C_OVERVIEW.md) → [L04](../../layers/L04_FRAME_CODEC.md) / [M06](../../modules/M06_UART_CODEC.md) → 본 함수 → [Data](../../data/UART_C_DATA.md) → [Pi→CIS 흐름](../../flows/UART_C_PI_TO_CIS.md). [Frame·Router](../UART_C_FRAME_ROUTER.md) / [송수신·Driver](../UART_C_TX_RX_CONTROL.md)는 실제 인계 이전의 공통 경계다.

<a id="data-api"></a>
## 기존 DTO·정확한 로컬 API

전체 선언의 단일 정본은 [NetVisionStatus](../../data/UART_C_DATA.md#type-NetVisionStatus)이며 S4A `data/T25.md#type-NetVisionStatus` / T25 L63–72의 7개 멤버를 유지한다. `NetVisionFaultCode`는 uint16_t raw 코드0..4다. 메타 정본 [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta)는 `link_sequence`와 실제 로컬 `received_at_ms`를 담으며 원 상태 평가 시각/번호가 아니다.

<a id="api-PiNet_PublishVisionStatus"></a>
<a id="api-CisApp_OnPiVisionStatus"></a>
```c
NetSubmitResult PiNet_PublishVisionStatus(
    const NetVisionStatus *message
);
void CisApp_OnPiVisionStatus(
    const NetVisionStatus *message,
    const NetUartRxMeta *meta
);
```

원형 근거는 T26 L70–72, T10 L208–211 및 기존 [Pi API](../../../interface/02_NODES/PI/20_API.md#api-pinet_publishvisionstatus), [CIS API](../../../interface/02_NODES/CIS/20_API.md#api-cisapp_onpivisionstatus)다. Pi 기능·고장 평가 생산자 → Pi Network/Serial 접수, UART-C → CIS Dispatcher → CIS 비전 기능·고장 App이라는 두 노드 내부 경계다. Pi 구현 언어/C ABI는 확정하지 않는다.

호출 `message`는 NULL 불가·borrowed IN이다. Pi runtime/Network가 실제 원 평가 generation과 Boot를 결합하고 성공 전에 7개 필드·원본 연결·시간 기준 전체를 복사한다. App이 Boot 슬롯을 초기화해야 한다는 새 의무는 없다. 아래 Validator는 **runtime 결합 뒤의 Wire용 DTO**만 검사하며 public API 입구의 미결합 Boot를 읽지 않는다. 결합 증거 부재는 접수 준비 미완료다. 과거 평가를 현재 Boot로 다시 찍어 유효한 상태로 만들지 않는다.

CIS `message/meta`는 같은 RX 사건이고 callback 반환까지 borrowed다. 지연 보관하는 소비자는 둘을 함께 복사한다. `void`는 상태 전달 notification이며 기능 정상·저장 commit·CAN 송신 결과를 뜻하지 않는다. Parser/Codec/App 전달은 ISR 밖의 소유자 Task/Superloop 문맥에서 수행하는 후보이며 실제 동기화·용량·Driver 연결은 IF-O12/D-03/D-05/G06 HOLD다.

<a id="wire"></a>
## 정확한 7필드

Payload B0=Frame B8, CRC 입력 **Frame B2..B23(22B)**, CRC 저장 **B24..B25 LE**다. 같은 외형의 UART-M VERSION2를 수용하지 않는다. Frame 전체 검증은 [UART-C Frame·Router](../UART_C_FRAME_ROUTER.md)에 둔다.

| Payload offset / 폭 | Frame offset | 원 필드 → typed 멤버 | 부호·순서·코드/단위 | 조건 / T35 행 |
|---|---|---|---|---|
| B0 / 4 | B8 | `PI_BOOT_ID` → `pi_boot_id` | u32 unsigned LE, 식별자 | 0 예약, 실제 발급/수용 HOLD / L68 |
| B4 / 4 | B12 | `STATUS_SEQUENCE` → `status_sequence` | u32 unsigned LE, 상태 평가 번호 | Vision/Link 번호와 독립; 초기값·wrap 미정 / L69 |
| B8 / 1 | B16 | `VISION_STATUS` → `vision_status` | u8 unsigned, N/A; 0 NOT_READY / 1 READY / 2 ACTIVE / 3 FAULT / 4 RECOVERING | 결과 관측을 생성하지 않음 / L70 |
| B9 / 2 | B17 | `ACTIVE_FAULT` → `active_fault` | u16 unsigned LE, 현재 고장0..4 | 현재 평가 / L71 |
| B11 / 2 | B19 | `LAST_FAULT` → `last_fault` | u16 unsigned LE, 최근 고장0..4 | 이번 Pi 기동의 이력 / L72 |
| B13 / 2 | B21 | `STATUS_AGE` → `status_age_ms` | u16 unsigned LE, ms | 65535 포화 하한; Vision AGE와 별개 / L73 |
| B15 / 1 | B23 | `AFFECTED_FUNCTION` → `affected_function` | u8 unsigned, N/A, bit mask | bit0 Presence / bit1 Count; 허용0x03 / L74 |

Fault 코드는 0 NONE, 1 INITIALIZATION, 2 CAMERA, 3 INFERENCE, 4 DATA_INVALID다(T25 L120–128). QualityReason의 VISION_FAULT raw4와 같은 숫자라고 같은 enum 의미로 처리하지 않는다. bit0와 bit1을 함께 설정한3은 허용되며 bit2..7은 정의되지 않았다. T35 L31의 1000ms 보고·3000ms 수신 중단은 후보이고 상태 원본의 최종 사용 기한 또는 Result의 최신성 한도가 아니다.

<a id="codec"></a>
## M06 F17/F18 — 전 필드 Codec

아래는 **PROPOSED_PRIVATE_FUNCTION**이다. 입력은 호출 중 안정, OUT은 독립된 호출자 저장소, 서로 겹치지 않으며 실패 OUT 불변이다. Encode true는 정확 16B 기록 완료이고 뒤의 용량을 쓰지 않는다. Decode true는 7필드 형식 완성일 뿐 기동 수용/정상 기능/외부 채택 증거가 아니다. Header/profile/방향/CRC 검증과 typed publish는 [공통 Router](../UART_C_FRAME_ROUTER.md)가 담당한다. [LE 도우미](../UART_C_FRAME_ROUTER.md)는 명시 폭만 읽고 쓴다.

```c
bool NetUartC_ValidateVisionStatus(const NetVisionStatus *message)
{
    if (message == NULL) return false;
    if (message->pi_boot_id == 0) return false; // 원 평가/runtime 결합 뒤
    if (message->vision_status > 4) return false;
    if (message->active_fault > 4) return false;
    if (message->last_fault > 4) return false;
    if ((message->affected_function & 0xFCu) != 0) return false;
    // ACTIVE_FAULT=0일 때 LAST_FAULT=0을 강제하지 않는다.
    // READY/FAULT/영향 mask 조합의 추가 규칙은 원문에 없어 만들지 않는다.
    return true;
}

bool NetUartC_EncodeVisionStatusPayload(const NetVisionStatus *message,
    uint8_t *out, uint16_t outCapacity)
{
    if (out == NULL || outCapacity < 16) return false;
    if (!NetUartC_ValidateVisionStatus(message)) return false;
    uint8_t p[16];
    NetUartC_WriteLe32(&p[0], message->pi_boot_id);
    NetUartC_WriteLe32(&p[4], message->status_sequence);
    p[8] = message->vision_status;
    NetUartC_WriteLe16(&p[9], message->active_fault);
    NetUartC_WriteLe16(&p[11], message->last_fault);
    NetUartC_WriteLe16(&p[13], message->status_age_ms);
    p[15] = message->affected_function;
    for (uint16_t i = 0; i < 16; ++i) out[i] = p[i];
    return true;
}

bool NetUartC_DecodeVisionStatusPayload(const uint8_t *payload,
    uint16_t length, NetVisionStatus *out)
{
    if (payload == NULL || out == NULL || length != 16) return false;
    NetVisionStatus tmp;
    tmp.pi_boot_id = NetUartC_ReadLe32(&payload[0]);
    tmp.status_sequence = NetUartC_ReadLe32(&payload[4]);
    tmp.vision_status = payload[8];
    tmp.active_fault = NetUartC_ReadLe16(&payload[9]);
    tmp.last_fault = NetUartC_ReadLe16(&payload[11]);
    tmp.status_age_ms = NetUartC_ReadLe16(&payload[13]);
    tmp.affected_function = payload[15];
    if (!NetUartC_ValidateVisionStatus(&tmp)) return false;
    *out = tmp;
    return true;
}
```

7필드 전체 raw 형식이 유효한 FAULT/RECOVERING 보고도 정상 열화 의미로 보존한다. 최신 READY를 만들기 위해 active_fault를 강제로0으로 바꾸거나 last_fault를 삭제하지 않는다. 미정 조합은 Owner 확인 대상이며 형식 Validator에 임의 의미 규칙을 추가하지 않는다. CRC 정상 typed 거부는 26B 전체를 소비하고 payload 내부 SYNC를 재해석하지 않는다.

<a id="state"></a>
## 상태 수명·순서·Result와의 분리

새 상태 평가에서만 STATUS_SEQUENCE가 바뀐다. 같은 상태 평가의 반복 송신은 PiBoot/StatusSequence를 유지하고 아직 미반영된 Pi 대기만 STATUS_AGE에 한 번 더한다. 65535는 경과 하한으로 보존하며 fresh로 사용하지 않는다. 최초 순번·wrap·역순 수용 근거가 없으면 낮거나 높은 번호만으로 새 상태를 승인하지 않는다. LINK_SEQUENCE는 방향별 frame 번호다. 정확한 같은 Link/raw 중복은 감시/callback을 갱신하지 않는다. 수용된 새 Link·같은 StatusSequence/의미·검증된 AGE의 정상 반복은 기존 `CisApp_OnPiVisionStatus`와 Status 수신 감시에 연결한다. Result 감시·VisionSequence·원 AGE는 갱신하지 않는다. 분류 자료/raw/meta·현재 보수 AGE는 같은 직렬 구간에 고정하고 void callback을 저장 ACK로 처리하지 않는다.

송신 반환은 기존 `NET_SUBMIT_OK/INVALID_ARGUMENT/NOT_READY/NO_CAPACITY` 네 의미를 따른다. 전체 사본 접수와 Driver accepted·최종 TX·참조 해제·CIS RX·기능 소비는 별개다. 같은 key의 미송신 현재값 교체 후보도 새로운 상태 평가임을 확인해야 하며, active에서 해제된 고장 이력을 임의 삭제하는 규칙으로 쓰지 않는다. Driver 참조 중 frame은 교체/수정하지 않는다. 공간 부족이면 부분 접수 없이 기존 원 자료를 보존한다. 후발 실패/정확한 버퍼 회수는 [공통 제어](../UART_C_TX_RX_CONTROL.md)에서 추적한다.

CIS는 Result cache와 Status record를 분리한다. Status만 먼저 도착해도 존재하지 않는 Presence/Count를0으로 만들지 않는다. READY/RECOVERING·새 STATUS_SEQUENCE·새 수신시각은 과거 Result의 PiBoot/VisionSequence/AGE/두 validity/reason을 수정할 자격이 없다. 상태에 원 관측의 sequence 참조가 없으므로 같은 PiBoot라는 이유만으로 다른 시점의 Result와 동일 평가라고 결합하지 않는다.

`ACTIVE_FAULT=0, LAST_FAULT=2`는 현재 해제와 이번 기동의 최근 카메라 고장 이력을 함께 나타낼 수 있다. LAST_FAULT는 상대 Pi가 보낸 원 값이며 CIS가 active0/상태timeout만으로 지우지 않는다. PiBoot 변경은 원 이력 세대가 바뀌는 사실이지만 과거 pending 상태를 새 기동의 이력으로 재표시하지 않는다.

CIS 진단 Owner가 이 상태를 `NetCisStatus`의 어떤 상태/고장/affected_function에 반영할지는 별도의 기능 mapping이다. UART-C의 bit0=Presence·bit1=Count를 CIS_STATUS의 bit0=비전·bit1=온도 등으로 raw 복사하지 않는다. Pi fault enum과 CIS fault enum도 독립이다. 실제 mapping·동일 원 평가 연결이 없으면 관련 진단 투영은 HOLD이며 다른 정상 센서 상태를 변경하지 않는다. [기존 CIS_STATUS](../can/CIS_STATUS.md) 설계는 그대로 유지한다.

<a id="counterexamples"></a>
## 반례와 복구

| ID | 입력·사건 | 분기 / 기대 출력·불변 조건 |
|---|---|---|
| VC-S01 | 조건부 정상: Boot7/Status20/READY/Active0/Last2/AGE30/mask1 | raw 검증 성공, 원 최근 고장2 보존. 현재 고장0이 최근 이력 삭제 조건이 아님 |
| VC-S02 | Boot7/Status21/FAULT/Active2/Last2/mask3 | 정상 열화 보고, mask 두 비트 보존. frame malformed로 버리거나 Result 품질을 자동 바꾸지 않음 |
| VC-S03 | affected_function=4 또는0x80 | Validator false, OUT 불변·callback 없음. CRC 정상 frame26B 전체 소비 |
| VC-S04 | status5 / active_fault5 / last_fault5 / 결합 뒤 Boot0 | raw 검사 거부, 임시 부분 DTO 공개 없음 |
| VC-S05 | P15/P17·VERSION2·PI 수신 방향 Type0x61·CRC 오류·CRC1byte만 | 완전/정확 경계 확보 전 callback 없음; 공통 Stream suffix와 대기 처리 |
| VC-S06 | Status READY 뒤 기존 Result INVALID·AGE65535 | Result 원 필드 불변, 카메라 관측 복구/AGE0 없음 |
| VC-S07 | Status가 Result보다 먼저 수신 | 독립 상태만 보관, 존재하지 않는 원 판정/인원수 생성 없음 |
| VC-S08 | 새 LinkSequence, 같은StatusSequence / wrap 판단 미확인 | 새 상태 평가 생성 없음. 순서 미확인 경로 HOLD, Link 번호를 원 평가 번호로 복사 안 함 |
| VC-S09 | old Boot 상태가 새 연결에서 도착 | 수용 근거 없으면 current 상태로 commit 안 함; half-frame epoch 혼합 금지 |
| VC-S10 | Pi mask1을 CIS_STATUS mask1에 그대로 복사 | 의미 mapping 근거 없어 해당 투영 HOLD. Presence와 CIS 비전 전체를 같은 bit 의미로 단정 안 함 |
| VC-S11 | runtime reset·실제 release 미확인·구 Driver callback | pending의 새 Boot 재귀속·참조 중 재사용·최종 성공 합성 없음 |

근거: S4C `evidence/b08/uart_c_contract.json`, `uart_c/P_VISION_STATUS.md`, T35 L59–74, T25 L63–85·L120–128, T26 L70–94, T10 L208–230, T37 L750–775. 기존 [UART-C Interface](../../../interface/03_COMMUNICATION/30_UART_C.md#message-p-vision-status)는 변경하지 않는다.

기존 IF-O09/IF-O12/IF-O13/IF-O19·D-02/D-03/D-05·G06 및 S6Q-04/06/07/12/14를 보존한다. 합의 차이는 대장에만 누적하고 S6 후 승인된 범위에서 통합한다. [Result](P_VISION_RESULT.md) · [Flow](../../flows/UART_C_PI_TO_CIS.md) · [Data](../../data/UART_C_DATA.md).
