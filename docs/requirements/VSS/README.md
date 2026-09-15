# VSS Requirements & Interface Package

VSS(Virtual Sound System)의 SR, SysRS, Logical Interface, Trace, ECU Interface 문서를 관리한다.

현재 문서 세트는 **VSS 단독 설계 기준을 정리한 Review Candidate**이며, Power Window / Door / Central / Sensing 담당자와의 Cross-check 전 단계이다.

## 문서 구성

| 순서 | 문서 | 역할 |
|---|---|---|
| 01 | `01_VSS_SR_FUNCTIONAL_BASELINE_DRAFT.md` | 상위 시스템/고객 관점의 기능 요구사항(What) |
| 02 | `02_VSS_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md` | VSS ECU가 만족해야 할 시스템 요구사항 |
| 03 | `03_VSS_LOGICAL_INTERFACE_NEEDS_DRAFT.md` | VSS와 외부 ECU 사이에서 교환해야 할 정보의 의미와 방향 |
| 04 | `04_VSS_SR_SYSRS_TRACE_NAVIGATION_DRAFT.md` | SR ↔ SysRS 양방향 추적 및 Navigation |
| 05 | `05_VSS_ECU_INTERFACE_INFO_DRAFT.md` | `State / Request / Event / Fault / Data / Local Logic / Fail-safe` 기준 ECU Interface 정보 정리 |

## 현재 기준

- One-shot Event와 Stateful Information을 분리한다.
- Stateful 입력은 최신 의미 상태와 Reception Quality를 분리해서 관리한다.
- `CLEAR`와 `STALE / INVALID / NOT_RECEIVED / SNA`를 동일하게 취급하지 않는다.
- VSS 내부 Fault와 Input/Interface Diagnostic을 분리한다.
- `Emergency > Warning > Feedback` 우선순위를 적용한다.
- 동일 Priority Class 내부는 deterministic tie-break 정책이 필요하다.
- 선점된 One-shot은 자동 Resume하지 않고, Stateful Warning은 계속 유효하면 재중재 후 다시 활성화할 수 있다.
- CAN ID / DLC / Start Bit / CRC/E2E / 최종 Cycle·Timeout은 Interface 의미 Freeze 이후 Network 단계에서 확정한다.

## 현재 상태

**Review Candidate / Interface Freeze 전**

VSS 단독 문서의 주요 구조와 Trace는 정리되었으며, 다음 항목은 상대 ECU와 Cross-check 후 확정한다.

- Vehicle Welcome / Goodbye의 Semantic Owner 및 Network Producer
- Door Lock Complete / Unlock / Error의 의미 범위와 Producer
- Anti-Pinch `ACTIVE / CLEAR / SNA` 판정 및 전달 조건
- Occupant Hazard 의미 경계
- Rear Obstacle `CLEAR / CAUTION / EMERGENCY`의 최종 판단 Owner
- Stateful 입력의 Freshness / Timeout / Fail-safe 동작
- Startup / Wake One-shot Event Delivery 방식
- One-shot Max Age 및 occurrence identity 규칙
- `VSS_ACCEPTING_EVENTS`, `VSS_AVAILABILITY`의 실제 Consumer 필요 여부

## 범위 미확정 항목

다음 항목은 현재 Functional Baseline에 확정 포함하지 않는다.

- **Mute 정책**: 적용 여부 `TBD`
- **음향 규제/법규 적용 범위**: 프로젝트 대상 VSS 기능과의 적용/비적용 범위 `TBD`
- 실제 Speaker 위치 / 평가 위치 / 최종 음압(dBA) 기준

해당 항목이 필요해지는 경우 SR/SysRS Change Request를 통해 요구사항으로 추가한다.


## Deferred Security Extension

VSS와 Central/Domain Controller 간 통신 보안은 **현재 Functional Baseline의 필수 구현 범위에 포함하지 않는다.**

현재 단계에서는 VSS 기능, Semantic Interface 및 기본 차량 네트워크 통신의 안정적인 구현과 검증을 우선한다. 기본 네트워크와 VSS 기능이 안정화되고 프로젝트 일정·CPU/RAM·통신 부하 측면에서 추가 구현을 감당할 수 있다고 판단되는 경우 다음 보안 기능을 후속 확장으로 검토한다.

- 공개키 기반 ECU 인증
- 비대칭키 기반 Key Establishment / Key Exchange
- 세션 키 생성 및 관리
- 세션 키 기반 메시지 기밀성·무결성·인증 보호
- Replay Protection 및 보안 메타데이터 관리
- 인증/키 설정/메시지 검증 실패 시 오류 처리
- 필요 시 학습·데모 목적의 비대칭키 암호화/복호화 검증

```text
Security Status
= DEFERRED / FUTURE EXTENSION

Current Scope
= Functional communication baseline

Future Scope
= Public-key based authentication / key establishment
  + session-key based message protection
```

> 위 항목은 현재 SR/SysRS Baseline 요구사항이 아니다. 실제 구현을 결정하는 시점에 별도 Security Requirement와 Interface/Network 변경 사항으로 승격하고 Trace/Test를 추가한다.

## 다음 단계

```text
VSS 문서 Review Candidate
    ↓
Power Window / Door / Central / Sensing Cross-check
    ↓
Semantic Owner / CLEAR / SNA / Wake·Delivery 계약 확정
    ↓
Interface Freeze Candidate
    ↓
Network Mapping (CAN ID / DLC / Bit Layout / Cycle / Timeout / CRC/E2E)
    ↓
기본 VSS/Network 안정화 및 일정·자원 여유 확인
    ↓
Security Extension 검토 (DEFERRED)
```
