# CIS Requirements & Interface Package

CIS(Cabin Interior Sensing)의 SR, SysRS, logical interface, trace, ECU interface 문서를 관리한다.

## 문서 구성

| 순서 | 문서 | 역할 |
|---|---|---|
| 01 | `01_CIS_SR_FUNCTIONAL_BASELINE_DRAFT.md` | 상위 시스템 관점의 기능 요구사항 |
| 02 | `02_CIS_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md` | CIS ECU 시스템 요구사항 |
| 03 | `03_CIS_LOGICAL_INTERFACE_NEEDS_DRAFT.md` | 중앙 제어와 교환할 정보의 의미와 방향 |
| 04 | `04_CIS_SR_SYSRS_TRACE_NAVIGATION_DRAFT.md` | SR ↔ SysRS 추적 navigation |
| 05 | `05_CIS_ECU_INTERFACE_INFO_DRAFT.md` | `State / Data / Fault / Quality / Local Logic / Fail-safe` 기준 ECU Interface 정보 |

## 현재 기준

- CIS는 중앙 제어에만 탑승자·환경·후방 거리 정보를 제공한다.
- 후방 거리는 유효성·원본 시점/경과 시간·갱신 정보와 함께 제공한다.
- 후방 위험 수준의 판단과 VSS 전달은 중앙 제어가 담당한다.
- `NO_OBJECT`, `INVALID`, `NO_DATA`, `STALE`, 통신 미수신을 동일하게 취급하지 않는다.
- 전체 ECU 상태와 기능별 상태·오류·복구를 분리한다.
- 한 센서 또는 비전 기능의 오류가 독립된 다른 기능을 중단시키지 않는다.
- CAN ID, payload, cycle, timeout, CRC/E2E는 semantic interface freeze 이후 Network 단계에서 확정한다.

## 현재 상태

**Review Candidate / Central·Network·HW Cross-check 전**

특히 차량 전원 허용의 source와 품질, 선택 구성의 후진 연계, 거리/인원수의 범위, value age·sequence 표현, partial fault availability, network timing을 후속 단계에서 확정한다.
