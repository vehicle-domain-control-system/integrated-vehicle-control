# DOMAIN — Domain_Interface_v0.6_C2.h 전체 의미 타입

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

<a id="production-c2-compatibility"></a>
## 운영 코드의 참고 C2와 설계 후보 사이의 연결 제한

현재 `develop`의 Production C2 Target 헤더 `firmware/DOMAIN/src/interface/Domain_Interface_v0.5_C2_target.h` 및 `Vehicle_Types_v0.2.h`는 PR #85 병합 소스로 확인됐다. 아래 C 선언·공개 API는 별도의 **v0.3/v0.6 후보**이며 Production과 자동 ABI 호환된다고 보지 않는다. `docs/Domain/architecture/2_Domain_interface/Domain_Interface_v0.5.h`는 현재 Target 헤더가 아닌 과거 참고 파일이다. Target 빌드/실보드 검증은 별도다.

| 선언 경계 | v0.5 참고 헤더 | v0.6 후보 헤더 |
|---|---|---|
| 공개 DomainIf 함수 | 41개 | 72개 |
| QueryGet | 없음 | 18개 |
| TxPort Callback | 15개 | 26개 |
| RxObserver Callback | 12개 | 11개 |

위 표는 **과거 v0.5 참고 헤더(41개)와 v0.6 후보** 비교로, 현재 Production v0.5 C2 Target과는 다른 비교다. 현재 `develop`의 C2 Target에는 공개 API 72개·Query 18개·TxPort 26개·RxObserver 11개 선언이 있으며, 해당 PR의 실제 본체 구현은 44개로 확인됐다. 선언만 존재하는 API를 사용 가능한 구현으로 계산하지 않는다. 기존 후보 API를 임의로 자동 채택·삭제하거나 구형 Port로 되돌리지 않는다. **Network Adapter 연결·Target 빌드/ABI·실보드 검증은 대기** 상태다.

| 차이 범위 | 호환 검토 대상 |
|---|---|
| Query·Health·LocalFailure | 후보의 QueryGet18, passive context 검증, Network Health/LocalFailure 함수·타입의 실제 연결 |
| BCM Rx·Observer | 구 EcuState/Event/Fault 경계와 후보 Status/CommandResult/ProtectionEvent 경계, confirmation·도어 근거 타입 |
| Mobile Tx·TxPort | 구 통합 VehicleState 전송과 후보의 상태별 포트, RequestResult/Availability 인자 타입·UserSettings 추가 |
| CommandContext·DigitalKeyResult | 요청 Device·origin·commandAge, occurrence/resultAge와 SessionId 폭의 파급 |
| CIS Status·Interface enum | 환경 기능 하나와 센서별 상태의 차이, Pi UART/CAN 상태·Fault·세대 추가, enum 항목·숫자 의미 차이 |
| VSS·Warning·Window·AntiPinch | 상태·Fault·원 세대·입력별 근거, 원 Command 연결, Warning 종류·읽음·품질·SourceEvidence 차이 |
| Status·Function ID·헤더 의존성 | UNSUPPORTED와 Environment/COUNT 등의 추가, include guard/version·필수 Vehicle 헤더 변경 |

공통 이름의 함수에도 [Vehicle 타입 차이](11_VEHICLE_TYPES.md#production-compatibility)가 파급된다. 필드·enum 의미를 명시 변환하고, C 구조체 배치나 포트 개수가 같다는 이유만으로 호환된다고 판단하지 않는다. 컴파일러별 sizeof/정렬·호출 ABI와 실제 Manager/Observer/TxPort 바인딩은 미검증이며 Wire raw/길이·품질·시간 규칙은 기존 계약을 유지한다.

**DECLARED_SOURCE / BINDING_OPEN.** 아래 선언은 C2 v0.6 후보의 의미 타입이다. 공개 API 72개와 TxPort 26개를 정의한다. 실제 ABI·연결 검증은 남아 있다. 설명 표는 원 생성자·유효성·수명을 명시한다. 이 문서는 컴파일용 헤더가 아니며 C enum/padding을 Wire로 복사하지 않는다.

[노드 Data](10_DATA.md) · [노드 API](20_API.md) · [원 선언 헤더](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/36b4f43db03859518dd82664faa2464d13875a75/firmware/DOMAIN/src/interface/Domain_Interface_v0.6_C2.h)

## 타입 바로 찾기

| 사용하는 의미 | 전체 선언·필드 표 |
|---|---|
| 호출 상태·명령 공통 | [`DomainIf_Status_t`](#type-domainif-status-t), [`DomainFunctionId_t`](#type-domainfunctionid-t), [`DomainIf_CommandContext_t`](#type-domainif-commandcontext-t) |
| MOBILE 요청·경고 | [`DomainIf_MobileRequestType_t`](#type-domainif-mobilerequesttype-t), [`DomainIf_MobileRequestPayload_t`](#type-domainif-mobilerequestpayload-t), [`DomainIf_MobileRequest_t`](#type-domainif-mobilerequest-t), [`DomainWarningType_t`](#type-domainwarningtype-t), [`DomainIf_WarningAck_t`](#type-domainif-warningack-t), [`DomainWarningSeverity_t`](#type-domainwarningseverity-t), [`DomainWarningState_t`](#type-domainwarningstate-t), [`DomainIf_WarningInfo_t`](#type-domainif-warninginfo-t), [`DomainIf_WarningInputEvidence_t`](#type-domainif-warninginputevidence-t) |
| 가용성·모바일 결과·설정 | [`DomainIf_FunctionAvailability_t`](#type-domainif-functionavailability-t), [`DomainIf_FunctionAvailabilitySet_t`](#type-domainif-functionavailabilityset-t), [`DomainIf_DigitalKeyResult_t`](#type-domainif-digitalkeyresult-t), [`DomainIf_MobileResultTarget_t`](#type-domainif-mobileresulttarget-t), [`DomainIf_MobileRequestResult_t`](#type-domainif-mobilerequestresult-t), [`DomainIf_UserSettingsState_t`](#type-domainif-usersettingsstate-t) |
| BCM 명령·실제 결과 | [`DomainIf_BcmDoorCommand_t`](#type-domainif-bcmdoorcommand-t), [`DomainIf_BcmClimateCommand_t`](#type-domainif-bcmclimatecommand-t), [`DomainIf_BcmInteriorLightCommand_t`](#type-domainif-bcminteriorlightcommand-t), [`DomainIf_BcmCommandKind_t`](#type-domainif-bcmcommandkind-t), [`DomainIf_BcmDoorEvidenceType_t`](#type-domainif-bcmdoorevidencetype-t), [`DomainIf_BcmDoorResultEvidence_t`](#type-domainif-bcmdoorresultevidence-t), [`DomainIf_BcmCommandResult_t`](#type-domainif-bcmcommandresult-t), [`DomainIf_BcmProtectionEventType_t`](#type-domainif-bcmprotectioneventtype-t), [`DomainIf_BcmProtectionEvent_t`](#type-domainif-bcmprotectionevent-t), [`DomainIf_BcmStatusReport_t`](#type-domainif-bcmstatusreport-t) |
| Network Health·최종 로컬 실패 | [`NetworkLinkId_t`](#type-networklinkid-t), [`NetworkTransportState_t`](#type-networktransportstate-t), [`NetworkOperationalCounters_t`](#type-networkoperationalcounters-t), [`NetworkHealthSnapshot_t`](#type-networkhealthsnapshot-t), [`NetworkLocalItemKind_t`](#type-networklocalitemkind-t), [`NetworkLocalFailureReason_t`](#type-networklocalfailurereason-t), [`NetworkLocalFailure_t`](#type-networklocalfailure-t) |
| CIS 상태·허용 | [`DomainIf_CisInterfaceStatus_t`](#type-domainif-cisinterfacestatus-t), [`DomainIf_CisStatusReport_t`](#type-domainif-cisstatusreport-t), [`DomainIf_CisPowerPermission_t`](#type-domainif-cispowerpermission-t) |
| WINDOW · 구현 보류 | [`DomainIf_WindowFaultState_t`](#type-domainif-windowfaultstate-t), [`DomainIf_AntiPinchProtectionState_t`](#type-domainif-antipinchprotectionstate-t), [`DomainIf_AntiPinchReport_t`](#type-domainif-antipinchreport-t), [`DomainIf_WindowCommandResult_t`](#type-domainif-windowcommandresult-t), [`DomainIf_WindowCommand_t`](#type-domainif-windowcommand-t), [`DomainIf_WindowOperationPermission_t`](#type-domainif-windowoperationpermission-t), [`DomainIf_WindowDomainAlive_t`](#type-domainif-windowdomainalive-t) |
| VSS Event·State·Fault | [`DomainIf_VssOneShotEvent_t`](#type-domainif-vssoneshotevent-t), [`OccupantHazardState_t`](#type-occupanthazardstate-t), [`DomainIf_VssWarningState_t`](#type-domainif-vsswarningstate-t), [`DomainIf_VssStateReport_t`](#type-domainif-vssstatereport-t) |
| C2 등록 callback | [`DomainIf_RxObserver_t`](#type-domainif-rxobserver-t), [`DomainIf_TxPort_t`](#type-domainif-txport-t) |

<a id="type-domainif-status-t"></a>
## `DomainIf_Status_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOMAIN_IF_OK = 0,
    DOMAIN_IF_INVALID_ARGUMENT,
    DOMAIN_IF_NOT_INITIALIZED,
    DOMAIN_IF_NOT_AVAILABLE,
    DOMAIN_IF_QUEUE_FULL,
    DOMAIN_IF_REJECTED,
    DOMAIN_IF_UNSUPPORTED
} DomainIf_Status_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOMAIN_IF_OK` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_IF_INVALID_ARGUMENT` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_IF_NOT_INITIALIZED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_IF_NOT_AVAILABLE` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_IF_QUEUE_FULL` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_IF_REJECTED` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_IF_UNSUPPORTED` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainfunctionid-t"></a>
## `DomainFunctionId_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOMAIN_FUNCTION_DOOR = 0,
    DOMAIN_FUNCTION_CLIMATE,
    DOMAIN_FUNCTION_INTERIOR_LIGHT,
    DOMAIN_FUNCTION_DIGITAL_KEY,
    DOMAIN_FUNCTION_WINDOW,
    DOMAIN_FUNCTION_REAR_WARNING,
    DOMAIN_FUNCTION_OCCUPANT_WARNING,
    DOMAIN_FUNCTION_VSS,
    DOMAIN_FUNCTION_ENVIRONMENT,
    DOMAIN_FUNCTION_COUNT
} DomainFunctionId_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOMAIN_FUNCTION_DOOR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_CLIMATE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_INTERIOR_LIGHT` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_DIGITAL_KEY` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_WINDOW` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_REAR_WARNING` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_OCCUPANT_WARNING` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_VSS` | 7 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_ENVIRONMENT` | 8 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_FUNCTION_COUNT` | 9 | COUNT 센티널: 의미값/전송값으로 사용 금지. |

<a id="type-domainif-commandcontext-t"></a>
## `DomainIf_CommandContext_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainCommandId_t commandId;

    /**
     * 중앙이 최종 실행 목표를 결정한 논리 순서.
     * Domain 내부 provenance로만 유지하며 Network 필수 field 또는 wire 값으로 전송하지 않는다.
     */
    uint32_t decisionSequence;

    bool hasRequestContext;
    RequestContext_t requestContext;
    DeviceContextId_t requestDeviceContextId;
    CommandOrigin_t origin;
    uint32_t commandAgeMs; /* command birth age, not sensor sourceMeta.ageMs */

    SignalMeta_t meta; /* existing Domain decision/source provenance */
} DomainIf_CommandContext_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `commandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | Domain 기능 결정 owner의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | Domain 기능 결정/설정/결과 owner |
| `decisionSequence` | `uint32_t` | Domain 내부 최종 정책 판단 순서 u32. Network 필수 인자나 Wire 필드로 내보내지 않는다. | Domain 기능 결정/설정/결과 owner |
| `hasRequestContext` | `bool` | 원 MOBILE 요청 존재 bool. false 인 정책 명령에 가짜 Session/Request를 합성하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `requestContext` | [`RequestContext_t`](11_VEHICLE_TYPES.md#type-requestcontext-t) | 원 MOBILE Session/Request; hasRequestContext가 참일 때만 해석하고 현재 Session으로 재라벨 금지. | Domain 기능 결정/설정/결과 owner |
| `requestDeviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | 원 요청 단말 u32. 원 요청이 있을 때만 유효하고 현재 수신 대상 단말과 별도. | Domain 기능 결정/설정/결과 owner |
| `origin` | [`CommandOrigin_t`](11_VEHICLE_TYPES.md#type-commandorigin-t) | 원 요청/명령/해제 출처. 명시 enum mapping 필요; Network가 임의 판정하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `commandAgeMs` | `uint32_t` | Command 최초 생성 후 u32 ms. 센서 meta.ageMs와 별개; 송신에서 자체 대기를 추가하고 raw u16 포화. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-mobilerequesttype-t"></a>
## `DomainIf_MobileRequestType_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    MOBILE_REQUEST_DOOR_LOCK = 0,
    MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE,
    MOBILE_REQUEST_CLIMATE_AUTO_ENABLE,
    MOBILE_REQUEST_FAN_LEVEL,
    MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE,
    MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL,
    MOBILE_REQUEST_INTERIOR_LIGHT_COLOR,
    MOBILE_REQUEST_DIGITAL_KEY_SETTING
} DomainIf_MobileRequestType_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `MOBILE_REQUEST_DOOR_LOCK` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_CLIMATE_AUTO_ENABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_FAN_LEVEL` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_INTERIOR_LIGHT_COLOR` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_REQUEST_DIGITAL_KEY_SETTING` | 7 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-mobilerequestpayload-t"></a>
## `DomainIf_MobileRequestPayload_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef union
{
    DoorLockTarget_t doorTarget;
    int16_t targetTemperature;
    bool enable;
    FanLevel_t fanLevel;
    uint8_t levelPercent;
    RgbColor_t color;
    DigitalKeySetting_t digitalKeySetting;
} DomainIf_MobileRequestPayload_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `doorTarget` | [`DoorLockTarget_t`](11_VEHICLE_TYPES.md#type-doorlocktarget-t) | MOBILE 도어 LOCK/UNLOCK 의도. 원 MobileRequest type 선택 시에만 유효. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `targetTemperature` | `int16_t` | MOBILE 공조 목표 i16 centi-°C. 기능 허용 범위는 Domain 에서 검사. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `enable` | `bool` | 선택된 setting의 bool enable. type에 따라 공조·조명 대상이 달라진다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `fanLevel` | [`FanLevel_t`](11_VEHICLE_TYPES.md#type-fanlevel-t) | 팬 수준 OFF/LOW/MEDIUM/HIGH. 요청에서는 UNKNOWN 허용 금지. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `levelPercent` | `uint8_t` | 밝기 0~100%. 값 범위를 확인하고 enum/목표와 함께 부호화. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `color` | [`RgbColor_t`](11_VEHICLE_TYPES.md#type-rgbcolor-t) | RGB 각 0~255 인 의미 색상. C struct padding을 raw로 복사하지 않는다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `digitalKeySetting` | [`DigitalKeySetting_t`](11_VEHICLE_TYPES.md#type-digitalkeysetting-t) | MOBILE 디지털 키 설정 ENABLE/DISABLE. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |

<a id="type-domainif-mobilerequest-t"></a>
## `DomainIf_MobileRequest_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestContext_t context;
    DomainIf_MobileRequestType_t type;
    DomainIf_MobileRequestPayload_t payload;
    SignalMeta_t meta;
} DomainIf_MobileRequest_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `deviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | 검증된 원 요청/등록 단말 u32. Gateway 실제 peer/session binding과 대조. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `context` | [`RequestContext_t`](11_VEHICLE_TYPES.md#type-requestcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `type` | [`DomainIf_MobileRequestType_t`](12_DOMAIN_TYPES.md#type-domainif-mobilerequesttype-t) | 8 종 MOBILE request 분류. payload 에서 대응하는 정확한 union 멤버만 유효. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `payload` | [`DomainIf_MobileRequestPayload_t`](12_DOMAIN_TYPES.md#type-domainif-mobilerequestpayload-t) | MOBILE Request type으로 선택된 union 멤버만 유효. 다른 멤버를 함께 해석하지 않는다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |

<a id="type-domainwarningtype-t"></a>
## `DomainWarningType_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOMAIN_WARNING_WINDOW_ANTIPINCH = 0,
    DOMAIN_WARNING_OCCUPANT_HAZARD,
    DOMAIN_WARNING_REAR_OBSTACLE,
    DOMAIN_WARNING_DOOR_LEFT_OPEN,
    DOMAIN_WARNING_BCM_FAULT,
    DOMAIN_WARNING_CIS_FAULT,
    DOMAIN_WARNING_WINDOW_FAULT,
    DOMAIN_WARNING_VSS_FAULT,
    DOMAIN_WARNING_COUNT
} DomainWarningType_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOMAIN_WARNING_WINDOW_ANTIPINCH` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_OCCUPANT_HAZARD` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_REAR_OBSTACLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_DOOR_LEFT_OPEN` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_BCM_FAULT` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_CIS_FAULT` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_WINDOW_FAULT` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_VSS_FAULT` | 7 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_COUNT` | 8 | COUNT 센티널: 의미값/전송값으로 사용 금지. |

<a id="type-domainif-warningack-t"></a>
## `DomainIf_WarningAck_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    DomainWarningType_t type;
    OccurrenceId_t occurrenceId;
} DomainIf_WarningAck_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `deviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | 검증된 원 요청/등록 단말 u32. Gateway 실제 peer/session binding과 대조. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `sessionId` | [`SessionId_t`](11_VEHICLE_TYPES.md#type-sessionid-t) | 원 Session 식별 u64. 현재 전달 대상 Session과 원 요청 Session을 섞지 않는다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `type` | [`DomainWarningType_t`](12_DOMAIN_TYPES.md#type-domainwarningtype-t) | 원 Domain 경고 kind를 echo 하는 읽음 확인 대상. 경고 해제 신호가 아니다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |

<a id="type-domainwarningseverity-t"></a>
## `DomainWarningSeverity_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOMAIN_WARNING_SEVERITY_INFO = 0,
    DOMAIN_WARNING_SEVERITY_CAUTION,
    DOMAIN_WARNING_SEVERITY_EMERGENCY
} DomainWarningSeverity_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOMAIN_WARNING_SEVERITY_INFO` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_SEVERITY_CAUTION` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_SEVERITY_EMERGENCY` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainwarningstate-t"></a>
## `DomainWarningState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOMAIN_WARNING_STATE_CLEAR = 0,
    DOMAIN_WARNING_STATE_ACTIVE,
    DOMAIN_WARNING_STATE_UNCONFIRMED
} DomainWarningState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOMAIN_WARNING_STATE_CLEAR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_STATE_ACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOMAIN_WARNING_STATE_UNCONFIRMED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-warninginfo-t"></a>
## `DomainIf_WarningInfo_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainWarningType_t type;
    DomainWarningSeverity_t severity;
    DomainWarningState_t state;
    OccurrenceId_t occurrenceId;
    bool isRead;
    bool simulated; /* Domain-local; no silent drop on export, see O-C05 */
    DataQuality_t quality;
    QualityReason_t qualityReason;
    SourceEvidence_t source;
} DomainIf_WarningInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `type` | [`DomainWarningType_t`](12_DOMAIN_TYPES.md#type-domainwarningtype-t) | Domain 경고 8 종. C enum 숫자와 raw WARNING_TYPE의 숫자 대응은 명시 mapping. | Domain 기능 결정/설정/결과 owner |
| `severity` | [`DomainWarningSeverity_t`](12_DOMAIN_TYPES.md#type-domainwarningseverity-t) | Domain 경고 심각도 INFO/CAUTION/EMERGENCY. Network가 위험 재판정하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `state` | [`DomainWarningState_t`](12_DOMAIN_TYPES.md#type-domainwarningstate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | Domain 기능 결정/설정/결과 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | Domain 기능 결정/설정/결과 owner |
| `isRead` | `bool` | MOBILE 읽음 확인 상태. true는 위험 해제나 음향 종료 의미가 아니다. | Domain 기능 결정/설정/결과 owner |
| `simulated` | `bool` | Domain-local 시험 근거 bool. raw에 대응이 없는 상태를 정상 실측으로 조용히 export 하지 않음: WIRE_MAPPING_OPEN. | Domain 기능 결정/설정/결과 owner |
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | Domain 기능 결정/설정/결과 owner |
| `qualityReason` | [`QualityReason_t`](11_VEHICLE_TYPES.md#type-qualityreason-t) | 원 품질 사유. 무효/STALE/NO_DATA를 정상 값으로 바꾸지 않는다. | Domain 기능 결정/설정/결과 owner |
| `source` | [`SourceEvidence_t`](11_VEHICLE_TYPES.md#type-sourceevidence-t) | 원 경고 판단 근거 generation/sequence/AGE 또는 타입별 원본 출처. 다음 링크 전송순번으로 대체 금지. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-functionavailability-t"></a>
## `DomainIf_FunctionAvailability_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainFunctionId_t functionId;
    FunctionAvailability_t availability;
    ResultReason_t reason;
} DomainIf_FunctionAvailability_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `functionId` | [`DomainFunctionId_t`](12_DOMAIN_TYPES.md#type-domainfunctionid-t) | Domain 기능 식별. COUNT 센티널 전송 금지, NetFunctionId로 명시 변환. | Domain 기능 결정/설정/결과 owner |
| `availability` | [`FunctionAvailability_t`](11_VEHICLE_TYPES.md#type-functionavailability-t) | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | Domain 기능 결정/설정/결과 owner |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-functionavailabilityset-t"></a>
## `DomainIf_FunctionAvailabilitySet_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_FunctionAvailability_t items[DOMAIN_FUNCTION_COUNT];
    SignalMeta_t meta;
} DomainIf_FunctionAvailabilitySet_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `items` | [`DomainIf_FunctionAvailability_t[DOMAIN_FUNCTION_COUNT]`](12_DOMAIN_TYPES.md#type-domainif-functionavailability-t) | 각 Domain 기능의 가용성 항목 배열. DOMAIN_FUNCTION_COUNT와 인덱스 의미를 확인하고 지원하지 않는 Window는 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-digitalkeyresult-t"></a>
## `DomainIf_DigitalKeyResult_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    /**
     * false이면 현재 확정 가능한 차량 실행 결과가 없음.
     * confirmation=UNCONFIRMED과 함께 MOBILE에서는 UNKNOWN 의미로 표시 가능.
     */
    bool hasVehicleResult;

    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;

    UnlockOrigin_t origin;
    DomainCommandId_t relatedDoorCommandId;
    OccurrenceId_t occurrenceId; /* automatic unlock execution identity */
    uint32_t resultAgeMs;

    /**
     * 실제 BCM 잠금 해제 전이가 확인된 경우만 true.
     * ALREADY_AT_TARGET + DONE은 false.
     */
    bool actualUnlockTransition;
} DomainIf_DigitalKeyResult_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `hasVehicleResult` | `bool` | 현재 확인 가능한 차량 결과 존재 bool. false 이면 result를 검사해 FAILED/DONE을 생성하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `result` | [`RequestResult_t`](11_VEHICLE_TYPES.md#type-requestresult-t) | 기능 owner가 판정한 차량 처리 단계. Network local OK와 구분하며 DONE은 실행 근거가 있을 때만. | Domain 기능 결정/설정/결과 owner |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | Domain 기능 결정/설정/결과 owner |
| `confirmation` | [`ResultConfirmation_t`](11_VEHICLE_TYPES.md#type-resultconfirmation-t) | 차량 결과 확인 여부. UNCONFIRMED를 FAILED로 치환하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `origin` | [`UnlockOrigin_t`](11_VEHICLE_TYPES.md#type-unlockorigin-t) | 원 요청/명령/해제 출처. 명시 enum mapping 필요; Network가 임의 판정하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `relatedDoorCommandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | 자동 잠금 해제에 연결된 Domain 도어 Command ID. 이미 목표 상태 DONE은 실제 전이 아님. | Domain 기능 결정/설정/결과 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | Domain 기능 결정/설정/결과 owner |
| `resultAgeMs` | `uint32_t` | 원 결과 생성 후 u32 ms. Network가 자신이 대기한 구간만 더한다. | Domain 기능 결정/설정/결과 owner |
| `actualUnlockTransition` | `bool` | 실제 BCM 잠금 해제 전이 확인 bool. ALREADY_AT_TARGET+DONE은 false. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-bcmcommandkind-t"></a>
## `DomainIf_BcmCommandKind_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    BCM_COMMAND_KIND_DOOR = 0,
    BCM_COMMAND_KIND_CLIMATE,
    BCM_COMMAND_KIND_INTERIOR_LIGHT
} DomainIf_BcmCommandKind_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `BCM_COMMAND_KIND_DOOR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_COMMAND_KIND_CLIMATE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_COMMAND_KIND_INTERIOR_LIGHT` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-bcmdoorevidencetype-t"></a>
## `DomainIf_BcmDoorEvidenceType_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    BCM_DOOR_EVIDENCE_LOCK_COMPLETED = 0,
    BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED,
    BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET,
    BCM_DOOR_EVIDENCE_REQUEST_REJECTED,
    BCM_DOOR_EVIDENCE_REQUEST_FAILED
} DomainIf_BcmDoorEvidenceType_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `BCM_DOOR_EVIDENCE_LOCK_COMPLETED` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_DOOR_EVIDENCE_REQUEST_REJECTED` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_DOOR_EVIDENCE_REQUEST_FAILED` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-bcmdoorresultevidence-t"></a>
## `DomainIf_BcmDoorResultEvidence_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_BcmDoorEvidenceType_t type;
    OccurrenceId_t occurrenceId;
    DoorLockTarget_t target;
    DataQuality_t quality;
    uint32_t ageMs;
} DomainIf_BcmDoorResultEvidence_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `type` | [`DomainIf_BcmDoorEvidenceType_t`](12_DOMAIN_TYPES.md#type-domainif-bcmdoorevidencetype-t) | BCM lock/unlock 완료, 이미 목표 상태, 거부, 실패의 현장 실행 근거. | BCM 해당 구동·관측·보호 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | BCM 해당 구동·관측·보호 owner |
| `target` | [`DoorLockTarget_t`](11_VEHICLE_TYPES.md#type-doorlocktarget-t) | Domain 명령 도어 LOCK/UNLOCK 또는 결과 대상 typed 값. 해당 타입 enum만 해석. | BCM 해당 구동·관측·보호 owner |
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | BCM 해당 구동·관측·보호 owner |
| `ageMs` | `uint32_t` | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | BCM 해당 구동·관측·보호 owner |

<a id="type-domainif-bcmcommandresult-t"></a>
## `DomainIf_BcmCommandResult_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainCommandId_t commandId;
    DomainIf_BcmCommandKind_t kind;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
    bool hasDoorEvidence;
    DomainIf_BcmDoorResultEvidence_t doorEvidence;
} DomainIf_BcmCommandResult_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `commandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | Domain 기능 결정 owner의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | BCM 해당 구동·관측·보호 owner |
| `kind` | [`DomainIf_BcmCommandKind_t`](12_DOMAIN_TYPES.md#type-domainif-bcmcommandkind-t) | BCM 실행 명령 종류 DOOR/CLIMATE/INTERIOR_LIGHT. 결과의 commandId와 원 TX kind를 대조. | BCM 해당 구동·관측·보호 owner |
| `result` | [`RequestResult_t`](11_VEHICLE_TYPES.md#type-requestresult-t) | 기능 owner가 판정한 차량 처리 단계. Network local OK와 구분하며 DONE은 실행 근거가 있을 때만. | BCM 해당 구동·관측·보호 owner |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | BCM 해당 구동·관측·보호 owner |
| `confirmation` | [`ResultConfirmation_t`](11_VEHICLE_TYPES.md#type-resultconfirmation-t) | 결과 확인 여부. CAN BCM_RESULT에는 해당 필드가 없으므로 별도 증거 없이는 UNCONFIRMED 인계 후보. DONE만으로 CONFIRMED를 만들지 않는다. [양단 확인 제한](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result). | BCM 실행 근거 → DOMAIN 수신 Adapter의 명시 연결(PROPOSED/BINDING_OPEN) |
| `hasDoorEvidence` | `bool` | 도어 실행 근거 포함 bool. true 일 때 doorEvidence 해석, 보호 Event와 혼합 금지. | BCM 해당 구동·관측·보호 owner |
| `doorEvidence` | [`DomainIf_BcmDoorResultEvidence_t`](12_DOMAIN_TYPES.md#type-domainif-bcmdoorresultevidence-t) | BCM 도어 결과의 atomic 실행 근거. occurrence·target·quality·age 유지. | BCM 해당 구동·관측·보호 owner |

<a id="type-domainif-bcmprotectioneventtype-t"></a>
## `DomainIf_BcmProtectionEventType_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    BCM_PROTECTION_OVERHEAT_DETECTED = 0,
    BCM_PROTECTION_FAN_MISMATCH_DETECTED,
    BCM_PROTECTION_RECOVERY_CONFIRMED
} DomainIf_BcmProtectionEventType_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `BCM_PROTECTION_OVERHEAT_DETECTED` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_PROTECTION_FAN_MISMATCH_DETECTED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BCM_PROTECTION_RECOVERY_CONFIRMED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-bcmprotectionevent-t"></a>
## `DomainIf_BcmProtectionEvent_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_BcmProtectionEventType_t type;
    OccurrenceId_t occurrenceId;
    SourceGeneration_t sourceGeneration; /* identity/derived fault-warning lineage */
    DataQuality_t quality;
    uint32_t ageMs;
    bool hasRelatedCommand;
    DomainCommandId_t relatedCommandId;
    uint8_t affectedFunctionMask; /* semantic bits: door / climate / light */
} DomainIf_BcmProtectionEvent_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `type` | [`DomainIf_BcmProtectionEventType_t`](12_DOMAIN_TYPES.md#type-domainif-bcmprotectioneventtype-t) | BCM 과열·팬 불일치·회복 확인 보호 사건. 도어 실행 완료와 별도. | BCM 해당 구동·관측·보호 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | BCM 해당 구동·관측·보호 owner |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | BCM 해당 구동·관측·보호 owner |
| `ageMs` | `uint32_t` | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `hasRelatedCommand` | `bool` | 원 Domain 명령 연결 여부 bool. false 이면 relatedCommandId를 해석하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `relatedCommandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | 있으면 원 Domain Command ID echo. 로컬 사건에 가짜 명령 ID 생성 금지. | BCM 해당 구동·관측·보호 owner |
| `affectedFunctionMask` | `uint8_t` | 영향 기능 bitmask. 노드별 의미를 적용하고 알 수 없는 bit를 임의 확정하지 않는다. | BCM 해당 구동·관측·보호 owner |

<a id="type-domainif-bcmstatusreport-t"></a>
## `DomainIf_BcmStatusReport_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    CommonEcuState_t ecuState;
    uint16_t faultMask;
    FaultInfo_t activeFault; /* faultCode==0: explicitly NO active fault.
        category/state/quality are then ignored; no redundant hasActiveFault.
        If code!=0, category must be valid. Wire NONE=0 is not SENSOR=0. */
    uint8_t affectedFunctionMask;
    bool recovering;
    SourceGeneration_t sourceGeneration; /* for the derived BCM fault warning */
    SignalMeta_t meta;
} DomainIf_BcmStatusReport_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `ecuState` | [`CommonEcuState_t`](11_VEHICLE_TYPES.md#type-commonecustate-t) | 원 ECU 기능 상태. 링크 사용 가능성과 구별. | BCM 해당 구동·관측·보호 owner |
| `faultMask` | `uint16_t` | 활성/종합 fault bitmask. 노드별 bit 계약 및 활성 code와 구분. | BCM 해당 구동·관측·보호 owner |
| `activeFault` | [`FaultInfo_t`](11_VEHICLE_TYPES.md#type-faultinfo-t) | 현재 고장 의미 객체. faultCode=0 이면 category/state/quality는 해석하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `affectedFunctionMask` | `uint8_t` | 영향 기능 bitmask. 노드별 의미를 적용하고 알 수 없는 bit를 임의 확정하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `recovering` | `bool` | 기능 회복 진행 bool. 링크 recovering 및 성공 완료와 별개. | BCM 해당 구동·관측·보호 owner |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 owner |

<a id="type-domainif-warninginputevidence-t"></a>
## `DomainIf_WarningInputEvidence_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DataQuality_t quality;
    QualityReason_t reason;
    FunctionAvailability_t availability;
    SourceEvidence_t source;
} DomainIf_WarningInputEvidence_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `reason` | [`QualityReason_t`](11_VEHICLE_TYPES.md#type-qualityreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `availability` | [`FunctionAvailability_t`](11_VEHICLE_TYPES.md#type-functionavailability-t) | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `source` | [`SourceEvidence_t`](11_VEHICLE_TYPES.md#type-sourceevidence-t) | 원 경고 판단 근거 generation/sequence/AGE 또는 타입별 원본 출처. 다음 링크 전송순번으로 대체 금지. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-domainif-mobileresulttarget-t"></a>
## `DomainIf_MobileResultTarget_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    MOBILE_RESULT_TARGET_DOMAIN_SETTING = 0,
    MOBILE_RESULT_TARGET_BCM,
    MOBILE_RESULT_TARGET_WINDOW /* [구현 보류 — 설계 유지] */
} DomainIf_MobileResultTarget_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `MOBILE_RESULT_TARGET_DOMAIN_SETTING` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_RESULT_TARGET_BCM` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `MOBILE_RESULT_TARGET_WINDOW` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-mobilerequestresult-t"></a>
## `DomainIf_MobileRequestResult_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestResultInfo_t resultInfo; /* original Session + Request ID */
    bool hasVehicleResult;         /* false: resultInfo.result is not inspected */
    DomainIf_MobileResultTarget_t target;
    DomainCommandId_t commandId;   /* zero for a setting-only result */
    CommandOrigin_t origin;
    uint32_t resultAgeMs;          /* at API call; Network adds its own waiting */
} DomainIf_MobileRequestResult_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `deviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | 검증된 원 요청/등록 단말 u32. Gateway 실제 peer/session binding과 대조. | Domain 기능 결정/설정/결과 owner |
| `resultInfo` | [`RequestResultInfo_t`](11_VEHICLE_TYPES.md#type-requestresultinfo-t) | 원 Session/Request를 유지하는 결과 정보. target·command·origin·hasVehicleResult와 같이 소비. | Domain 기능 결정/설정/결과 owner |
| `hasVehicleResult` | `bool` | 현재 확인 가능한 차량 결과 존재 bool. false 이면 result를 검사해 FAILED/DONE을 생성하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `target` | [`DomainIf_MobileResultTarget_t`](12_DOMAIN_TYPES.md#type-domainif-mobileresulttarget-t) | 결과 대상 의미: DOMAIN_SETTING/BCM/WINDOW의 기존 enum. 도어 LOCK/UNLOCK 목표가 아니며 hasVehicleResult·commandId와 같이 해석. WINDOW는 구현 보류다. | Domain 기능 결정/설정/결과 owner |
| `commandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | Domain 기능 결정 owner의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | Domain 기능 결정/설정/결과 owner |
| `origin` | [`CommandOrigin_t`](11_VEHICLE_TYPES.md#type-commandorigin-t) | 원 요청/명령/해제 출처. 명시 enum mapping 필요; Network가 임의 판정하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `resultAgeMs` | `uint32_t` | 원 결과 생성 후 u32 ms. Network가 자신이 대기한 구간만 더한다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-usersettingsstate-t"></a>
## `DomainIf_UserSettingsState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    bool hasTargetTemperature;
    int16_t targetTemperatureCentiC;
    bool hasClimateAutoSetting;
    bool climateAutoEnabled;
    bool hasUserFanLevel;
    FanLevel_t userFanLevel;
    bool hasInteriorLightEnableSetting;
    bool interiorLightEnabled;
    bool hasNormalLightLevel;
    uint8_t normalLightLevelPercent;
    bool hasNormalLightColor;
    RgbColor_t normalLightColor;
    SignalMeta_t meta;
} DomainIf_UserSettingsState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `hasTargetTemperature` | `bool` | 목표 온도 설정 존재 bool. false 이면 해당 수치 해석 금지. | Domain 기능 결정/설정/결과 owner |
| `targetTemperatureCentiC` | `int16_t` | 설정 목표 온도 i16 centi-°C, hasTargetTemperature 일 때 유효. 허용 온도 범위 팀 기능 계약 참조. | Domain 기능 결정/설정/결과 owner |
| `hasClimateAutoSetting` | `bool` | 자동 공조 설정 존재 bool. | Domain 기능 결정/설정/결과 owner |
| `climateAutoEnabled` | `bool` | 자동 공조 사용 bool. hasClimateAutoSetting 일 때 유효. | Domain 기능 결정/설정/결과 owner |
| `hasUserFanLevel` | `bool` | 사용자 팬 설정 존재 bool. | Domain 기능 결정/설정/결과 owner |
| `userFanLevel` | [`FanLevel_t`](11_VEHICLE_TYPES.md#type-fanlevel-t) | 사용자 팬 설정. hasUserFanLevel 일 때 유효. | Domain 기능 결정/설정/결과 owner |
| `hasInteriorLightEnableSetting` | `bool` | 실내 조명 enable 설정 존재 bool. | Domain 기능 결정/설정/결과 owner |
| `interiorLightEnabled` | `bool` | 실내 조명 사용 bool; 대응 has가 참일 때 유효. | Domain 기능 결정/설정/결과 owner |
| `hasNormalLightLevel` | `bool` | 일반 실내 밝기 설정 존재 bool. | Domain 기능 결정/설정/결과 owner |
| `normalLightLevelPercent` | `uint8_t` | 일반 실내 밝기 0~100%, 대응 has가 참일 때 유효. | Domain 기능 결정/설정/결과 owner |
| `hasNormalLightColor` | `bool` | 일반 실내 RGB 설정 존재 bool. | Domain 기능 결정/설정/결과 owner |
| `normalLightColor` | [`RgbColor_t`](11_VEHICLE_TYPES.md#type-rgbcolor-t) | 일반 실내 RGB 0~255/채널, 대응 has가 참일 때 유효. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |

<a id="type-networklinkid-t"></a>
## `NetworkLinkId_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    NETWORK_LINK_CAN = 0,
    NETWORK_LINK_UART_M,
    NETWORK_LINK_UART_C
} NetworkLinkId_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `NETWORK_LINK_CAN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_LINK_UART_M` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_LINK_UART_C` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-networktransportstate-t"></a>
## `NetworkTransportState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    NETWORK_TRANSPORT_UNKNOWN = 0,
    NETWORK_TRANSPORT_UNAVAILABLE,
    NETWORK_TRANSPORT_AVAILABLE,
    NETWORK_TRANSPORT_RECOVERING
} NetworkTransportState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `NETWORK_TRANSPORT_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_TRANSPORT_UNAVAILABLE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_TRANSPORT_AVAILABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_TRANSPORT_RECOVERING` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-networkoperationalcounters-t"></a>
## `NetworkOperationalCounters_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    uint32_t rxCrcRejectCount;
    uint32_t rxFormatRejectCount;
    uint32_t rxOverflowCount;
    uint32_t txFinalFailureCount;
    uint32_t busOffCount;
} NetworkOperationalCounters_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `rxCrcRejectCount` | `uint32_t` | CRC 거부 수 u32 saturating. | 해당 노드 Network 감독/전송 owner |
| `rxFormatRejectCount` | `uint32_t` | 길이/enum/형식 거부 수 u32 saturating. | 해당 노드 Network 감독/전송 owner |
| `rxOverflowCount` | `uint32_t` | 수신 capacity 초과 수 u32 saturating. | 해당 노드 Network 감독/전송 owner |
| `txFinalFailureCount` | `uint32_t` | 로컬 제출 후 최종 전송 실패 수 u32 saturating, 차량 FAILED 합성 금지. | 해당 노드 Network 감독/전송 owner |
| `busOffCount` | `uint32_t` | CAN bus-off 수 u32 saturating. | 해당 노드 Network 감독/전송 owner |

<a id="type-networkhealthsnapshot-t"></a>
## `NetworkHealthSnapshot_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    NetworkLinkId_t link;
    NetworkTransportState_t transportState;
    bool hasValidRx;
    uint32_t lastValidRxAgeMs; /* meaningful only when hasValidRx */
    NetworkOperationalCounters_t counters;
} NetworkHealthSnapshot_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `link` | [`NetworkLinkId_t`](12_DOMAIN_TYPES.md#type-networklinkid-t) | 현재 노드의 링크 CAN/UART_M/UART_C. DOMAIN 에서 없는 UART-C 요청은 UNSUPPORTED. | 해당 노드 Network 감독/전송 owner |
| `transportState` | [`NetworkTransportState_t`](12_DOMAIN_TYPES.md#type-networktransportstate-t) | 링크 UNKNOWN/UNAVAILABLE/AVAILABLE/RECOVERING. ECU 기능 availability 아님. | 해당 노드 Network 감독/전송 owner |
| `hasValidRx` | `bool` | 이 링크에서 현재까지 유효 수신 기록 존재 bool. | 해당 노드 Network 감독/전송 owner |
| `lastValidRxAgeMs` | `uint32_t` | 마지막 유효 수신 후 u32 ms. hasValidRx가 참일 때만 해석. | 해당 노드 Network 감독/전송 owner |
| `counters` | [`NetworkOperationalCounters_t`](12_DOMAIN_TYPES.md#type-networkoperationalcounters-t) | Network 소유 u32 saturating 운영 카운터. 요청 결과나 새로운 진단 store가 아님. | 해당 노드 Network 감독/전송 owner |

<a id="type-networklocalitemkind-t"></a>
## `NetworkLocalItemKind_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    NETWORK_LOCAL_ITEM_COMMAND = 0,
    NETWORK_LOCAL_ITEM_REQUEST_RESULT,
    NETWORK_LOCAL_ITEM_OCCURRENCE /* one-shot Event or occurrence-keyed Result */
} NetworkLocalItemKind_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `NETWORK_LOCAL_ITEM_COMMAND` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_LOCAL_ITEM_REQUEST_RESULT` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_LOCAL_ITEM_OCCURRENCE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-networklocalfailurereason-t"></a>
## `NetworkLocalFailureReason_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL = 0,
    NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND,
    NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND
} NetworkLocalFailureReason_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-networklocalfailure-t"></a>
## `NetworkLocalFailure_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    NetworkLinkId_t link;
    NetworkLocalItemKind_t itemKind;
    NetworkLocalFailureReason_t reason;
    SourceGeneration_t identityGeneration; /* captured at admission:
        COMMAND: command producer/current Domain generation;
        REQUEST_RESULT: result publication producer generation;
        OCCURRENCE: occurrence producer generation.
        Not target ECU BOOT_ID, RequestContext Session or current App Session.
        Never relabel an old item with a new admitted generation. */
    uint32_t elapsedSinceAdmissionMs; /* local evidence, not wire/source age */
    bool hasCommandId;
    DomainCommandId_t commandId;
    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext; /* original Session, never current relabel */
    bool hasOccurrenceId;
    OccurrenceId_t occurrenceId;
} NetworkLocalFailure_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `link` | [`NetworkLinkId_t`](12_DOMAIN_TYPES.md#type-networklinkid-t) | 현재 노드의 링크 CAN/UART_M/UART_C. DOMAIN 에서 없는 UART-C 요청은 UNSUPPORTED. | 해당 노드 Network 감독/전송 owner |
| `itemKind` | [`NetworkLocalItemKind_t`](12_DOMAIN_TYPES.md#type-networklocalitemkind-t) | 기존 로컬 인계 항목 종류 COMMAND/REQUEST_RESULT/OCCURRENCE. periodic loss와 별개. | 해당 노드 Network 감독/전송 owner |
| `reason` | [`NetworkLocalFailureReason_t`](12_DOMAIN_TYPES.md#type-networklocalfailurereason-t) | 로컬 제출 후 최종 driver 실패/송신 전 만료/문맥 무효의 typed 원인. 차량 FAILED를 뜻하지 않는다. | 해당 노드 Network 감독/전송 owner |
| `identityGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 최초 admission 때의 원 identity 생산 세대. 대상 Boot·Request Session·현재 App Session으로 바꾸지 않는다. | 해당 노드 Network 감독/전송 owner |
| `elapsedSinceAdmissionMs` | `uint32_t` | Network local admission 후 경과 u32 ms. wire/source age와 다름. | 해당 노드 Network 감독/전송 owner |
| `hasCommandId` | `bool` | 원 Command ID 포함 bool. true 일 때 commandId 해석. | 해당 노드 Network 감독/전송 owner |
| `commandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | Domain 기능 결정 owner의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | 해당 노드 Network 감독/전송 owner |
| `hasRequestContext` | `bool` | 원 MOBILE 요청 존재 bool. false 인 정책 명령에 가짜 Session/Request를 합성하지 않는다. | 해당 노드 Network 감독/전송 owner |
| `requestDeviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | 원 요청 단말 u32. 원 요청이 있을 때만 유효하고 현재 수신 대상 단말과 별도. | 해당 노드 Network 감독/전송 owner |
| `requestContext` | [`RequestContext_t`](11_VEHICLE_TYPES.md#type-requestcontext-t) | 원 MOBILE Session/Request; hasRequestContext가 참일 때만 해석하고 현재 Session으로 재라벨 금지. | 해당 노드 Network 감독/전송 owner |
| `hasOccurrenceId` | `bool` | 원 occurrence 포함 bool. true 일 때 occurrenceId 해석. | 해당 노드 Network 감독/전송 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | 해당 노드 Network 감독/전송 owner |

<a id="type-domainif-windowfaultstate-t"></a>
## `DomainIf_WindowFaultState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    FaultInfo_t fault;
    OccurrenceId_t occurrenceId;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration;
} DomainIf_WindowFaultState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `fault` | [`FaultInfo_t`](11_VEHICLE_TYPES.md#type-faultinfo-t) | 고장 의미 객체. 별도 occurrence/sourceGeneration/meta를 버리지 않는다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |

<a id="type-domainif-bcmdoorcommand-t"></a>
## `DomainIf_BcmDoorCommand_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    DoorLockTarget_t target;
} DomainIf_BcmDoorCommand_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 owner |
| `target` | [`DoorLockTarget_t`](11_VEHICLE_TYPES.md#type-doorlocktarget-t) | Domain 명령 도어 LOCK/UNLOCK 또는 결과 대상 typed 값. 해당 타입 enum만 해석. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-bcmclimatecommand-t"></a>
## `DomainIf_BcmClimateCommand_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    FanLevel_t fanTarget;
    ThermalDirection_t thermalDirection;
    uint8_t thermalTargetLevelPercent;
} DomainIf_BcmClimateCommand_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 owner |
| `fanTarget` | [`FanLevel_t`](11_VEHICLE_TYPES.md#type-fanlevel-t) | Domain 확정 팬 목표. BCM 실제 구동 피드백과 별개. | Domain 기능 결정/설정/결과 owner |
| `thermalDirection` | [`ThermalDirection_t`](11_VEHICLE_TYPES.md#type-thermaldirection-t) | BCM 열 출력 방향 IDLE/COOL/HEAT. 방향 품질과 별도로 소비. | Domain 기능 결정/설정/결과 owner |
| `thermalTargetLevelPercent` | `uint8_t` | Domain 확정 열 출력 목표 0~100%. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-bcminteriorlightcommand-t"></a>
## `DomainIf_BcmInteriorLightCommand_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
} DomainIf_BcmInteriorLightCommand_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 owner |
| `type` | [`InteriorLightType_t`](11_VEHICLE_TYPES.md#type-interiorlighttype-t) | Domain 확정 실내 조명 종류 NORMAL/GOODBYE/WARNING/FAULT. Mobile raw 요청 권한과 별도. | Domain 기능 결정/설정/결과 owner |
| `levelPercent` | `uint8_t` | 밝기 0~100%. 값 범위를 확인하고 enum/목표와 함께 부호화. | Domain 기능 결정/설정/결과 owner |
| `color` | [`RgbColor_t`](11_VEHICLE_TYPES.md#type-rgbcolor-t) | RGB 각 0~255 인 의미 색상. C struct padding을 raw로 복사하지 않는다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-cisinterfacestatus-t"></a>
## `DomainIf_CisInterfaceStatus_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    CIS_INTERFACE_AVAILABLE = 0, /* semantic READY; explicitly mapped */
    CIS_INTERFACE_UNAVAILABLE = 2,
    CIS_INTERFACE_RECOVERING = 3 /* no public DEGRADED; never raw-cast */
} DomainIf_CisInterfaceStatus_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `CIS_INTERFACE_AVAILABLE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_INTERFACE_UNAVAILABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_INTERFACE_RECOVERING` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-cisstatusreport-t"></a>
## `DomainIf_CisStatusReport_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    CisState_t ecuState;
    CisFunctionStatus_t occupantFunctionStatus;
    CisFunctionStatus_t temperatureFunctionStatus;
    CisFunctionStatus_t humidityFunctionStatus;
    CisFunctionStatus_t illuminanceFunctionStatus;
    CisFunctionStatus_t rearFunctionStatus;
    DomainIf_CisInterfaceStatus_t piUartStatus;
    DomainIf_CisInterfaceStatus_t canInterfaceStatus;
    uint16_t faultMask;
    uint16_t activeFaultCode;
    bool recovering;
    uint16_t lastFaultCode;
    OccurrenceId_t lastFaultOccurrenceId;
    uint8_t affectedFunctionMask;
    SourceGeneration_t sourceGeneration; /* derived CIS fault-warning lineage */
    SignalMeta_t meta;
} DomainIf_CisStatusReport_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `ecuState` | [`CisState_t`](11_VEHICLE_TYPES.md#type-cisstate-t) | 원 ECU 기능 상태. 링크 사용 가능성과 구별. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `occupantFunctionStatus` | [`CisFunctionStatus_t`](11_VEHICLE_TYPES.md#type-cisfunctionstatus-t) | CIS 탑승자 판정 기능 상태. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `temperatureFunctionStatus` | [`CisFunctionStatus_t`](11_VEHICLE_TYPES.md#type-cisfunctionstatus-t) | CIS 온도 기능 상태. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `humidityFunctionStatus` | [`CisFunctionStatus_t`](11_VEHICLE_TYPES.md#type-cisfunctionstatus-t) | CIS 습도 기능 상태. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `illuminanceFunctionStatus` | [`CisFunctionStatus_t`](11_VEHICLE_TYPES.md#type-cisfunctionstatus-t) | CIS 실내 조도 기능 상태. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `rearFunctionStatus` | [`CisFunctionStatus_t`](11_VEHICLE_TYPES.md#type-cisfunctionstatus-t) | CIS 후방 초음파 기능 상태. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `piUartStatus` | [`DomainIf_CisInterfaceStatus_t`](12_DOMAIN_TYPES.md#type-domainif-cisinterfacestatus-t) | CIS Network가 관측한 Pi UART 링크 상태. 0 READY/2 UNAVAILABLE/3 RECOVERING, 공개 DEGRADED 없음. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `canInterfaceStatus` | [`DomainIf_CisInterfaceStatus_t`](12_DOMAIN_TYPES.md#type-domainif-cisinterfacestatus-t) | CIS Network가 관측한 CAN 링크 상태. 원 raw와 C enum 숫자 명시 변환. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `faultMask` | `uint16_t` | 활성/종합 fault bitmask. 노드별 bit 계약 및 활성 code와 구분. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `activeFaultCode` | `uint16_t` | 활성 고장 code u16. 0 없음, lastFaultCode와 별도. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `recovering` | `bool` | 기능 회복 진행 bool. 링크 recovering 및 성공 완료와 별개. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `lastFaultCode` | `uint16_t` | 원 ECU 최근 고장 code u16. 활성 고장 여부가 아니다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `lastFaultOccurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 최근 고장 발생 식별, 코드·원 세대·AGE와 함께 소비. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `affectedFunctionMask` | `uint8_t` | 영향 기능 bitmask. 노드별 의미를 적용하고 알 수 없는 bit를 임의 확정하지 않는다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |

<a id="type-domainif-cispowerpermission-t"></a>
## `DomainIf_CisPowerPermission_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    bool vehiclePowerAllowed;
    DataQuality_t quality;
    SignalMeta_t meta;
} DomainIf_CisPowerPermission_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `vehiclePowerAllowed` | `bool` | Domain 전원 허용 bool. 유효 true/OK는 확정 ALLOW, false/OK는 확정 DENY. UNKNOWN을 false+OK로 축약하지 않는다. 아래 권장 투영을 따른다. | Domain 기능 결정/설정/결과 owner |
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |

<a id="delta-permission-projection"></a>
**추가 권장안 / DOMAIN 연동 조건 확인 필요:** 미관측·근거 없음은 false+NO_DATA, 수명 만료는 false+STALE, 신뢰할 수 없는 평가 실패는 false+INVALID로 투영하는 안이다. `quality`와 `meta.quality`가 일치해야 하며 충돌 시 기존 CAN 후보처럼 확인 불가로 제한하고 임의 OK 선택을 금지한다. 무효 bool을 정상 DENY로 읽지 않는다. UNKNOWN+OK 원 품질을 반드시 보존해야 하면 현재 bool+quality+meta 조합으로는 의미를 손실 없이 표현할 수 없다. 의미 축약은 DOMAIN Adapter의 정책 수용이 필요하고, 원 의미·원 품질·원인 근거의 DOMAIN 내부 보존 위치도 확인 대기다. Network는 품질 검증·전달을 담당하며 이 정보 손실을 숨기거나 새 필드를 추가하지 않는다. [원인별 권장표·상태](../../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

`meta.updateSequence`는 DOMAIN의 실제 성공 평가에 대한 원 순번이다. 실패 보고는 마지막 성공 순번·원 시각을 보존한 현재 품질 하향이며 새 성공 평가가 아니다. CAN 합성 UPDATE_SEQUENCE·TX_SEQUENCE로 이를 대체하지 않는다. [제출 조건](20_API.md#delta-permission-submit)

<a id="type-domainif-antipinchprotectionstate-t"></a>
## `DomainIf_AntiPinchProtectionState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    ANTIPINCH_PROTECTION_IDLE = 0,
    ANTIPINCH_PROTECTION_REVERSING,
    ANTIPINCH_PROTECTION_COMPLETED,
    ANTIPINCH_PROTECTION_ABORTED
} DomainIf_AntiPinchProtectionState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `ANTIPINCH_PROTECTION_IDLE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ANTIPINCH_PROTECTION_REVERSING` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ANTIPINCH_PROTECTION_COMPLETED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ANTIPINCH_PROTECTION_ABORTED` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-antipinchreport-t"></a>
## `DomainIf_AntiPinchReport_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    OccurrenceId_t occurrenceId;
    DomainIf_AntiPinchProtectionState_t protectionState;
    bool outputDisabled;
    bool movementStopped;
    ResultReason_t reason;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration; /* [구현 보류 — 설계 유지] */
} DomainIf_AntiPinchReport_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `protectionState` | [`DomainIf_AntiPinchProtectionState_t`](12_DOMAIN_TYPES.md#type-domainif-antipinchprotectionstate-t) | WINDOW 로컬 anti-pinch IDLE/REVERSING/COMPLETED/ABORTED. END Event와 동의어 아님. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `outputDisabled` | `bool` | WINDOW 로컬 출력 차단 근거 bool; 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `movementStopped` | `bool` | WINDOW 실제 이동 정지 근거 bool; 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |

<a id="type-domainif-windowcommandresult-t"></a>
## `DomainIf_WindowCommandResult_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainCommandId_t commandId;
    RequestResult_t result;
    ResultReason_t reason;
    uint8_t channel;
    SignalMeta_t meta;
} DomainIf_WindowCommandResult_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `commandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | Domain 기능 결정 owner의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | Domain 기능 결정/설정/결과 owner |
| `result` | [`RequestResult_t`](11_VEHICLE_TYPES.md#type-requestresult-t) | 기능 owner가 판정한 차량 처리 단계. Network local OK와 구분하며 DONE은 실행 근거가 있을 때만. | Domain 기능 결정/설정/결과 owner |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | Domain 기능 결정/설정/결과 owner |
| `channel` | `uint8_t` | WINDOW 채널 u8. 지원 채널 범위는 구현 전 확정 필요; 현재 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-windowcommand-t"></a>
## `DomainIf_WindowCommand_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    uint8_t channel;
    WindowCommand_t command;
    uint8_t targetPositionPercent;
} DomainIf_WindowCommand_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 owner |
| `channel` | `uint8_t` | WINDOW 채널 u8. 지원 채널 범위는 구현 전 확정 필요; 현재 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `command` | [`WindowCommand_t`](11_VEHICLE_TYPES.md#type-windowcommand-t) | WINDOW 목표 명령 OPEN/CLOSE/STOP/VENT. VENT70↔80% 충돌 미확정; 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `targetPositionPercent` | `uint8_t` | WINDOW 목표 위치 0~100%. VENT70↔80% 충돌은 FUNCTION_POLICY_OPEN; 현재 구현 보류. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-windowoperationpermission-t"></a>
## `DomainIf_WindowOperationPermission_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    bool operationAllowed;
    DataQuality_t quality;
    SignalMeta_t meta;
} DomainIf_WindowOperationPermission_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `operationAllowed` | `bool` | Domain WINDOW 동작 허용 bool. 품질/기한 유효할 때만 사용; 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-windowdomainalive-t"></a>
## `DomainIf_WindowDomainAlive_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    bool domainLinkAvailable;
    SignalMeta_t meta;
} DomainIf_WindowDomainAlive_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `domainLinkAvailable` | `bool` | Domain↔WINDOW 링크 감독 근거 bool. 도메인 차량 정책 허용과 다름; 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |

<a id="type-domainif-vssoneshotevent-t"></a>
## `DomainIf_VssOneShotEvent_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    VssOneShotEvent_t event;
    OccurrenceId_t occurrenceId;
    SignalMeta_t meta; /* quality/original age; updateSequence is ignored by
        VSS_EVENT codec and occurrence dedup/replay, but semantic zero is not forced */
    FunctionAvailability_t availability;
    bool hasRelatedCommand;
    DomainCommandId_t relatedCommandId;
} DomainIf_VssOneShotEvent_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `event` | [`VssOneShotEvent_t`](11_VEHICLE_TYPES.md#type-vssoneshotevent-t) | Domain이 확정한 VSS 일회 음향 Event. occurrence·원 quality/AGE·availability와 함께 사용. | Domain 기능 결정/설정/결과 owner |
| `occurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | Domain 기능 결정/설정/결과 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 owner |
| `availability` | [`FunctionAvailability_t`](11_VEHICLE_TYPES.md#type-functionavailability-t) | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | Domain 기능 결정/설정/결과 owner |
| `hasRelatedCommand` | `bool` | 원 Domain 명령 연결 여부 bool. false 이면 relatedCommandId를 해석하지 않는다. | Domain 기능 결정/설정/결과 owner |
| `relatedCommandId` | [`DomainCommandId_t`](11_VEHICLE_TYPES.md#type-domaincommandid-t) | 있으면 원 Domain Command ID echo. 로컬 사건에 가짜 명령 ID 생성 금지. | Domain 기능 결정/설정/결과 owner |

<a id="type-occupanthazardstate-t"></a>
## `OccupantHazardState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    OCCUPANT_HAZARD_CLEAR = 0,
    OCCUPANT_HAZARD_ACTIVE,
    OCCUPANT_HAZARD_UNCONFIRMED
} OccupantHazardState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `OCCUPANT_HAZARD_CLEAR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `OCCUPANT_HAZARD_ACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `OCCUPANT_HAZARD_UNCONFIRMED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-domainif-vsswarningstate-t"></a>
## `DomainIf_VssWarningState_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    AntiPinchState_t windowAntiPinchState; /* [구현 보류 — 설계 유지] */
    DomainIf_WarningInputEvidence_t antiPinchEvidence;
    OccupantHazardState_t occupantHazardState;
    DomainIf_WarningInputEvidence_t occupantEvidence;
    RearObstacleState_t rearObstacleState;
    DomainIf_WarningInputEvidence_t rearEvidence;
    RearDetectionActivation_t rearDetectionActivation;
    SignalMeta_t rearActivationMeta; /* independent Domain policy evaluation */
    QualityReason_t rearActivationReason;
    /* No format marker or separate activation source boot here. */
} DomainIf_VssWarningState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `windowAntiPinchState` | [`AntiPinchState_t`](11_VEHICLE_TYPES.md#type-antipinchstate-t) | Domain에 인계된 WINDOW 끼임 의미. 현재 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `antiPinchEvidence` | [`DomainIf_WarningInputEvidence_t`](12_DOMAIN_TYPES.md#type-domainif-warninginputevidence-t) | 끼임 원 품질/가용성/세대/평가/AGE. WINDOW 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `occupantHazardState` | [`OccupantHazardState_t`](12_DOMAIN_TYPES.md#type-occupanthazardstate-t) | Domain 잔류 탑승자 위험 CLEAR/ACTIVE/UNCONFIRMED. VSS 에서 재판정 금지. | Domain 기능 결정/설정/결과 owner |
| `occupantEvidence` | [`DomainIf_WarningInputEvidence_t`](12_DOMAIN_TYPES.md#type-domainif-warninginputevidence-t) | Domain 잔류 탑승자 위험의 원 근거. Pi→CIS 원 lineage·품질 보존. | Domain 기능 결정/설정/결과 owner |
| `rearObstacleState` | [`RearObstacleState_t`](11_VEHICLE_TYPES.md#type-rearobstaclestate-t) | Domain 후방 위험 상태. VSS가 거리를 다시 평가해 만들지 않는다. | Domain 기능 결정/설정/결과 owner |
| `rearEvidence` | [`DomainIf_WarningInputEvidence_t`](12_DOMAIN_TYPES.md#type-domainif-warninginputevidence-t) | 후방 위험 판단 근거와 원 관측 계보. | Domain 기능 결정/설정/결과 owner |
| `rearDetectionActivation` | [`RearDetectionActivation_t`](11_VEHICLE_TYPES.md#type-reardetectionactivation-t) | Domain 후방 감지 활성 정책. rear 위험 상태와 독립. | Domain 기능 결정/설정/결과 owner |
| `rearActivationMeta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 후방 감지 활성의 독립 평가 품질/원 AGE/순서. rear 센서 관측 메타와 별도. | Domain 기능 결정/설정/결과 owner |
| `rearActivationReason` | [`QualityReason_t`](11_VEHICLE_TYPES.md#type-qualityreason-t) | 후방 감지 활성 평가 사유. 입력 고장 및 활성 상태를 분리. | Domain 기능 결정/설정/결과 owner |

<a id="type-domainif-vssstatereport-t"></a>
## `DomainIf_VssStateReport_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    VssServiceState_t service;
    uint8_t faultMask;
    uint16_t lastFaultCode;
    OccurrenceId_t lastFaultOccurrenceId;
    uint32_t lastFaultAgeMs;
    DataQuality_t faultQuality;
    SourceGeneration_t sourceGeneration; /* derived VSS fault-warning lineage */
} DomainIf_VssStateReport_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `service` | [`VssServiceState_t`](11_VEHICLE_TYPES.md#type-vssservicestate-t) | VSS 실제 음향 서비스 상태. A2 producer는 기존 INPUT의 완전 Snapshot→NetVssStatus 단일 변환으로 선택했다. DOMAIN Net DTO↔C2/Getter association과 설치 profile은 E-S05 확인 대기이며 v0.5 service-only 수용은 전체 이력 소비가 아니다. | VSS 음향 서비스 owner |
| `faultMask` | `uint8_t` | 활성/종합 fault bitmask. 노드별 bit 계약 및 활성 code와 구분. | VSS 음향 서비스 owner |
| `lastFaultCode` | `uint16_t` | 원 ECU 최근 고장 code u16. 활성 고장 여부가 아니다. | VSS 음향 서비스 owner |
| `lastFaultOccurrenceId` | [`OccurrenceId_t`](11_VEHICLE_TYPES.md#type-occurrenceid-t) | 최근 고장 발생 식별, 코드·원 세대·AGE와 함께 소비. | VSS 음향 서비스 owner |
| `lastFaultAgeMs` | `uint32_t` | 원 최근 고장 발생 후 u32 ms. Link 마지막 수신 AGE와 다르다. | VSS 음향 서비스 owner |
| `faultQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | VSS 고장 정보 품질. 정상 오디오 상태와 별개. | VSS 음향 서비스 owner |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | VSS 음향 서비스 owner |

<a id="type-domainif-rxobserver-t"></a>
## `DomainIf_RxObserver_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_Status_t (*onMobileRequest)(
        const DomainIf_MobileRequest_t *request);

    DomainIf_Status_t (*onWarningAck)(
        const DomainIf_WarningAck_t *ack);

    DomainIf_Status_t (*onBcmStatus)(
        const DomainIf_BcmStatusReport_t *status);

    DomainIf_Status_t (*onBcmCommandResult)(
        const DomainIf_BcmCommandResult_t *result);

    DomainIf_Status_t (*onBcmProtectionEvent)(
        const DomainIf_BcmProtectionEvent_t *event);

    DomainIf_Status_t (*onCisStatus)(
        const DomainIf_CisStatusReport_t *status);

    DomainIf_Status_t (*onCisFault)(
        const FaultInfo_t *fault);

    DomainIf_Status_t (*onWindowCommandResult)(
        const DomainIf_WindowCommandResult_t *result);

    DomainIf_Status_t (*onWindowAntiPinch)(
        const DomainIf_AntiPinchReport_t *report);

    DomainIf_Status_t (*onWindowFault)(
        const FaultInfo_t *fault);

    DomainIf_Status_t (*onVssFault)(
        const FaultInfo_t *fault);
} DomainIf_RxObserver_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `onMobileRequest` | `DomainIf_Status_t (*)(const DomainIf_MobileRequest_t *request)` | 선택적/필수 책임에 따른 typed callback `onMobileRequest`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onWarningAck` | `DomainIf_Status_t (*)(const DomainIf_WarningAck_t *ack)` | 선택적/필수 책임에 따른 typed callback `onWarningAck`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onBcmStatus` | `DomainIf_Status_t (*)(const DomainIf_BcmStatusReport_t *status)` | 선택적/필수 책임에 따른 typed callback `onBcmStatus`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onBcmCommandResult` | `DomainIf_Status_t (*)(const DomainIf_BcmCommandResult_t *result)` | 선택적/필수 책임에 따른 typed callback `onBcmCommandResult`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onBcmProtectionEvent` | `DomainIf_Status_t (*)(const DomainIf_BcmProtectionEvent_t *event)` | 선택적/필수 책임에 따른 typed callback `onBcmProtectionEvent`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onCisStatus` | `DomainIf_Status_t (*)(const DomainIf_CisStatusReport_t *status)` | 선택적/필수 책임에 따른 typed callback `onCisStatus`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onCisFault` | `DomainIf_Status_t (*)(const FaultInfo_t *fault)` | 선택적/필수 책임에 따른 typed callback `onCisFault`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onWindowCommandResult` | `DomainIf_Status_t (*)(const DomainIf_WindowCommandResult_t *result)` | 선택적/필수 책임에 따른 typed callback `onWindowCommandResult`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onWindowAntiPinch` | `DomainIf_Status_t (*)(const DomainIf_AntiPinchReport_t *report)` | 선택적/필수 책임에 따른 typed callback `onWindowAntiPinch`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onWindowFault` | `DomainIf_Status_t (*)(const FaultInfo_t *fault)` | 선택적/필수 책임에 따른 typed callback `onWindowFault`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |
| `onVssFault` | `DomainIf_Status_t (*)(const FaultInfo_t *fault)` | 선택적/필수 책임에 따른 typed callback `onVssFault`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Domain 통합 owner(callback 등록); 입력 원 owner는 각 typed Data |

<a id="type-domainif-txport-t"></a>
## `DomainIf_TxPort_t`

**원 소속:** `Domain_Interface_v0.6_C2.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DomainIf_Status_t (*txBcmDoorCommand)(
        const DomainIf_BcmDoorCommand_t *command);

    DomainIf_Status_t (*txBcmClimateCommand)(
        const DomainIf_BcmClimateCommand_t *command);

    DomainIf_Status_t (*txBcmInteriorLightCommand)(
        const DomainIf_BcmInteriorLightCommand_t *command);

    DomainIf_Status_t (*txCisPowerPermission)(
        const DomainIf_CisPowerPermission_t *permission);

    DomainIf_Status_t (*txWindowCommand)(
        const DomainIf_WindowCommand_t *command);

    DomainIf_Status_t (*txWindowOperationPermission)(
        const DomainIf_WindowOperationPermission_t *permission);

    DomainIf_Status_t (*txWindowDomainAlive)(
        const DomainIf_WindowDomainAlive_t *alive);

    DomainIf_Status_t (*txVssOneShotEvent)(
        const DomainIf_VssOneShotEvent_t *event);

    DomainIf_Status_t (*txVssWarningState)(
        const DomainIf_VssWarningState_t *state);

    DomainIf_Status_t (*txMobileRequestResult)(
        const DomainIf_MobileRequestResult_t *result);

    DomainIf_Status_t (*txMobileBcmDoorState)(const DoorState_t *state);
    DomainIf_Status_t (*txMobileBcmClimateState)(const ClimateState_t *state);
    DomainIf_Status_t (*txMobileBcmInteriorLightState)(const InteriorLightState_t *state);
    DomainIf_Status_t (*txMobileBcmStatus)(const DomainIf_BcmStatusReport_t *state);
    DomainIf_Status_t (*txMobileCisEnvironment)(const CisCabinEnvironmentState_t *state);
    DomainIf_Status_t (*txMobileCisOccupant)(const CisOccupantState_t *state);
    DomainIf_Status_t (*txMobileCisRear)(const CisRearState_t *state);
    DomainIf_Status_t (*txMobileCisStatus)(const DomainIf_CisStatusReport_t *state);
    DomainIf_Status_t (*txMobileVssStatus)(const DomainIf_VssStateReport_t *state);
    DomainIf_Status_t (*txMobileWindowState)(const WindowState_t *state);
    DomainIf_Status_t (*txMobileWindowFault)(const DomainIf_WindowFaultState_t *state);

    DomainIf_Status_t (*txMobileWarning)(
        const DomainIf_WarningInfo_t *warning);

    DomainIf_Status_t (*txMobileFunctionAvailability)(
        const DomainIf_FunctionAvailabilitySet_t *availability);

    DomainIf_Status_t (*txMobileDigitalKeyState)(
        const DigitalKeyState_t *state);

    DomainIf_Status_t (*txMobileDigitalKeyResult)(
        const DomainIf_DigitalKeyResult_t *result);

    DomainIf_Status_t (*txMobileUserSettings)(
        const DomainIf_UserSettingsState_t *settings);
} DomainIf_TxPort_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `txBcmDoorCommand` | `DomainIf_Status_t (*)(const DomainIf_BcmDoorCommand_t *command)` | 선택적/필수 책임에 따른 typed callback `txBcmDoorCommand`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txBcmClimateCommand` | `DomainIf_Status_t (*)(const DomainIf_BcmClimateCommand_t *command)` | 선택적/필수 책임에 따른 typed callback `txBcmClimateCommand`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txBcmInteriorLightCommand` | `DomainIf_Status_t (*)(const DomainIf_BcmInteriorLightCommand_t *command)` | 선택적/필수 책임에 따른 typed callback `txBcmInteriorLightCommand`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txCisPowerPermission` | `DomainIf_Status_t (*)(const DomainIf_CisPowerPermission_t *permission)` | 선택적/필수 책임에 따른 typed callback `txCisPowerPermission`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txWindowCommand` | `DomainIf_Status_t (*)(const DomainIf_WindowCommand_t *command)` | 선택적/필수 책임에 따른 typed callback `txWindowCommand`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txWindowOperationPermission` | `DomainIf_Status_t (*)(const DomainIf_WindowOperationPermission_t *permission)` | 선택적/필수 책임에 따른 typed callback `txWindowOperationPermission`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txWindowDomainAlive` | `DomainIf_Status_t (*)(const DomainIf_WindowDomainAlive_t *alive)` | 선택적/필수 책임에 따른 typed callback `txWindowDomainAlive`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txVssOneShotEvent` | `DomainIf_Status_t (*)(const DomainIf_VssOneShotEvent_t *event)` | 선택적/필수 책임에 따른 typed callback `txVssOneShotEvent`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txVssWarningState` | `DomainIf_Status_t (*)(const DomainIf_VssWarningState_t *state)` | 선택적/필수 책임에 따른 typed callback `txVssWarningState`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileRequestResult` | `DomainIf_Status_t (*)(const DomainIf_MobileRequestResult_t *result)` | 선택적/필수 책임에 따른 typed callback `txMobileRequestResult`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileBcmDoorState` | `DomainIf_Status_t (*)(const DoorState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileBcmDoorState`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileBcmClimateState` | `DomainIf_Status_t (*)(const ClimateState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileBcmClimateState`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileBcmInteriorLightState` | `DomainIf_Status_t (*)(const InteriorLightState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileBcmInteriorLightState`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileBcmStatus` | `DomainIf_Status_t (*)(const DomainIf_BcmStatusReport_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileBcmStatus`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileCisEnvironment` | `DomainIf_Status_t (*)(const CisCabinEnvironmentState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileCisEnvironment`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileCisOccupant` | `DomainIf_Status_t (*)(const CisOccupantState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileCisOccupant`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileCisRear` | `DomainIf_Status_t (*)(const CisRearState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileCisRear`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileCisStatus` | `DomainIf_Status_t (*)(const DomainIf_CisStatusReport_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileCisStatus`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileVssStatus` | `DomainIf_Status_t (*)(const DomainIf_VssStateReport_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileVssStatus`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileWindowState` | `DomainIf_Status_t (*)(const WindowState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileWindowState`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileWindowFault` | `DomainIf_Status_t (*)(const DomainIf_WindowFaultState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileWindowFault`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileWarning` | `DomainIf_Status_t (*)(const DomainIf_WarningInfo_t *warning)` | 선택적/필수 책임에 따른 typed callback `txMobileWarning`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileFunctionAvailability` | `DomainIf_Status_t (*)(const DomainIf_FunctionAvailabilitySet_t *availability)` | 선택적/필수 책임에 따른 typed callback `txMobileFunctionAvailability`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileDigitalKeyState` | `DomainIf_Status_t (*)(const DigitalKeyState_t *state)` | 선택적/필수 책임에 따른 typed callback `txMobileDigitalKeyState`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileDigitalKeyResult` | `DomainIf_Status_t (*)(const DomainIf_DigitalKeyResult_t *result)` | 선택적/필수 책임에 따른 typed callback `txMobileDigitalKeyResult`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |
| `txMobileUserSettings` | `DomainIf_Status_t (*)(const DomainIf_UserSettingsState_t *settings)` | 선택적/필수 책임에 따른 typed callback `txMobileUserSettings`. 호출 동안 const 객체 borrowed, 반환은 Domain local 수용 의미. [Port/Observer 계약](20_API.md#ports) 적용. | Network Adapter 등록 owner(callback); 인자 값 원 owner는 각 typed Data |

<!-- 기존 문서 anchor 호환; 본문은 위 전체 선언과 설명으로 승계 -->
<a id="domain--domain_interface_v05_c2_targeth-전체-의미-타입"></a>
