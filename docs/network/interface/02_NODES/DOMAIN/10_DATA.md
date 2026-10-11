# DOMAIN — 공개 의미 Data와 내부 Wire 투영

**DECLARED_SOURCE / PROPOSED_DESIGN / BINDING_OPEN.** Application은 기존 C2 의미 객체를 만들고 Network는 직접 명시 Codec 투영한다. 추가 App Intent→중간 DTO→최종 Wire DTO의 세 단계 변환을 요구하지 않는다.

[개요](00_OVERVIEW.md) · [API](20_API.md) · [내부 경로](25_PRIVATE_ROUTING.md) · [공통 품질·식별·시간](../../04_COMMON/10_DATA_AND_ID_RULES.md)

## 타입 찾기

| 구현자가 다루는 의미 | 전체 선언·멤버 표 | 원 생성자 / 소비자 |
|---|---|---|
| `DomainIf_CommandContext_t` + BCM 도어/공조/실내 조명 Command | [C2 명령 타입](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t) | Domain 기능 결정→Network Codec→BCM |
| Request/Result, Door/Climate/Light, CIS 센서, VSS 서비스, WINDOW 상태 | [Vehicle 전체 타입](11_VEHICLE_TYPES.md) | MOBILE/각 ECU 의미 owner→Domain C2 |
| C2 MobileRequest union8 종·Warning·Availability·UserSettings | [Domain C2 전체 타입](12_DOMAIN_TYPES.md) | Domain 수용/설정/결과 owner |
| `DomainIf_TxPort_t` callback26 종·`DomainIf_RxObserver_t` callback11 종 | [정확한 함수포인터 선언](12_DOMAIN_TYPES.md#type-domainif-txport-t) | C2↔Network Adapter/기존 observer |
| Network Health와 correlated LocalFailure | [Health](12_DOMAIN_TYPES.md#type-networkhealthsnapshot-t), [LocalFailure](12_DOMAIN_TYPES.md#type-networklocalfailure-t) | Network supervision→기존 Domain 감독 owner |
| M_* typed Codec models·Query 응답 envelope | [Mobile Codec Data](14_MOBILE_CODEC_DATA.md) | Network-private; App 초기화 의무 아님 |
| 단일 CAN `DOMAIN_PERMISSION` | [공통 타입](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission) | Domain 원 허용 평가→BCM/CIS/VSS/WINDOW4 수신 |

전체 SOURCE struct/union/enum/alias는 두 전체 타입 문서에 선언 먼저, 멤버 타입·범위·유효성·원 owner를 다음 표에 배치했다. 단순 `Net` 접두사는 App 공개 Data 또는 packed Wire를 뜻하지 않는다.

<a id="command-context"></a>
## CommandContext — 원 ID·나이와 Network 보완 분리

```c
typedef struct
{
    DomainCommandId_t commandId;
    uint32_t decisionSequence;
    bool hasRequestContext;
    RequestContext_t requestContext;
    DeviceContextId_t requestDeviceContextId;
    CommandOrigin_t origin;
    uint32_t commandAgeMs;
    SignalMeta_t meta;
} DomainIf_CommandContext_t;
```

전체 멤버를 위 선언에 표시했다. 원 헤더 주석과 같은 타입의 상세 유효조건은 [CommandContext 정본](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t)에서 확인한다.

| 필드 | C 타입 | 단위·표현 | 생성·소비·전송 경계 |
|---|---|---|---|
| `commandId` | `DomainCommandId_t` = u32 | u32 식별자 | Domain 확정 Command owner가 한 번 생성. Network는 CAN `COMMAND_ID`로 보존; 결과 ECU echo. |
| `decisionSequence` | `uint32_t` | u32 판단 순번 | Domain 내부 판단 순서. 추가 Wire 필드 및 App Network 초기화 의무 없음. |
| `hasRequestContext` | `bool` | 참/거짓 | 정책 Command는 false 가능. 가짜 원 요청 생성 금지. |
| `requestContext` | `RequestContext_t` | Session u64 + Request u32 | 원 Session u64 + Request u32. true 일 때 Domain이 결과를 연결하는 local 관계. CAN 명령에 모든 필드를 추가하지 않음. |
| `requestDeviceContextId` | `DeviceContextId_t` = u32 | u32 단말 식별자 | 원 요청 단말. 결과 수신 현재 Session과 분리. |
| `origin` | `CommandOrigin_t` | 의미 enum | 기능 출처. CAN 코드와 enum 숫자 일대일 cast 금지. |
| `commandAgeMs` | `uint32_t` | ms, u32 → Wire u16 포화 | 명령 최초 생성 후 ms. Network가 자체 대기만 추가하고 wire u16 포화;200ms↔1 초 수용 정책 미정. |
| `meta` | `SignalMeta_t` | 품질·원 AGE(ms)·평가 순번 | 기존 판단/입력 provenance. 센서 AGE와 Command birth AGE는 서로 다른 의미. |

Network는 대상 Boot·송신 Boot·TX_SEQUENCE·CRC·길이·Endian을 생성/확인한다. TX_SEQUENCE는 송신 frame마다 증가하지만 원 Command ID, UPDATE_SEQUENCE, occurrence는 단순 재전송에서 증가하지 않는다.

<a id="snapshot-origin"></a>
## C2 snapshot ↔ UART 표시용 원본 필드

C2 Query Getter는 owner의 의미 상태를 반환하며 원 CAN header 전체를 반환하지 않는다. 11개 모바일 투영에는 [같은 accepted 원 입력에 대한 C2 Snapshot↔CAN provenance 연결](25_PRIVATE_ROUTING.md#r2-origin-join)이 필요하다. 연결이 확인되지 않은 projection만 전송 보류/부분 Query로 유지한다(PROPOSED/BINDING_OPEN). 최신 frame의 BOOT/TX_SEQUENCE를 과거 Snapshot에 붙이지 않는다.

| 경계 | 보존할 값 | 미정/금지 |
|---|---|---|
| CIS 탑승자 | Pi 원 Boot/vision_sequence/AGE + 존재·인원수 별 유효성, CIS 재평가 근거 | C2 한 meta로 원 Pi 품질과 추가 CIS 품질을 모두 표현하는 규칙 미정. 한 번 더 AGE 더하거나 새 관측을 합성하지 않음. |
| VSS 상태 | 음향 서비스 상태·원 fault code/occurrence/AGE/quality/generation | 실제 R4/R6 Snapshot→wire DTO owner/표현은 미정. Domain이 재생 DONE을 합성하지 않음. |
| WINDOW Event/Fault | END/AntiPinch 종류, occurrence, related command, 원 generation/AGE/quality | END를 AntiPinch로 압축 금지. FaultInfo_t 만으로 원 evidence 소실 금지. [구현 보류 — 설계 유지]. |
| 경고/가용성/결과 | UNCONFIRMED·UNSUPPORTED·unsupported reason·simulated | raw 표현 없으면 encode 차단/팀 결정. CLEAR/AVAILABLE/NONE의 정상 값 fallback 금지. |

C enum 원 숫자와 raw code 차이는 [명시 변환표](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)를 적용한다. source header의 `Domain_Interface_types_v0.2_draft.h`는 이전 대체 타입 참조이며 현 C2와 중복으로 include 하거나 live 배포 헤더로 교체하지 않는다. 제공 원 `.h`는 정확하게 보존했으며 include 이름/경로의 실제 Firmware resolution은 별도 binding 이다.
